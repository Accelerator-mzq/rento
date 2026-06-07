// DiceRngService.h
// =============================================================================
// RNG 服务宿主（ADR-0004 §Decision + §Key Interfaces + ADR-0001 §1）
//
// 实现内容（story-001 scope）：
//   - 权威 FRandomStream 实例成员（值语义，private，全项目恰一个权威流）
//   - 4 个 BP-callable 原语封装：RandomRange / RandomFloat01 / SetSeed / GetCurrentSeed
//   - Seed 注入唯一时机 = OnWorldBeginPlay（ADR-0001 / control-manifest Forbidden §Seed 注入）
//   - bUseFixedSeed / FixedSeed 测试旋钮（调试/测试用，生产路径不触发）
//   - ShouldCreateSubsystem 通过继承 UMatchSubsystemBase 排除 editor-preview World
//
// 实现内容（story-002 scope，追加，不改 story-001 既有语义）：
//   - FDiceRollResult USTRUCT（已实现，story-002）
//   - RollDice() 执行序铁律：流抽→固化→广播→返回同一固化值
//   - OnDiceRolled 最小可广播 seam（story-002 用于执行序铁律③广播 + AC-16c 验
//     返回==payload 同源）；触发次数不变式 / 重入禁止 / 完整 BP 暴露纪律仍
//     → story-005，story-005 在此既有 delegate 上加语义，不重新声明
//
// Out of Scope（严守各 story 边界）：
//   - RandomRange 退化契约（Min==Max/Min>Max/2^24） → story-003
//   - lazy-init 兜底种子（!bUseFixedSeed 路径）    → story-004
//   - OnDiceRolled 触发次数不变式 / 重入禁止 / 完整 BP 暴露纪律 → story-005
//   - 确定性序列 fixture（AC-8/9）                 → story-006
//   - CI 禁全局 RNG 静态扫描 / 流隔离 spy          → story-007
//   - Seed 序列化                                   → story-008
//
// 规范依据：
//   - ADR-0004（primary）— 确定性 RNG 服务 + 执行序铁律
//   - ADR-0001            — UObject 宿主与生命周期边界
//   - ADR-0003            — 事件总线：OnDiceRolled payload 形态
//   - ADR-0007            — BP/C++ 边界：含 gameplay 随机 → C++ UFUNCTION
//   - control-manifest Foundation Layer §确定性 RNG (ADR-0004)
//   - story-001（dice-rng epic）AC-23/18/13/12c + [Host] AC
//   - story-002（dice-rng epic）AC-1/2/6/7/16c
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "MatchSubsystemBase.h"
#include "Math/RandomStream.h"
#include "DiceRngService.generated.h"

// =============================================================================
// FDiceRollResult — 掷骰结果 payload（story-002；ADR-0004 §Key Interfaces）
//
// 构造时不变式（本系统产出时恒满足，调用方不应假设外部构造保持此不变式）：
//   Total == Die1 + Die2
//   bIsDouble == (Die1 == Die2)
//
// 字段均为 BlueprintReadOnly（ADR-0003：多字段 payload 必包 USTRUCT(BlueprintType)；
// BD-001 W-1 logged decision：呈现层需 BP 只读访问，可接受放宽为 ReadOnly）。
//
// 「两颗独立 d6」说明（dice GDD F-1 / CR-1）：
//   Die1 和 Die2 由两次独立顺序抽取产生（各推进流游标一次），
//   Total 由加法派生，而非「先掷 Total 再拆解」。
//   三角分布（2/12 各 1/36，7 为 6/36）由两独立骰自然产生。
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FDiceRollResult
{
	GENERATED_BODY()

	/** 第一颗骰子点数，∈[1,6]（由权威流第一次 RandRange(1,6) 产生） */
	UPROPERTY(BlueprintReadOnly, Category="Rento|RNG")
	int32 Die1 = 0;

	/** 第二颗骰子点数，∈[1,6]（由权威流第二次 RandRange(1,6) 产生） */
	UPROPERTY(BlueprintReadOnly, Category="Rento|RNG")
	int32 Die2 = 0;

	/** 两骰合计，构造时不变式：Total == Die1 + Die2 */
	UPROPERTY(BlueprintReadOnly, Category="Rento|RNG")
	int32 Total = 0;

	/** 是否双点，构造时不变式：bIsDouble == (Die1 == Die2) */
	UPROPERTY(BlueprintReadOnly, Category="Rento|RNG")
	bool bIsDouble = false;
};

// =============================================================================
// FOnDiceRolled delegate 声明（story-002 最小 seam）
//
// 本 story 仅声明最小可广播 seam，供执行序铁律③广播 + AC-16c 验返回==payload 同源。
// 事件机制完整语义（触发次数不变式、重入禁止、完整 BP 暴露纪律）归 story-005，
// story-005 在此既有 delegate 上加语义，不重新声明。
// =============================================================================
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDiceRolled, FDiceRollResult, Result);

/**
 * UDiceRngService — 权威 RNG 服务，per-match UWorldSubsystem 宿主
 *
 * 持有全项目唯一的权威 FRandomStream（dice 拥有，ADR-0004 §Decision）。
 * 所有需要可重放的 gameplay 随机（骰点 / 牌堆洗牌 / AI 决策扰动）
 * 必须经本服务的封装函数取随机，禁止旁路引擎全局 RNG（FMath::Rand 等）。
 *
 * 生命周期：
 *   - Initialize       — 宿主就绪，此时禁止注入 Seed（玩家配置未落地，ADR-0001 Forbidden）
 *   - OnWorldBeginPlay — 唯一合法 Seed 注入时机（ADR-0004 / control-manifest）
 *   - Deinitialize     — World 销毁，subsystem 随之销毁，FRandomStream 自动析构
 *
 * 权威流访问纪律（ADR-0004 划线表）：
 *   - 骰点 RollDice / 牌堆 Fisher-Yates / AI 决策扰动 → 必须经本服务
 *   - VFX / Audio / HUD juice 随机 → 各 owner 持独立非权威流（禁复用本流）
 *
 * OQ 登记（本 story 未实现，待后续 story）：
 *   - OQ-2：GetCurrentSeed() 显式存取，勿靠 struct 反射（已实现显式 accessor）
 *   - OQ-4：RandomFloat01() 半开 [0,1)，floor(f*N) 仅 MVP 小 N 安全；N>=2^24 浮点舍入越界未测
 */
UCLASS()
class RENTO_API UDiceRngService : public UMatchSubsystemBase
{
	GENERATED_BODY()

public:
	// -------------------------------------------------------------------------
	// story-002：RollDice + OnDiceRolled seam
	// -------------------------------------------------------------------------

	/**
	 * 掷出标准双骰（两颗独立 d6），返回完整结果（ADR-0004 执行序铁律，dice F-1/CR-1）。
	 *
	 * 执行序铁律（ADR-0004 §Key Interfaces，dice F-4 前提②，control-manifest Required）：
	 *   ① 流抽：Die1 = RandomRange(1,6)；Die2 = RandomRange(1,6)
	 *      （两次独立顺序抽取，各推进权威流游标一次；禁旁路 FMath::Rand）
	 *   ② 本地固化：构造 FDiceRollResult（Die1/Die2/Total=Die1+Die2/bIsDouble=(Die1==Die2)）
	 *   ③ 广播：OnDiceRolled.Broadcast(Result)
	 *   ④ 返回同一固化 Result（绝不广播后重读流——重读多消耗 Seed，导致返回值与 payload 不等）
	 *
	 * 保证：同步返回值 == 事件 payload（TR-dice-006，AC-16c）；
	 *       Total==Die1+Die2；bIsDouble==(Die1==Die2)。
	 *
	 * 「两颗独立 d6，非先取 Total 再拆解」（dice CR-1）：
	 *   三角分布由两独立骰自然产生（2/12 各 1/36，7 为 6/36），忠实经典大富翁。
	 *
	 * @return 本次掷骰的固化结果，与 OnDiceRolled 广播的 payload 同源
	 */
	UFUNCTION(BlueprintCallable, Category="Rento|RNG",
		meta=(DisplayName="Roll Dice", ToolTip="掷出标准双骰，返回固化结果并广播 OnDiceRolled"))
	FDiceRollResult RollDice();

	/**
	 * 掷骰广播事件（最小可广播 seam，story-002）。
	 *
	 * 本 story 仅声明最小 seam 供执行序铁律③广播 + AC-16c 验返回==payload 同源。
	 * 完整语义（触发次数不变式、重入禁止、完整 BP 暴露纪律）归 story-005，
	 * story-005 在此既有 delegate 上加语义，不重新声明。
	 *
	 * ADR-0004 Guideline 5：OnDiceRolled 回调内禁同步调任何抽取 API
	 * （单线程重入会插入权威序列，破坏 dice F-4 前提②）——完整门控见 story-005。
	 */
	UPROPERTY(BlueprintAssignable, Category="Rento|RNG")
	FOnDiceRolled OnDiceRolled;

	// -------------------------------------------------------------------------
	// story-001：4 个 BP-callable 原语（签名冻结，逐字对齐 ADR-0004 §Key Interfaces）
	// -------------------------------------------------------------------------

	/**
	 * 从权威流取 [Min, Max] 范围内的整数随机数（闭区间）。
	 *
	 * 本 story 只实现正常路径（直通 Stream.RandRange）。
	 * 退化契约（Min==Max 早退 / Min>Max ensure / 2^24 guard）由 story-003 实现。
	 *
	 * @param Min 最小值（含）
	 * @param Max 最大值（含）
	 * @return [Min, Max] 内的随机整数
	 */
	UFUNCTION(BlueprintCallable, Category="Rento|RNG",
		meta=(DisplayName="Random Range (Auth)", ToolTip="从权威流取 [Min,Max] 随机整数，退化契约见 story-003"))
	int32 RandomRange(int32 Min, int32 Max);

	/**
	 * 从权威流取 [0.0, 1.0) 半开区间浮点数（1.0 严格排除）。
	 *
	 * ADR-0004 Guideline 3 + OQ-4：
	 *   下游 floor(f * N) 仅 MVP 小 N（<2^24）安全。
	 *   N>=2^24 浮点舍入越界登记 OQ-4，本 story 不处理。
	 *
	 * @return [0.0, 1.0) 内的浮点随机数，1.0 严格排除
	 */
	UFUNCTION(BlueprintCallable, Category="Rento|RNG",
		meta=(DisplayName="Random Float 0-1 (Auth)", ToolTip="从权威流取 [0.0,1.0) 浮点数"))
	float RandomFloat01();

	/**
	 * 设置权威流种子，重置随机序列到确定性起点。
	 *
	 * 语义约定（ADR-0004 §Key Interfaces）：
	 *   - 0 是合法种子，非哨兵，无特殊路径
	 *   - 负数种子同样合法（FRandomStream::Initialize 接受 int32 全范围）
	 *   - 生产环境由 StartNewGame 玩家配置传入（ff-004/pt-001 接线），
	 *     固定种子仅用于测试 / 调试（bUseFixedSeed 旋钮）
	 *
	 * @param InitialSeed 种子值（int32 全范围合法）
	 */
	UFUNCTION(BlueprintCallable, Category="Rento|RNG",
		meta=(DisplayName="Set RNG Seed (Auth)", ToolTip="设置权威流种子，重置随机序列"))
	void SetSeed(int32 InitialSeed);

	/**
	 * 获取权威流当前种子（显式 accessor，勿靠 struct 反射，ADR-0004 R5 / OQ-2）。
	 *
	 * 说明：FRandomStream 经 UPROPERTY 序列化时可能只持久化 InitialSeed 而丢失
	 * 滚动 Seed，导致 OQ-2 续算静默破坏。本函数显式调用 GetCurrentSeed()
	 * 规避此风险。Seed 序列化（story-008）须经此 accessor。
	 *
	 * @return 当前权威流种子（int32）
	 */
	UFUNCTION(BlueprintCallable, Category="Rento|RNG",
		meta=(DisplayName="Get Current RNG Seed (Auth)", ToolTip="获取权威流当前种子"))
	int32 GetCurrentSeed() const;

	// -------------------------------------------------------------------------
	// 测试 / 调试旋钮（bUseFixedSeed / FixedSeed）
	// 生产路径：bUseFixedSeed=false，OnWorldBeginPlay 不注入（见注释）
	// 测试路径：设 bUseFixedSeed=true + FixedSeed，OnWorldBeginPlay 自动注入
	// -------------------------------------------------------------------------

	/**
	 * 是否使用固定种子（仅测试 / 调试用）。
	 *
	 * false（默认）：生产路径，OnWorldBeginPlay 不注入 Seed——
	 *   这是故意的「未接线」结构事实（story-001 Out of Scope 界定）：
	 *   生产每局不同种子的真实来源为 StartNewGame 玩家配置，
	 *   由 ff-004 / pt-001 在 OnWorldBeginPlay 之后调用 SetSeed() 传入；
	 *   防 seed=0 footgun 的 lazy-init 兜底属 story-004。
	 *   本 story 仅锚定「OnWorldBeginPlay = 唯一合法 Seed 注入时机」这一结构事实。
	 *
	 * true：调用 SetSeed(FixedSeed)，适用于确定性测试 / 回归门 / 调试复现。
	 *
	 * EditDefaultsOnly（而非 EditAnywhere）：UWorldSubsystem 实例不出现在 Details 面板，
	 * EditAnywhere 无意义；子系统调试值应在 CDO 层设置。测试经 C++ 直接设成员，不受影响。
	 */
	UPROPERTY(EditDefaultsOnly, Category="Rento|RNG|Debug",
		meta=(DisplayName="Use Fixed Seed (Test/Debug Only)"))
	bool bUseFixedSeed = false;

	/**
	 * 固定种子值（仅 bUseFixedSeed=true 时生效）。
	 * 0 是合法种子（非哨兵），可用于确定性测试。
	 */
	UPROPERTY(EditDefaultsOnly, Category="Rento|RNG|Debug",
		meta=(DisplayName="Fixed Seed Value", EditCondition="bUseFixedSeed"))
	int32 FixedSeed = 0;

	// -------------------------------------------------------------------------
	// 测试辅助入口（仅 WITH_AUTOMATION_TESTS 构建可见）
	// 用于 headless 测试手动触发 OnWorldBeginPlay 逻辑，
	// 避免测试代码直接访问 protected 虚函数（C++ access control 限制）。
	// -------------------------------------------------------------------------
#if WITH_AUTOMATION_TESTS
	/**
	 * 测试专用：手动触发 OnWorldBeginPlay 逻辑（仅 Automation 构建可见）。
	 * headless 下 CreateWorld 不自动触发 BeginPlay，
	 * 测试通过此入口验证「注入点语义」（TC_LC4）。
	 * 生产代码不应调用此函数（故用 WITH_AUTOMATION_TESTS 门控）。
	 */
	void Test_TriggerOnWorldBeginPlay(UWorld& InWorld)
	{
		OnWorldBeginPlay(InWorld);
	}
#endif // WITH_AUTOMATION_TESTS

protected:
	// -------------------------------------------------------------------------
	// 生命周期 override（ADR-0001 四骨架扩展点）
	// -------------------------------------------------------------------------

	/**
	 * 确定性起点钩子（覆写基类，OnWorldBeginPlay = 唯一合法 Seed 注入时机）。
	 *
	 * ADR-0001 §2 + ADR-0004 control-manifest Forbidden：
	 *   Seed 注入绝不在 Initialize()——此时玩家配置尚未由 StartNewGame 落地。
	 *
	 * @param InWorld 已开始游戏的 World 引用
	 */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	// -------------------------------------------------------------------------
	// 权威 FRandomStream（值语义，private，全项目恰一个）
	//
	// 设计决策（ADR-0004 §Decision）：
	//   - 值语义（非指针）：FRandomStream 是 plain C++ struct，无需 GC 管理
	//   - private：禁止任何 BP 直接访问——所有随机必须经封装函数（BlueprintCallable）
	//   - 不标 UPROPERTY：防止意外 BP 暴露；序列化由 story-008 显式处理（GetCurrentSeed/SetSeed）
	//   - 默构种子恒 0（ADR-0004 Verification ③ CONFIRMED）：
	//     lazy-init 兜底（生产路径 seed 非 0）属 story-004，本 story 不实现
	// -------------------------------------------------------------------------
	FRandomStream AuthoritativeStream;
};
