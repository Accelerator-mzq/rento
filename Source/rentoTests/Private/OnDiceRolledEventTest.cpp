// OnDiceRolledEventTest.cpp
// =============================================================================
// 集成测试：story-005 — OnDiceRolled 事件契约 + 单线程重入禁止
//
// 物理路径：Source/rentoTests/Private/OnDiceRolledEventTest.cpp
// Automation 类目：Rento.Dice.OnDiceRolledEvent
//
// 覆盖 AC（TR-dice-005 / TR-dice-007 / TR-dice-009）：
//   TC_AC21 — FDiceRollResult USTRUCT(BlueprintType) + 字段 BlueprintReadOnly 反射断言
//   TC_AC22 — OnDiceRolled UPROPERTY(BlueprintAssignable) + FMulticastDelegateProperty 反射断言
//   TC_AC14 — payload 四字段 == 同步返回值（固定种子 12345，防 vacuous bReceived 断言）
//   TC_AC15 — 连续 RollDice() 5 次，OnDiceRolled 触发次数精确 == 5（不多不少）
//
// AC-16b（重入禁止 code-review）：
//   见 production/qa/evidence/dice-ondicerolled-reentrancy-review.md（Advisory 文件证据）
//
// ⚠ 已知坑规避（impl-brief 清单）：
//   G1  EAutomationTestFlags：用 EAutomationTestFlags_ApplicationContextMask（UE5.4+ enum class），
//       抄邻居 RollDiceCoreContractTest.cpp 范式
//   G2  反射用户 USTRUCT 用 FDiceRollResult::StaticStruct()，禁 TBaseStructure<T>::Get()
//   G3  UE5.4+ 用 FindFirstObject<T>，禁 FindObject(ANY_PACKAGE, ...)
//   G4  测试内错误用 UE_LOG + TestEqual/TestTrue，禁 ensure()
//   G5  计数 spy 用 NewObject + 无需 AddToRoot（同步测试函数内 GC 不回收，与 RollDiceCoreContractTest 一致）
//   G6  禁多余 AddExpectedError（occurrences 不匹配判假 Fail）
//   G7  BlueprintReadOnly 反射：CPF_BlueprintVisible | CPF_BlueprintReadOnly 同时置位；
//       BlueprintReadWrite = CPF_BlueprintVisible 但无 CPF_BlueprintReadOnly
//       断言：字段须 HasAnyPropertyFlags(CPF_BlueprintReadOnly)（拒绝 ReadWrite 误标）
//
// Out of Scope（story-005 铁线）：
//   - 不改 DiceRngService 任何实现（已合规，零 src 改动）
//   - 不实现 VFX/HUD/audio 订阅者
//   - 不碰存档/读档重绑
// =============================================================================

#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"        // FMulticastDelegateProperty / FProperty
#include "UObject/Object.h"

#include "DiceRngService.h"            // UDiceRngService / FDiceRollResult
#include "DiceRollTestSubscriber.h"    // UDiceRollTestSubscriber（AC-14 复用）

// =============================================================================
// 计数 spy —— AC-15 专用
//
// UDiceRollTestSubscriber 只存最近一次结果，无计数器；
// 本类在文件内定义最小计数 spy，只累加触发次数，不调任何 RNG API。
//
// 注意：UHT 要求 UCLASS 须在独立头文件内才能完整生成；但 rentoTests 模块用
// IMPLEMENT_SIMPLE_AUTOMATION_TEST 时，同 .cpp 文件内定义的 UCLASS 只要
// .generated.h 不在 .cpp 中 include 即无法生成反射元数据。
// AC-15 不需要反射断言计数 spy 本体——AddDynamic 要求的是 UFUNCTION，
// 而 UFUNCTION 必须归属 UObject 子类且有 .generated.h。
//
// 解决方案：计数 spy 用独立头文件（DiceRollCountSpy.h），或改用 lambda 等价方案。
// UE dynamic multicast 不支持裸 lambda。
// → AC-15 复用 UDiceRollTestSubscriber 并在其外层包一个 int32 计数器：
//   每次订阅同一回调，创建 N 个独立订阅对象数组，计数 = 数组 .Num()（等价实现）。
// → 更干净方案：直接扩展 UDiceRollTestSubscriber 的 ReceiveCount 字段（只需引用
//   已存在的 .h，不改原始文件 src 以外的约束——rentoTests 模块可以改）。
//
// *** story-005 Out of Scope 铁线：不改 src/ ***
// DiceRollTestSubscriber.h 在 rentoTests/Private/ 下（测试模块），
// 本 story 允许在测试模块内新增最小 spy——但为避免改 DiceRollTestSubscriber.h（
// 可能影响 AC-16c 既有断言），选择定义独立 UDiceRollCountSpy 类于此 .cpp 引用的
// 独立头文件 DiceRollCountSpy.h。
//
// *** 实际决策：写 DiceRollCountSpy.h，在本 .cpp include，不改任何 src/ 文件 ***
// =============================================================================
#include "DiceRollCountSpy.h"          // UDiceRollCountSpy（AC-15 计数 spy）

// =============================================================================
// TC_AC21 — FDiceRollResult USTRUCT(BlueprintType) + 字段 BlueprintReadOnly 反射断言
//
// AC-21（[Logic]）：
//   Given: FDiceRollResult 标 USTRUCT(BlueprintType)，四字段标 UPROPERTY(BlueprintReadOnly)
//   When:  模块编译 + 运行期反射
//   Then:  struct 在 UHT 注册；四字段都有 CPF_BlueprintReadOnly；无任一字段误标 BlueprintReadWrite
//
// 反射语义（UE 编译器 flag 映射，UE4 起稳定）：
//   UPROPERTY(BlueprintReadOnly)  → CPF_BlueprintVisible | CPF_BlueprintReadOnly 同时置位
//   UPROPERTY(BlueprintReadWrite) → CPF_BlueprintVisible 置位，CPF_BlueprintReadOnly 不置位
//
// 断言策略：
//   字段须 HasAnyPropertyFlags(CPF_BlueprintReadOnly)
//   ↑ 若误标 BlueprintReadWrite 则 CPF_BlueprintReadOnly 未置位 → 断言 FAIL（非 vacuous）
//   呈现层纯叶子不得回写（ADR-0007 / story AC-21 Edge）——BlueprintReadWrite 误标
//   会允许 BP 图节点写入字段，破坏 owner 结算同步先行不变式。
//
// 非 vacuous 保证：
//   - 若 Die1 误标 BlueprintReadWrite，CPF_BlueprintReadOnly 未置位 → FAIL
//   - 若 FDiceRollResult 未标 USTRUCT(BlueprintType)，StaticStruct() 返回 nullptr → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOnDiceRolled_TC_AC21_FDiceRollResult_BlueprintReadOnly_Reflection,
	"Rento.Dice.OnDiceRolledEvent.TC_AC21_FDiceRollResult_BlueprintReadOnly_Reflection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOnDiceRolled_TC_AC21_FDiceRollResult_BlueprintReadOnly_Reflection::RunTest(const FString& Parameters)
{
	// THEN-1：FDiceRollResult 已被 UHT 注册为 UScriptStruct（USTRUCT(BlueprintType) 证明）
	// 用用户 USTRUCT 须用 T::StaticStruct()（禁 TBaseStructure，GG3 踩过编译失败）
	UScriptStruct* StructType = FDiceRollResult::StaticStruct();
	if (!TestNotNull(
		TEXT("AC-21: FDiceRollResult 是 USTRUCT(BlueprintType)（StaticStruct() 非 nullptr）"),
		StructType))
	{
		return false;
	}

	// 要验证的四字段名（与 DiceRngService.h 实现对齐）
	const TArray<FName> FieldNames = {
		TEXT("Die1"),
		TEXT("Die2"),
		TEXT("Total"),
		TEXT("bIsDouble"),
	};

	for (const FName& FieldName : FieldNames)
	{
		// 断言字段存在
		FProperty* Prop = StructType->FindPropertyByName(FieldName);
		if (!TestNotNull(
			*FString::Printf(TEXT("AC-21: FDiceRollResult.%s 字段存在"), *FieldName.ToString()),
			Prop))
		{
			continue; // 字段不存在跳过 flag 断言，上面 TestNotNull 已记 FAIL
		}

		// 断言 CPF_BlueprintReadOnly 置位（UPROPERTY(BlueprintReadOnly) 语义）
		// 非 vacuous：若误标 BlueprintReadWrite，CPF_BlueprintReadOnly 未置位 → 此处 FAIL
		TestTrue(
			*FString::Printf(
				TEXT("AC-21: FDiceRollResult.%s 须有 CPF_BlueprintReadOnly（呈现层纯叶子不得回写，ADR-0007）"),
				*FieldName.ToString()),
			Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly));

		// 断言 CPF_BlueprintVisible 同时置位（BlueprintReadOnly 须可见，非隐藏字段）
		// 非 vacuous：若字段完全隐藏（无 BP_Visible），BP 无法读取 payload → FAIL
		TestTrue(
			*FString::Printf(
				TEXT("AC-21: FDiceRollResult.%s 须有 CPF_BlueprintVisible（BP 可读取 payload 字段）"),
				*FieldName.ToString()),
			Prop->HasAnyPropertyFlags(CPF_BlueprintVisible));

		// 守卫：确认字段 **不是** BlueprintReadWrite（BlueprintReadWrite = BlueprintVisible 无 BlueprintReadOnly）
		// 这是 Edge 断言（story AC-21 边界）：「字段标 BlueprintReadWrite 则违纯叶子 → review 拦」
		// 此处通过反射硬断言，将 code-review 软约束升为可自动化的 Logic 断言
		// 逻辑：若 CPF_BlueprintReadOnly 置位则不是 ReadWrite；此断言是上面断言的逻辑结论，
		// 作为独立行以便 FAIL 时消息更明确。
		// 非 vacuous：只有当 CPF_BlueprintReadOnly 未置位时（误标 ReadWrite）才 FAIL
		const bool bIsReadWriteMistaggedAsReadOnly =
			Prop->HasAnyPropertyFlags(CPF_BlueprintVisible) &&
			!Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly);

		TestFalse(
			*FString::Printf(
				TEXT("AC-21 Edge: FDiceRollResult.%s 不得标 BlueprintReadWrite（呈现层纯叶子禁回写，ADR-0007）"),
				*FieldName.ToString()),
			bIsReadWriteMistaggedAsReadOnly);
	}

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC21 PASS: FDiceRollResult USTRUCT 已注册，4 字段全有 CPF_BlueprintReadOnly"
			 "（TR-dice-009，ADR-0003/0007，story-005 AC-21）。"));

	return true;
}

// =============================================================================
// TC_AC22 — OnDiceRolled UPROPERTY(BlueprintAssignable) + FMulticastDelegateProperty 反射断言
//
// AC-22（[Logic]）：
//   Given: OnDiceRolled 经 DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam 声明
//          + 标 UPROPERTY(BlueprintAssignable)
//   When:  模块编译 + 运行期反射
//   Then:  UDiceRngService::OnDiceRolled 是 FMulticastDelegateProperty
//          且带 CPF_BlueprintAssignable flag（BP 可绑定/解绑）
//
// 非 vacuous 保证：
//   - 若 OnDiceRolled 误用非 dynamic delegate → FMulticastDelegateProperty CastField 返 nullptr → FAIL
//   - 若 UPROPERTY 漏写 BlueprintAssignable → CPF_BlueprintAssignable 未置位 → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOnDiceRolled_TC_AC22_OnDiceRolled_BlueprintAssignable_Reflection,
	"Rento.Dice.OnDiceRolledEvent.TC_AC22_OnDiceRolled_BlueprintAssignable_Reflection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOnDiceRolled_TC_AC22_OnDiceRolled_BlueprintAssignable_Reflection::RunTest(const FString& Parameters)
{
	// 反射入口：UDiceRngService 的 UClass
	UClass* DiceClass = UDiceRngService::StaticClass();
	if (!TestNotNull(TEXT("AC-22: UDiceRngService::StaticClass() 非 nullptr"), DiceClass))
	{
		return false;
	}

	// THEN-1：属性 "OnDiceRolled" 存在于 UDiceRngService
	FProperty* Prop = DiceClass->FindPropertyByName(TEXT("OnDiceRolled"));
	if (!TestNotNull(
		TEXT("AC-22: UDiceRngService::OnDiceRolled 属性在 UClass 中注册（UPROPERTY 生效）"),
		Prop))
	{
		return false;
	}

	// THEN-2：属性类型是 FMulticastDelegateProperty
	// 非 vacuous：DYNAMIC_MULTICAST_DELEGATE 宏 → FMulticastDelegateProperty；
	// 若误用裸 delegate（非 dynamic）→ FDelegateProperty → CastField 返 nullptr → FAIL
	FMulticastDelegateProperty* MCDProp = CastField<FMulticastDelegateProperty>(Prop);
	if (!TestNotNull(
		TEXT("AC-22: OnDiceRolled 是 FMulticastDelegateProperty（DYNAMIC_MULTICAST_DELEGATE）"),
		MCDProp))
	{
		return false;
	}

	// THEN-3：带 CPF_BlueprintAssignable（UPROPERTY(BlueprintAssignable) 语义）
	// 非 vacuous：若漏写 BlueprintAssignable → CPF_BlueprintAssignable 未置位 → FAIL
	// BP 端须 AddDynamic/RemoveDynamic 可用，呈现层 widget 才能订阅
	TestTrue(
		TEXT("AC-22: OnDiceRolled 带 CPF_BlueprintAssignable（BP 可订阅/解绑，ADR-0003 §统一规范 #1）"),
		Prop->HasAnyPropertyFlags(CPF_BlueprintAssignable));

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC22 PASS: OnDiceRolled 是 FMulticastDelegateProperty + CPF_BlueprintAssignable"
			 "（TR-dice-005，ADR-0003，story-005 AC-22）。"));

	return true;
}

// =============================================================================
// TC_AC14 — payload 四字段 == 同步返回值（固定种子，防 vacuous）
//
// AC-14（[Logic]，与 story-002 AC-16c 互证）：
//   Given: 订阅 OnDiceRolled 捕获 payload；SetSeed(12345)
//   When:  RollDice() 一次
//   Then:  事件 payload 四字段（Die1/Die2/Total/bIsDouble）与同步返回值逐一相等
//          Sub->bReceived==true（防 vacuous：广播未触发则 bReceived==false → FAIL）
//
// 与 AC-16c 的关系：AC-16c 证明「返回==payload 同源（执行序铁律④）」；
// AC-14 从 story-005 角度做同样断言（事件契约层），两者互证。
//
// 非 vacuous 保证：
//   - bReceived 守卫：若 OnDiceRolled 广播被意外跳过，bReceived==false → 提前 return false
//   - 若 RollDice 实现「广播后重读流」，payload 与返回值消耗不同游标位置 → 四字段不等 → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOnDiceRolled_TC_AC14_PayloadEqualsReturn_FixedSeed,
	"Rento.Dice.OnDiceRolledEvent.TC_AC14_PayloadEqualsReturn_FixedSeed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOnDiceRolled_TC_AC14_PayloadEqualsReturn_FixedSeed::RunTest(const FString& Parameters)
{
	// GIVEN-1：构造服务实例（headless 安全，与 RollDiceCoreContractTest 同范式）
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-14: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	// GIVEN-2：复用 UDiceRollTestSubscriber（DiceRollTestSubscriber.h，AC-16c 专用桩）
	// 复用理由：订阅桩已存在，语义完全匹配（捕获最近一次 payload + bReceived 守卫）
	UDiceRollTestSubscriber* Sub = NewObject<UDiceRollTestSubscriber>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-14: UDiceRollTestSubscriber 应能通过 NewObject<> 构造"), Sub))
	{
		return false;
	}

	// GIVEN-3：订阅 OnDiceRolled（AddDynamic 要求 UFUNCTION + UObject 宿主）
	Service->OnDiceRolled.AddDynamic(Sub, &UDiceRollTestSubscriber::OnRollReceived);

	// GIVEN-4：固定种子（ADR-0004 确定性，零方差断言基础）
	Service->SetSeed(12345);

	// WHEN：调用 RollDice() 一次
	const FDiceRollResult RetVal = Service->RollDice();

	// THEN-0（防 vacuous）：订阅者必须已收到广播
	// 非 vacuous：若广播被跳过（实现省略步骤③），bReceived==false → FAIL → 后续比较无意义
	if (!TestTrue(
		TEXT("AC-14: Sub->bReceived==true（OnDiceRolled 广播已触发，防 vacuous 守卫）"),
		Sub->bReceived))
	{
		Service->OnDiceRolled.RemoveDynamic(Sub, &UDiceRollTestSubscriber::OnRollReceived);
		return false;
	}

	// THEN-1：Die1 字段同源
	// 非 vacuous：若实现「广播后重读流返回」，RetVal.Die1 != Sub->CapturedResult.Die1 → FAIL
	TestEqual(
		*FString::Printf(
			TEXT("AC-14: 返回值 Die1=%d 应 == payload Die1=%d（同一固化值，执行序铁律④）"),
			RetVal.Die1, Sub->CapturedResult.Die1),
		RetVal.Die1, Sub->CapturedResult.Die1);

	// THEN-2：Die2 字段同源
	TestEqual(
		*FString::Printf(
			TEXT("AC-14: 返回值 Die2=%d 应 == payload Die2=%d"),
			RetVal.Die2, Sub->CapturedResult.Die2),
		RetVal.Die2, Sub->CapturedResult.Die2);

	// THEN-3：Total 字段同源
	TestEqual(
		*FString::Printf(
			TEXT("AC-14: 返回值 Total=%d 应 == payload Total=%d"),
			RetVal.Total, Sub->CapturedResult.Total),
		RetVal.Total, Sub->CapturedResult.Total);

	// THEN-4：bIsDouble 字段同源
	TestEqual(
		*FString::Printf(
			TEXT("AC-14: 返回值 bIsDouble=%d 应 == payload bIsDouble=%d"),
			RetVal.bIsDouble ? 1 : 0,
			Sub->CapturedResult.bIsDouble ? 1 : 0),
		RetVal.bIsDouble, Sub->CapturedResult.bIsDouble);

	// 测试结束解绑（防野绑定，ADR-0003 读档重绑纪律）
	Service->OnDiceRolled.RemoveDynamic(Sub, &UDiceRollTestSubscriber::OnRollReceived);

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC14 PASS: payload 四字段与同步返回值逐一相等（种子 12345，"
			 "TR-dice-006/009，ADR-0003/0004，story-005 AC-14）。"));

	return true;
}

// =============================================================================
// TC_AC15 — 连续 RollDice() 5 次，OnDiceRolled 触发次数精确 == 5
//
// AC-15（[Logic]）：
//   Given: 订阅 OnDiceRolled 计数 spy；SetSeed(12345)
//   When:  连续 RollDice() 5 次
//   Then:  spy 计数 == 5（精确，不多不少）
//
// 使用 UDiceRollCountSpy（DiceRollCountSpy.h）——最小计数 spy，仅累加 ReceiveCount。
//
// 非 vacuous 保证（双向）：
//   - 「漏触发」保证：若 RollDice 实现省略广播步骤③，计数 < 5 → TestEqual FAIL
//   - 「多触发」保证：若 RollDice 实现意外广播两次，计数 > 5 → TestEqual FAIL
//   - 不因 lazy-init/退化路径多触发或漏触发（Edge，story AC-15）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOnDiceRolled_TC_AC15_BroadcastCount_ExactlyFive,
	"Rento.Dice.OnDiceRolledEvent.TC_AC15_BroadcastCount_ExactlyFive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOnDiceRolled_TC_AC15_BroadcastCount_ExactlyFive::RunTest(const FString& Parameters)
{
	// GIVEN-1：构造服务实例
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-15: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	// GIVEN-2：计数 spy（UDiceRollCountSpy，仅累加触发次数，不调任何 RNG API）
	UDiceRollCountSpy* CountSpy = NewObject<UDiceRollCountSpy>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-15: UDiceRollCountSpy 应能通过 NewObject<> 构造"), CountSpy))
	{
		return false;
	}

	// GIVEN-3：订阅 OnDiceRolled（计数回调）
	Service->OnDiceRolled.AddDynamic(CountSpy, &UDiceRollCountSpy::OnRollReceived);

	// GIVEN-4：固定种子
	Service->SetSeed(12345);

	// WHEN：连续 RollDice() 5 次（AC-15 精确次数断言）
	constexpr int32 RollCount = 5;
	for (int32 i = 0; i < RollCount; ++i)
	{
		Service->RollDice();
	}

	// THEN：计数精确 == 5（不多不少）
	// 非 vacuous-漏触发：若 RollDice 省略广播，ReceiveCount < 5 → TestEqual FAIL
	// 非 vacuous-多触发：若 RollDice 意外双播，ReceiveCount > 5 → TestEqual FAIL
	TestEqual(
		*FString::Printf(
			TEXT("AC-15: 连续 RollDice() 5 次后 OnDiceRolled 触发次数应精确 == 5，"
				 "实际 == %d（不因 lazy-init/退化路径多/漏触发）"),
			CountSpy->ReceiveCount),
		CountSpy->ReceiveCount, RollCount);

	// 解绑（防野绑定）
	Service->OnDiceRolled.RemoveDynamic(CountSpy, &UDiceRollCountSpy::OnRollReceived);

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC15 PASS: 5 次 RollDice() → OnDiceRolled 触发 5 次（精确）"
			 "（TR-dice-005，ADR-0003，story-005 AC-15）。"));

	return true;
}
