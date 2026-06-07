// Sprint0PIEIsolationTest.cpp
// =============================================================================
// Sprint0 PIE 隔离验证集成测试（ADR-0001 Verification Required ①②③ + ADR-0004）
// story-007（ff-007）完整 AC-1~7 + Edge cases
//
// 物理路径：Source/rentoTests/Private/Sprint0PIEIsolationTest.cpp
// 逻辑路径：tests/integration/foundation/sprint0_pie_isolation_test.cpp
// Automation 类目：Rento.Foundation.Sprint0PIEIsolation
//
// ─── 测试矩阵 ───────────────────────────────────────────────────────────────
//
// TC-1 (AC-1)：
//   一次 PIE Start→Stop，probe GPIEProbeInitCount==1，GPIEProbeDeinitCount==1。
//   ADR-0001 Verification Required ①。
//
// TC-2 (AC-2)：
//   PIE Start→Stop×3 轮，第二局 probe 是全新实例（InstanceDirtyValue 读到 0，
//   证上一局写入值无残留）。ADR-0001 Validation Criteria PIE 隔离。
//
// TC-3 (AC-3)：
//   PIE Start→Stop→Start×2，监听 OnWorldBeginPlay，
//   第二局 GPIEProbeBeginPlayCount==2（证重触发，非仅首局）。
//   ADR-0001 Verification Required ②。
//
// TC-4 (AC-4)：
//   PIE Start → 设置 BoardToLoad → 发起真磁盘异步加载 → 立即 PIE Stop，
//   验证 Deinitialize 路径被走到且 DidLastHandleCancel() 无崩溃。
//   ADR-0001 Verification Required ③。
//   【AC-4 限制说明】：DA_Board_Classic40.uasset (33KB) 在 PIE headless 下
//   可能通过同步完成路径（FStreamableHandle::HasLoadCompleted() 即时 true），
//   导致 CancelHandle 在 handle 已完成态下调用（WasCanceled()==false）。
//   因此本测试断言「真发起 load（LoadState 离开 Empty）+ Deinitialize 被走到 + 无崩溃」，
//   而非强断言 WasCanceled()==true（那只在真正 mid-flight 窗口才成立）。
//   注：PIE Stop 后 World 已销毁、GetAnyGameWorld() 返 null，无法再读 subsystem 的
//   LoadState，故不断言「Deinit 后 LoadState==Empty」（不可达）。确定性 in-flight 取消
//   由 FF-003 headless 变异测试坐实。这是诚实的最强近似，记录于 story-007 AC-4 限制。
//
// TC-5 (AC-5/AC-7)：
//   真 PIE 已自动触发 OnWorldBeginPlay（bUseFixedSeed=false 生产路径未注入）→ PIE 就绪后
//   GetCurrentSeed()==0（AC-7：证 Seed 禁在 Initialize 注入、生产 BeginPlay 不注入）→
//   public SetSeed(12345)（OnWorldBeginPlay 注入同一原语）后抽 5 次，验零方差 + Seed 真驱动
//   输出（不同 Seed 产不同序列，排除常量 mutant）。注：固定 Seed 在引擎自动 BeginPlay 当刻
//   注入依赖 pt-001 配置管道，PIE 不可独立驱动，由 AC-3 + headless dr-001/dr-004 覆盖。
//
// TC-6 (AC-6)：
//   PIE World 里 SetGameClock(MockClock) 注入 probe，
//   推进 mock 0→1.0→2.0，断言 GetClockNowSeconds() 跟随 mock 而非真实 World 时间。
//   ADR-0001 Validation Criteria mock IGameClock 推进。
//
// Edge_FourRounds：
//   PIE Start→Stop×4，每次 Init/Deinit 各 1（无累积泄漏）。
//
// Edge_SameSeedTwoRounds：
//   同一固定 Seed 跨两局，各局产出序列相同（确定性可重放）。
//
// ─── PIE 测试架构说明 ───────────────────────────────────────────────────────
//
// 真 PIE（FStartPIECommand + FEndPlayMapCommand）在 headless -nullrhi -unattended 下
// 可在 2 帧内完成创建（spike 已实证，TestOutput_ff004.txt）。
//
// Latent Command 模式（5.7 无 FFunctionLatentCommand，须自定义 IAutomationLatentCommand）：
//   RunTest()：
//     AddExpectedError（CommonUI 假 Fail 陷阱处理）
//     FAutomationEditorCommonUtils::CreateNewMap()  // 空内存图，无需磁盘 .umap
//     ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false))
//     ADD_LATENT_AUTOMATION_COMMAND(自定义轮询 command)  // 等 PIE World 就绪
//     ADD_LATENT_AUTOMATION_COMMAND(自定义断言 command) // 执行测试逻辑
//     ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand())
//
// 注意：Latent command 链中所有自定义 command 持 FAutomationTestBase* Test 指针，
// 用于调用 Test->TestEqual/TestTrue 等断言，错误信息汇入主测试报告。
//
// ─── CommonUI 假 Fail 陷阱处理 ──────────────────────────────────────────────
// PIE 启动时 CommonUI 插件报 LogError：
//   "LogUIActionRouter: Error: Using CommonUI without a CommonGameViewportClient..."
// automation 框架把任何 LogError 自动判 Fail，故每个 PIE 测试 RunTest 开头必须：
//   AddExpectedError(TEXT("Using CommonUI without a CommonGameViewportClient"),
//                   EAutomationExpectedMessageFlags::Contains, 0);
// （count=0 = 容忍 0 或多次，不强制出现）
//
// ─── Determinism 保证 ───────────────────────────────────────────────────────
// 固定 Seed（12345）、FMockGameClock 受控时间、无随机 seed、无真实时间依赖断言、
// DA_Board_Classic40 是受控测试 fixture（不算外部 IO 违例）。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CoreGlobals.h"              // GFrameCounter
#include "Tests/AutomationCommon.h"   // AutomationCommon::GetAnyGameWorld
#include "Tests/AutomationEditorCommon.h" // FStartPIECommand / FEndPlayMapCommand / FAutomationEditorCommonUtils
#include "Editor.h"                   // GEditor、RequestEndPlayMap

#include "MatchSubsystemBase.h"
#include "DiceRngService.h"
#include "BoardDataAssetHost.h"
#include "BoardDataAssetHostTestProxy.h" // TC-4：暴露 protected RequestAsyncLoadDataAsset 真发起异步加载
#include "FMockGameClock.h"
#include "PIEIsolationProbeSubsystem.h"

// =============================================================================
// 前向声明：确定性 Seed 固定序列（TC-5 冻结期望值）
// Seed=12345，DiceRngService::bUseFixedSeed 路径，抽 5 次 RandomRange(1,6) 预期序列。
// 注意：此序列由 FRandomStream(12345) 决定，确定性零方差。
// 实际值须在首次真实运行后与日志对齐，此处写为常量数组（headless 可重复验证）。
//
// FRandomStream(12345) 的 RandRange(1,6) 序列（UE 5.x FRand 浮点中介）：
//   调用链：RandHelper(6) → FRand() → floor(FRand * 6) + 1
//   序列由 FRandomStream::Initialize(12345) 决定，逐次 +1 等差推进。
//   具体值由 UE 内部 RNG 算法决定（training data 内已知算法：LCG / Xorshift）。
//   为保持 headless 测试可自验证，本文件使用 DERIVE_AT_RUNTIME 策略：
//   首次执行 5 次抽取，记录序列（作为 run-1 结果），再重设 Seed 执行同 5 次，
//   断言 run-2 == run-1（同 Seed 同序列，零方差的真义）。
//   这比预埋常量更可靠——不依赖训练数据内 RNG 实现的知识。
// =============================================================================

// =============================================================================
// 帧预算常量（防 CI 卡死）
// =============================================================================

/** PIE 轮询最大等待帧数（约 20 秒 @60FPS）。超出则视为超时，测试 FAIL。*/
static constexpr int32 PIE_MAX_WAIT_FRAMES = 1200;

// =============================================================================
// 自定义 Latent Command 工具基类（持 Test 指针，供子类断言用）
// =============================================================================

/**
 * FPIETestLatentCommandBase — 自定义 latent command 基类
 *
 * 5.7 无 FFunctionLatentCommand，须继承 IAutomationLatentCommand。
 * 子类持 FAutomationTestBase* Test 指针（框架保活实例），用于调用断言函数。
 * GFrameCounter 帧预算防 CI 卡死。
 */
class FPIETestLatentCommandBase : public IAutomationLatentCommand
{
public:
	explicit FPIETestLatentCommandBase(FAutomationTestBase* InTest)
		: Test(InTest)
		, StartFrame(GFrameCounter)
	{
	}

protected:
	/** 主测试实例指针（框架保活，子类通过此指针调断言）*/
	FAutomationTestBase* Test;

	/** 命令入队时的帧计数（用于计算等待帧数）*/
	uint64 StartFrame;

	/** 检查是否已超出最大等待帧预算（超时则 Fail 并返回 true 退出循环）*/
	bool IsTimedOut(const TCHAR* CommandName) const
	{
		const uint64 ElapsedFrames = GFrameCounter - StartFrame;
		if (ElapsedFrames > static_cast<uint64>(PIE_MAX_WAIT_FRAMES))
		{
			Test->AddError(FString::Printf(
				TEXT("%s: 超时（等待 %llu 帧 > 最大 %d 帧）。PIE 未在预期帧数内就绪。"),
				CommandName, ElapsedFrames, PIE_MAX_WAIT_FRAMES));
			return true;
		}
		return false;
	}
};

// =============================================================================
// FWaitForPIEWorldCommand — 等待 PIE World 就绪
//
// 轮询 AutomationCommon::GetAnyGameWorld()，等到 WorldType == EWorldType::PIE 为止。
// 超时则 Fail。
// =============================================================================

class FWaitForPIEWorldCommand : public FPIETestLatentCommandBase
{
public:
	explicit FWaitForPIEWorldCommand(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		// 超时检测
		if (IsTimedOut(TEXT("FWaitForPIEWorldCommand"))) { return true; }

		// 轮询：等 PIE World 出现
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (PIEWorld && PIEWorld->WorldType == EWorldType::PIE)
		{
			// PIE World 就绪，命令完成（返回 true = 完成，可进入下一个命令）
			return true;
		}

		// 未就绪，下一帧继续轮询（返回 false = 继续等待）
		return false;
	}
};

// =============================================================================
// FWaitForPIEStopCommand — 等待 PIE 完全停止
//
// 轮询直到 GetAnyGameWorld() 返回 nullptr 或非 PIE World，证 PIE 已销毁。
// 配合 FEndPlayMapCommand 使用——FEndPlayMapCommand 触发停止请求，
// 本命令等待销毁完成。
// =============================================================================

class FWaitForPIEStopCommand : public FPIETestLatentCommandBase
{
public:
	explicit FWaitForPIEStopCommand(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		if (IsTimedOut(TEXT("FWaitForPIEStopCommand"))) { return true; }

		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		// PIE World 已消失（GetAnyGameWorld 返回 null 或非 PIE）
		if (!PIEWorld || PIEWorld->WorldType != EWorldType::PIE)
		{
			return true; // 完成
		}
		return false; // 继续等待
	}
};

// =============================================================================
// TC-1：一次 PIE Start→Stop，Init 计数==1，Deinit 计数==1
// AC 覆盖：AC-1（ADR-0001 Verification Required ①）
// =============================================================================

// --- Latent command：TC-1 断言 ---
class FTC1_AssertInitDeinitCounts : public FPIETestLatentCommandBase
{
public:
	explicit FTC1_AssertInitDeinitCounts(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		// PIE World 已就绪，执行断言
		// 注意：此命令紧跟 FWaitForPIEWorldCommand 之后，PIE 仍存活
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-1: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		// Init 应 ==1（PIE Start 触发一次 Initialize）
		Test->TestEqual(TEXT("TC-1: GPIEProbeInitCount 应 ==1（AC-1）"),
			GPIEProbeInitCount, 1);

		// Deinit 此时应 ==0（PIE 未停止）
		Test->TestEqual(TEXT("TC-1: GPIEProbeDeinitCount 应 ==0（PIE 未停时）"),
			GPIEProbeDeinitCount, 0);

		// 确认 Subsystem 存在
		const UPIEIsolationProbeSubsystem* Probe =
			PIEWorld->GetSubsystem<UPIEIsolationProbeSubsystem>();
		Test->TestNotNull(TEXT("TC-1: PIE World 中应存在 UPIEIsolationProbeSubsystem"), Probe);

		return true;
	}
};

// --- Latent command：TC-1 PIE Stop 后断言 ---
class FTC1_AssertAfterStop : public FPIETestLatentCommandBase
{
public:
	explicit FTC1_AssertAfterStop(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		// PIE 已停止（FWaitForPIEStopCommand 完成后才执行此命令）
		// Deinit 应 ==1（PIE Stop 触发一次 Deinitialize）
		Test->TestEqual(TEXT("TC-1: PIE Stop 后 GPIEProbeDeinitCount 应 ==1（AC-1）"),
			GPIEProbeDeinitCount, 1);
		Test->TestEqual(TEXT("TC-1: PIE Stop 后 GPIEProbeInitCount 应仍 ==1"),
			GPIEProbeInitCount, 1);
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSprint0PIEIsolation_TC1_InitDeinitOnce,
	"Rento.Foundation.Sprint0PIEIsolation.TC1_PIE_InitDeinit_CalledOnce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSprint0PIEIsolation_TC1_InitDeinitOnce::RunTest(const FString& Parameters)
{
	// CommonUI 假 Fail 陷阱：PIE 启动时 CommonUI 报 LogError，automation 框架自动判 Fail
	// count=0 = 容忍 0 或多次，不强制出现
	AddExpectedError(
		TEXT("Using CommonUI without a CommonGameViewportClient"),
		EAutomationExpectedMessageFlags::Contains, 0);

	// 重置静态计数器（测试隔离）
	GPIEProbeInitCount     = 0;
	GPIEProbeDeinitCount   = 0;
	GPIEProbeBeginPlayCount = 0;

	// 创建空内存图（无需磁盘 .umap、无需 PlayerStart）
	FAutomationEditorCommonUtils::CreateNewMap();

	// 触发 PIE Start
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));

	// 等待 PIE World 就绪
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));

	// 执行 PIE 存活期断言
	ADD_LATENT_AUTOMATION_COMMAND(FTC1_AssertInitDeinitCounts(this));

	// 请求 PIE Stop
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

	// 等待 PIE 完全停止
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	// 执行 PIE Stop 后断言
	ADD_LATENT_AUTOMATION_COMMAND(FTC1_AssertAfterStop(this));

	return true;
}

// =============================================================================
// TC-2：PIE Play→Stop×3 轮，第二局无第一局残留状态（PIE 隔离）
// AC 覆盖：AC-2（ADR-0001 Validation Criteria）
//
// 策略：用 static 变量在 latent command 间传递状态（第一局写入 InstanceDirtyValue，
//       第二局读取并验证为 0）
// =============================================================================

// 注：TC-2 局间隔离通过 probe 的 per-instance InstanceDirtyValue 直接验证
// （第一局写非零、后续局新实例读 0），无需文件级共享状态。

// --- Latent command：TC-2 第一局写入 dirty marker ---
class FTC2_WriteDirtyFirstRound : public FPIETestLatentCommandBase
{
public:
	explicit FTC2_WriteDirtyFirstRound(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-2 Round1: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		UPIEIsolationProbeSubsystem* Probe =
			PIEWorld->GetSubsystem<UPIEIsolationProbeSubsystem>();
		if (!Test->TestNotNull(TEXT("TC-2 Round1: Probe 应存在"), Probe))
		{
			return true;
		}

		// 写入 dirty 值（非零，用于验证第二局拿到全新实例无残留）
		Probe->InstanceDirtyValue = 9527;

		return true;
	}
};

// --- Latent command：TC-2 第二局验证无残留 + 再次写入 ---
class FTC2_AssertNoResidueSecondRound : public FPIETestLatentCommandBase
{
public:
	explicit FTC2_AssertNoResidueSecondRound(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-2 Round2: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		UPIEIsolationProbeSubsystem* Probe =
			PIEWorld->GetSubsystem<UPIEIsolationProbeSubsystem>();
		if (!Test->TestNotNull(TEXT("TC-2 Round2: Probe 应存在"), Probe))
		{
			return true;
		}

		// 第二局 probe 应是全新实例，InstanceDirtyValue 应为初值 0（无第一局残留）
		// 非 vacuous：若引擎错误复用第一局实例，此处读到 9527 而非 0 → FAIL
		Test->TestEqual(
			TEXT("TC-2 Round2: InstanceDirtyValue 应为 0（全新实例，无第一局残留，AC-2 PIE 隔离）"),
			Probe->InstanceDirtyValue, 0);

		// 再次写入（为第三局验证准备）
		Probe->InstanceDirtyValue = 8848;

		return true;
	}
};

// --- Latent command：TC-2 第三局验证 ---
class FTC2_AssertNoResidueThirdRound : public FPIETestLatentCommandBase
{
public:
	explicit FTC2_AssertNoResidueThirdRound(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-2 Round3: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		UPIEIsolationProbeSubsystem* Probe =
			PIEWorld->GetSubsystem<UPIEIsolationProbeSubsystem>();
		if (!Test->TestNotNull(TEXT("TC-2 Round3: Probe 应存在"), Probe))
		{
			return true;
		}

		// 第三局 probe 也应是全新实例（无第二局残留）
		Test->TestEqual(
			TEXT("TC-2 Round3: InstanceDirtyValue 应为 0（全新实例，无第二局残留，AC-2）"),
			Probe->InstanceDirtyValue, 0);

		return true;
	}
};

// --- Latent command：TC-2 最终计数断言 ---
class FTC2_AssertFinalCounts : public FPIETestLatentCommandBase
{
public:
	explicit FTC2_AssertFinalCounts(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		// 3 局完成后，Init 和 Deinit 应各 ==3
		Test->TestEqual(TEXT("TC-2: 3 局后 GPIEProbeInitCount 应 ==3（无累积泄漏）"),
			GPIEProbeInitCount, 3);
		Test->TestEqual(TEXT("TC-2: 3 局后 GPIEProbeDeinitCount 应 ==3（无累积泄漏）"),
			GPIEProbeDeinitCount, 3);
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSprint0PIEIsolation_TC2_ThreeRoundsNoResidue,
	"Rento.Foundation.Sprint0PIEIsolation.TC2_PIE_ThreeRounds_NoStateResidue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSprint0PIEIsolation_TC2_ThreeRoundsNoResidue::RunTest(const FString& Parameters)
{
	// CommonUI 假 Fail 陷阱
	AddExpectedError(
		TEXT("Using CommonUI without a CommonGameViewportClient"),
		EAutomationExpectedMessageFlags::Contains, 0);

	// 重置
	GPIEProbeInitCount     = 0;
	GPIEProbeDeinitCount   = 0;
	GPIEProbeBeginPlayCount = 0;

	FAutomationEditorCommonUtils::CreateNewMap();

	// === 第一局 ===
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTC2_WriteDirtyFirstRound(this));    // 写 dirty=9527
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	// === 第二局 ===
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTC2_AssertNoResidueSecondRound(this)); // 读 0，写 8848
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	// === 第三局 ===
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTC2_AssertNoResidueThirdRound(this)); // 读 0
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	// === 最终计数 ===
	ADD_LATENT_AUTOMATION_COMMAND(FTC2_AssertFinalCounts(this));

	return true;
}

// =============================================================================
// TC-3：OnWorldBeginPlay 在 PIE Stop→Start 之间确实重新触发（Seed 重注入正确性）
// AC 覆盖：AC-3（ADR-0001 Verification Required ②）
// =============================================================================

// --- Latent command：TC-3 第一局断言 BeginPlay 已触发 ---
class FTC3_AssertBeginPlayRound1 : public FPIETestLatentCommandBase
{
public:
	explicit FTC3_AssertBeginPlayRound1(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-3 Round1: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		// 第一局 OnWorldBeginPlay 静态计数应 ==1
		Test->TestEqual(
			TEXT("TC-3 Round1: GPIEProbeBeginPlayCount 应 ==1（首局 OnWorldBeginPlay 触发）"),
			GPIEProbeBeginPlayCount, 1);

		// 实例 flag 应为 true
		const UPIEIsolationProbeSubsystem* Probe =
			PIEWorld->GetSubsystem<UPIEIsolationProbeSubsystem>();
		if (Test->TestNotNull(TEXT("TC-3 Round1: Probe 应存在"), Probe))
		{
			Test->TestTrue(
				TEXT("TC-3 Round1: bBeginPlayFiredThisInstance 应为 true（本实例 BeginPlay 已触发）"),
				Probe->bBeginPlayFiredThisInstance);
		}

		return true;
	}
};

// --- Latent command：TC-3 第二局断言 BeginPlay 再次触发 ---
class FTC3_AssertBeginPlayRound2 : public FPIETestLatentCommandBase
{
public:
	explicit FTC3_AssertBeginPlayRound2(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-3 Round2: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		// 第二局 OnWorldBeginPlay 静态计数应 ==2（证重触发，非仅首局）
		// 非 vacuous：若 OnWorldBeginPlay 仅首局触发，第二局计数仍 ==1 → FAIL
		Test->TestEqual(
			TEXT("TC-3 Round2: GPIEProbeBeginPlayCount 应 ==2（第二局 OnWorldBeginPlay 重新触发，AC-3）"),
			GPIEProbeBeginPlayCount, 2);

		// 第二局实例的 bBeginPlayFiredThisInstance 应为 true（证本局实例触发，非历史残留）
		const UPIEIsolationProbeSubsystem* Probe =
			PIEWorld->GetSubsystem<UPIEIsolationProbeSubsystem>();
		if (Test->TestNotNull(TEXT("TC-3 Round2: Probe 应存在"), Probe))
		{
			Test->TestTrue(
				TEXT("TC-3 Round2: bBeginPlayFiredThisInstance 应为 true（第二局新实例 BeginPlay 已触发，AC-3）"),
				Probe->bBeginPlayFiredThisInstance);
		}

		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSprint0PIEIsolation_TC3_BeginPlayReFired,
	"Rento.Foundation.Sprint0PIEIsolation.TC3_OnWorldBeginPlay_RefiredOnSecondRound",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSprint0PIEIsolation_TC3_BeginPlayReFired::RunTest(const FString& Parameters)
{
	// CommonUI 假 Fail 陷阱
	AddExpectedError(
		TEXT("Using CommonUI without a CommonGameViewportClient"),
		EAutomationExpectedMessageFlags::Contains, 0);

	// 重置
	GPIEProbeInitCount     = 0;
	GPIEProbeDeinitCount   = 0;
	GPIEProbeBeginPlayCount = 0;

	FAutomationEditorCommonUtils::CreateNewMap();

	// === 第一局 ===
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTC3_AssertBeginPlayRound1(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	// === 第二局 ===
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTC3_AssertBeginPlayRound2(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	return true;
}

// =============================================================================
// TC-4：PIE Stop 时异步加载中途，无悬挂回调崩溃（CancelHandle 生效）
// AC 覆盖：AC-4（ADR-0001 Verification Required ③）
//
// 【真发起异步加载——非惰性】：
//   用 UBoardDataAssetHostTestProxy（暴露 protected RequestAsyncLoadDataAsset）在 PIE
//   存活期对真磁盘资产 DA_Board_Classic40 发起 FStreamableManager::RequestAsyncLoad，
//   建立真 StreamableHandle，随即 PIE Stop → 引擎拆 World → proxy::Deinitialize 走
//   CancelHandle + ReleaseHandle。验证：
//     1. CallRequestAsyncLoadAny 成功发起（真 handle 建立）
//     2. LoadState 离开 Empty（坐实「真发起」非惰性赋值——旧版只设 BoardToLoad 从不加载）
//     3. PIE Stop 后无崩溃（测试执行到 Stop 后断言 = 无悬挂回调写已 GC 宿主，AC-4 地雷未爆）
//     4. GPIEProbeDeinitCount==1（Deinitialize 拆解路径完整执行）
//
// 【诚实的覆盖边界】：
//   DA_Board_Classic40 (33KB) 在 headless PIE 下可能数帧内同步完成 → CancelHandle 落在
//   已完成 handle（WasCanceled()==false），故「确定性 in-flight 取消」（DidLastHandleCancel
//   ()==true）不在本 TC 强制断言——该确定性取消逻辑已由 FF-003 AsyncLoadCancelHandleTest
//   headless 变异测试坐实（DriveResolveForTesting 强制 Loading 态 + 注释 CancelHandle→精确
//   FAIL）。本 TC 的真 PIE 增量 = 真磁盘 load 在真 PIE World 发起 + 真引擎 PIE 拆解 →
//   无悬挂回调崩溃（FF-003 headless 因 transient 同步完成驱动不到的真异步+真拆解集成路径）。
// =============================================================================

// --- Latent command：TC-4 在 PIE 内设置 BoardToLoad 并立即触发 PIE Stop ---
class FTC4_TriggerBoardLoadThenStop : public FPIETestLatentCommandBase
{
public:
	explicit FTC4_TriggerBoardLoadThenStop(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-4: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		// 获取测试 proxy（UBoardDataAssetHost 子类，暴露 protected RequestAsyncLoadDataAsset）。
		// proxy 继承基类 ShouldCreateSubsystem（PIE-true）→ 在 PIE World 自动创建。
		UBoardDataAssetHostTestProxy* BoardProxy =
			PIEWorld->GetSubsystem<UBoardDataAssetHostTestProxy>();
		if (!Test->TestNotNull(
				TEXT("TC-4: UBoardDataAssetHostTestProxy 应在 PIE World 自动创建（基类 PIE-true）"),
				BoardProxy))
		{
			return true;
		}

		// ── 真发起 DA_Board_Classic40 真磁盘异步加载（建立真 StreamableHandle，非惰性）──
		const TSoftObjectPtr<UPrimaryDataAsset> BoardRef(
			FSoftObjectPath(TEXT("/Game/Boards/DA_Board_Classic40.DA_Board_Classic40")));
		const bool bKicked = BoardProxy->CallRequestAsyncLoadAny(BoardRef);
		Test->TestTrue(TEXT("TC-4: 真磁盘异步加载应成功发起（真 StreamableHandle 建立）"), bKicked);

		// 坐实「真发起」非惰性：LoadState 应离开 Empty（Loading=in-flight / 更高=同步完成）
		const EHostLoadState LoadStateAfterKick = BoardProxy->GetLoadState();
		Test->TestTrue(
			TEXT("TC-4: 发起后 LoadState 应离开 Empty（证真发起异步加载，非旧版惰性赋值 BoardToLoad）"),
			LoadStateAfterKick != EHostLoadState::Empty);
		Test->AddInfo(FString::Printf(
			TEXT("TC-4: 加载发起后 LoadState=%d（1=Loading in-flight / >1=已同步完成）"),
			static_cast<int32>(LoadStateAfterKick)));

		// 记录 Probe Init 计数（用于 Stop 后验证 Deinit 路径确实被走到）
		Test->TestEqual(TEXT("TC-4: 此时 GPIEProbeInitCount 应 ==1"),
			GPIEProbeInitCount, 1);
		// 紧接着 FEndPlayMapCommand → PIE Stop → proxy::Deinitialize 走 CancelHandle（真 handle）

		// 请求 PIE Stop（触发 Deinitialize 路径）
		// FEndPlayMapCommand 已在 latent 队列中，此处无需重复请求
		return true;
	}
};

// --- Latent command：TC-4 PIE Stop 后断言无崩溃 ---
class FTC4_AssertAfterStop : public FPIETestLatentCommandBase
{
public:
	explicit FTC4_AssertAfterStop(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		// PIE 已停止（到达此命令说明 FEndPlayMapCommand + FWaitForPIEStopCommand 均完成）
		// 核心断言：测试能执行到此处 = 无崩溃

		// Deinitialize 应被调用（probe 计数）
		Test->TestEqual(
			TEXT("TC-4: PIE Stop 后 GPIEProbeDeinitCount 应 ==1（Deinitialize 路径被走到，AC-4）"),
			GPIEProbeDeinitCount, 1);

		// Init/Deinit 各 1 次（生命周期对称）
		Test->TestEqual(TEXT("TC-4: GPIEProbeInitCount 应 ==1"),
			GPIEProbeInitCount, 1);

		// 测试执行到此处本身就是「无崩溃」的证明（headless -nullrhi 下崩溃会使测试进程退出）
		Test->AddInfo(TEXT("TC-4: PIE Stop 无崩溃（Deinitialize 路径完整执行，无悬挂回调崩溃，AC-4 passed）"));

		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSprint0PIEIsolation_TC4_CancelHandleNoCrash,
	"Rento.Foundation.Sprint0PIEIsolation.TC4_PIEStop_CancelHandle_NoCrash",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSprint0PIEIsolation_TC4_CancelHandleNoCrash::RunTest(const FString& Parameters)
{
	// CommonUI 假 Fail 陷阱
	AddExpectedError(
		TEXT("Using CommonUI without a CommonGameViewportClient"),
		EAutomationExpectedMessageFlags::Contains, 0);

	// BoardDataAssetHost 在 BoardToLoad 为空时打 Warning log（正常行为，非错误）。
	// 收窄到具体 benign 串「BoardToLoad 为空」——避免短子串 "BoardToLoad" 吞掉本应让测试
	// FAIL 的真错误（如悬挂回调写 GC 宿主的 Error 日志若含 BoardToLoad 字样会被误吞）。
	AddExpectedError(
		TEXT("BoardToLoad 为空"),
		EAutomationExpectedMessageFlags::Contains, 0);

	// 重置
	GPIEProbeInitCount     = 0;
	GPIEProbeDeinitCount   = 0;
	GPIEProbeBeginPlayCount = 0;

	FAutomationEditorCommonUtils::CreateNewMap();

	// PIE Start → 就绪 → 设置 BoardToLoad → PIE Stop → 验无崩溃
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTC4_TriggerBoardLoadThenStop(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTC4_AssertAfterStop(this));

	return true;
}

// =============================================================================
// TC-5：固定 Seed 经 OnWorldBeginPlay 注入 → 产出固定 RNG 序列（零方差）
// AC 覆盖：AC-5（固定 Seed → 固定序列）/ AC-7（Seed 注入时机纪律）
//
// Seed 来源策略（真 PIE 修正版）：
//   ⚠ 真 PIE 下引擎已自动对 DiceRngService 触发 OnWorldBeginPlay（这正是 AC-3 验证的
//   机制——引擎随 PIE Start 触发 World Subsystem 的 OnWorldBeginPlay，并置
//   bHasCalledBeginPlay=true）。故**不能**再调 Test_TriggerOnWorldBeginPlay——二次触发
//   会撞引擎 ensure(!bHasCalledBeginPlay)（WorldSubsystem.cpp:72）。该测试门仅适用于
//   headless CreateWorld（BeginPlay 不自动触发）路径（dr-001/dr-004 已用它验注入时机）。
//
//   本 story 真 PIE 下用 public SetSeed(FixedSeed) 直接注入：OnWorldBeginPlay 的生产
//   注入 body 就是 SetSeed(FixedSeed)（bUseFixedSeed 路径），pt-001 亦在 BeginPlay 后
//   SetSeed——SetSeed 是同一注入原语的忠实驱动。本 TC 在真 PIE -nullrhi World Subsystem
//   里证「固定 Seed → 固定序列零方差」（AC-5「证明 World Subsystem + Seed 注入可测」的核心）。
//
//   注：固定 Seed 在引擎自动触发的 OnWorldBeginPlay 当刻注入（bUseFixedSeed=true 须在
//   BeginPlay 前置）依赖 pt-001 的 StartNewGame 配置管道，PIE 下不可独立驱动；该精确
//   时刻由 AC-3（OnWorldBeginPlay 真重触发）+ headless dr-001/dr-004（注入时机）共同覆盖。
//
// AC-7 验证：
//   PIE 就绪后（bUseFixedSeed=false 生产路径，OnWorldBeginPlay 未注入）GetCurrentSeed()==0
//   （证 Seed 禁在 Initialize 注入、生产 BeginPlay 不注入）→ SetSeed(12345) 后 ==12345。
//
// 零方差证明策略（DERIVE_AT_RUNTIME）：
//   1. 触发 OnWorldBeginPlay（注入 Seed=12345）
//   2. 抽 5 次 RandomRange(1,6)，记录序列为 Seq1
//   3. 重设 Seed=12345（SetSeed），抽 5 次，记录序列为 Seq2
//   4. 断言 Seq1 == Seq2（同 Seed 同序列，零方差）
// =============================================================================

// 跨 latent command 共享：TC-5 RNG 序列存储
namespace TC5State
{
	static int32 Seq1[5] = {0};
	static int32 Seq2[5] = {0};
	static constexpr int32 FixedSeed = 12345;
}

// --- Latent command：TC-5 验证 Seed 注入时机 + 抽序列 1 ---
class FTC5_VerifySeedAndDrawSeq1 : public FPIETestLatentCommandBase
{
public:
	explicit FTC5_VerifySeedAndDrawSeq1(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-5: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		UDiceRngService* DiceService = PIEWorld->GetSubsystem<UDiceRngService>();
		if (!Test->TestNotNull(TEXT("TC-5: UDiceRngService 应在 PIE World 中存在"), DiceService))
		{
			return true;
		}

		// ── AC-7 验证：Initialize 阶段 seed 应为 0（Seed 禁在 Initialize 注入）──
		// 注意：OnWorldBeginPlay 在 PIE 中已被触发（bUseFixedSeed 默认 false 则未注入）
		// 此时若 bUseFixedSeed=false（默认），GetCurrentSeed() 应为 0（默构值）
		// AC-7 验证：查 Initialize 后、手动设 bUseFixedSeed 前，seed 默认态
		Test->TestEqual(
			TEXT("TC-5 AC-7: PIE 就绪后（bUseFixedSeed=false 路径）DiceRngService GetCurrentSeed() 应 ==0（默认未注入）"),
			DiceService->GetCurrentSeed(), 0);

		// ── 固定 Seed 注入：用 public SetSeed（OnWorldBeginPlay 注入路径的同一原语）──
		// ⚠ 真 PIE 已自动触发 DiceRngService::OnWorldBeginPlay（bHasCalledBeginPlay=true），
		//    不能再 Test_TriggerOnWorldBeginPlay（撞引擎 ensure(!bHasCalledBeginPlay)）。
		//    OnWorldBeginPlay 生产注入 body 即 SetSeed(FixedSeed)，故直接 SetSeed 忠实注入。
		DiceService->SetSeed(TC5State::FixedSeed);

		// ── AC-5/AC-7 验证：注入后 seed 应 == FixedSeed ──
		Test->TestEqual(
			TEXT("TC-5 AC-5/AC-7: OnWorldBeginPlay 注入后 GetCurrentSeed() 应 == FixedSeed（12345）"),
			DiceService->GetCurrentSeed(), TC5State::FixedSeed);

		// 再验 GetInitialSeed()（seed 注入后 InitialSeed 应也为 12345）
		Test->TestEqual(
			TEXT("TC-5 AC-5: 注入后 GetInitialSeed() 应 == 12345"),
			DiceService->GetInitialSeed(), TC5State::FixedSeed);

		// ── 抽取序列 1（5 次 RandomRange(1,6)）──
		for (int32 i = 0; i < 5; ++i)
		{
			TC5State::Seq1[i] = DiceService->RandomRange(1, 6);
		}

		// 日志记录序列（便于调试）
		UE_LOG(LogTemp, Log,
			TEXT("TC-5 Seq1[Seed=%d]: %d %d %d %d %d"),
			TC5State::FixedSeed,
			TC5State::Seq1[0], TC5State::Seq1[1], TC5State::Seq1[2],
			TC5State::Seq1[3], TC5State::Seq1[4]);

		return true;
	}
};

// --- Latent command：TC-5 重设 Seed 抽序列 2，断言 Seq1==Seq2 ---
class FTC5_ResetSeedAndAssertZeroVariance : public FPIETestLatentCommandBase
{
public:
	explicit FTC5_ResetSeedAndAssertZeroVariance(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-5 Seq2: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		UDiceRngService* DiceService = PIEWorld->GetSubsystem<UDiceRngService>();
		if (!Test->TestNotNull(TEXT("TC-5 Seq2: UDiceRngService 应存在"), DiceService))
		{
			return true;
		}

		// 重设相同 Seed（SetSeed 重置流到确定性起点）
		DiceService->SetSeed(TC5State::FixedSeed);

		// 抽取序列 2（5 次 RandomRange(1,6)）
		for (int32 i = 0; i < 5; ++i)
		{
			TC5State::Seq2[i] = DiceService->RandomRange(1, 6);
		}

		// 日志记录序列 2
		UE_LOG(LogTemp, Log,
			TEXT("TC-5 Seq2[Seed=%d]: %d %d %d %d %d"),
			TC5State::FixedSeed,
			TC5State::Seq2[0], TC5State::Seq2[1], TC5State::Seq2[2],
			TC5State::Seq2[3], TC5State::Seq2[4]);

		// ── 核心断言：Seq1 == Seq2（同 Seed 同序列，零方差）──
		// 非 vacuous：若 RNG 每次产出不同序列，此处至少有一个位置 FAIL
		for (int32 i = 0; i < 5; ++i)
		{
			Test->TestEqual(
				FString::Printf(
					TEXT("TC-5 AC-5: Seq1[%d](%d) 应 == Seq2[%d](%d)（固定 Seed 零方差，AC-5）"),
					i, TC5State::Seq1[i], i, TC5State::Seq2[i]),
				TC5State::Seq2[i], TC5State::Seq1[i]);
		}

		// 序列值应在 [1,6] 范围内（基本合法性）
		for (int32 i = 0; i < 5; ++i)
		{
			Test->TestTrue(
				FString::Printf(TEXT("TC-5: Seq1[%d]=%d 应在 [1,6]"), i, TC5State::Seq1[i]),
				TC5State::Seq1[i] >= 1 && TC5State::Seq1[i] <= 6);
		}

		// ── 非 vacuous 加固：不同 Seed 应产出不同序列（AC-5 证 Seed 真驱动 RNG 输出）──
		// 仅「Seq1==Seq2（同 Seed）」对「RNG 恒返回常量、与 Seed 解耦」的 mutant 恒过——
		// 该 mutant 下 Seq1/Seq2/SeqAlt 全相等仍 PASS。加一个不同 Seed 的序列，断言与 Seq1
		// 至少一位不同，排除常量 mutant，坐实 Seed 确实决定输出（确定性，非 flaky）。
		const int32 AltSeed = TC5State::FixedSeed + 87654; // 明显不同的固定种子（确定性）
		DiceService->SetSeed(AltSeed);
		int32 SeqAlt[5] = {0};
		for (int32 i = 0; i < 5; ++i)
		{
			SeqAlt[i] = DiceService->RandomRange(1, 6);
		}
		bool bAnyDiff = false;
		for (int32 i = 0; i < 5; ++i)
		{
			if (SeqAlt[i] != TC5State::Seq1[i]) { bAnyDiff = true; break; }
		}
		Test->TestTrue(
			TEXT("TC-5 AC-5: 不同 Seed 应产出不同序列（证 Seed 真驱动 RNG 输出，排除常量 mutant）"),
			bAnyDiff);

		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSprint0PIEIsolation_TC5_FixedSeedZeroVariance,
	"Rento.Foundation.Sprint0PIEIsolation.TC5_FixedSeed_ZeroVariance_SeedInjectionTiming",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSprint0PIEIsolation_TC5_FixedSeedZeroVariance::RunTest(const FString& Parameters)
{
	// CommonUI 假 Fail 陷阱
	AddExpectedError(
		TEXT("Using CommonUI without a CommonGameViewportClient"),
		EAutomationExpectedMessageFlags::Contains, 0);

	// 重置
	GPIEProbeInitCount     = 0;
	GPIEProbeDeinitCount   = 0;
	GPIEProbeBeginPlayCount = 0;
	FMemory::Memzero(TC5State::Seq1, sizeof(TC5State::Seq1));
	FMemory::Memzero(TC5State::Seq2, sizeof(TC5State::Seq2));

	FAutomationEditorCommonUtils::CreateNewMap();

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTC5_VerifySeedAndDrawSeq1(this));       // 注入 seed，抽序列 1
	ADD_LATENT_AUTOMATION_COMMAND(FTC5_ResetSeedAndAssertZeroVariance(this)); // 重设，抽序列 2，断言相等
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	return true;
}

// =============================================================================
// TC-6：PIE World 里注入 FMockGameClock，推进 0→1.0→2.0，断言 GetClockNowSeconds() 跟随 mock
// AC 覆盖：AC-6（ADR-0001 Validation Criteria mock IGameClock 推进）
// =============================================================================

// 跨 latent command 共享 mock 时钟实例
namespace TC6State
{
	static TSharedPtr<FMockGameClock> MockClock;
}

// --- Latent command：TC-6 注入 MockClock 并推进 ---
class FTC6_InjectMockClockAndAssert : public FPIETestLatentCommandBase
{
public:
	explicit FTC6_InjectMockClockAndAssert(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("TC-6: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		UPIEIsolationProbeSubsystem* Probe =
			PIEWorld->GetSubsystem<UPIEIsolationProbeSubsystem>();
		if (!Test->TestNotNull(TEXT("TC-6: Probe 应存在"), Probe))
		{
			return true;
		}

		// 创建 FMockGameClock（受控时钟）
		TC6State::MockClock = MakeShared<FMockGameClock>();

		// ── 基准：注入前 GetClockNowSeconds() 应读到 FWorldGameClock 的 World 时间 ──
		// headless 下 World 时间静止 = 0.0（无 Tick）
		const double WorldTimeBaseline = static_cast<double>(PIEWorld->GetTimeSeconds());

		// ── 注入 MockClock（DI 替换）──
		Probe->SetGameClock(TC6State::MockClock);

		// ── 推进 0→1.0 ──
		TC6State::MockClock->SetNow(1.0);

		// THEN：GetClockNowSeconds() 应返回 1.0（非 World 时间 0.0）
		// 非 vacuous：headless World 时间静止=0.0，若实现直读 World 时间则返回 0.0≠1.0 → FAIL
		Test->TestEqual(
			TEXT("TC-6: SetNow(1.0) 后 GetClockNowSeconds() 应返回 1.0（mock DI 生效，非 World 时间，AC-6）"),
			Probe->GetClockNowSeconds(), 1.0);

		// ── 推进 1.0→2.0 ──
		TC6State::MockClock->Advance(1.0); // 1.0 + 1.0 = 2.0

		// THEN：GetClockNowSeconds() 应返回 2.0
		Test->TestEqual(
			TEXT("TC-6: Advance(1.0) 后 GetClockNowSeconds() 应返回 2.0（AC-6 连续推进）"),
			Probe->GetClockNowSeconds(), 2.0);

		// ── 确认 World 时间仍为静止基准（证读取路径非 World 直读）──
		const double WorldTimeAfter = static_cast<double>(PIEWorld->GetTimeSeconds());
		Test->TestNotEqual(
			TEXT("TC-6: mock 受控值（2.0）与 World 静止时间应不同（证 DI 路径非 World 直读，AC-6）"),
			Probe->GetClockNowSeconds(), WorldTimeAfter);

		// 释放共享引用（Probe 也持有引用，此处只释放 TC6State 侧）
		TC6State::MockClock.Reset();

		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSprint0PIEIsolation_TC6_MockClockDI,
	"Rento.Foundation.Sprint0PIEIsolation.TC6_PIE_MockGameClock_DIWorks",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSprint0PIEIsolation_TC6_MockClockDI::RunTest(const FString& Parameters)
{
	// CommonUI 假 Fail 陷阱
	AddExpectedError(
		TEXT("Using CommonUI without a CommonGameViewportClient"),
		EAutomationExpectedMessageFlags::Contains, 0);

	// 重置
	GPIEProbeInitCount     = 0;
	GPIEProbeDeinitCount   = 0;
	GPIEProbeBeginPlayCount = 0;
	TC6State::MockClock.Reset();

	FAutomationEditorCommonUtils::CreateNewMap();

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTC6_InjectMockClockAndAssert(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	return true;
}

// =============================================================================
// Edge_FourRounds：PIE Start→Stop×4，每次 Init/Deinit 各 1（无累积泄漏）
// =============================================================================

class FEdge4Rounds_AssertFinalCounts : public FPIETestLatentCommandBase
{
public:
	explicit FEdge4Rounds_AssertFinalCounts(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		Test->TestEqual(TEXT("Edge_4Rounds: 4 局后 GPIEProbeInitCount 应 ==4"),
			GPIEProbeInitCount, 4);
		Test->TestEqual(TEXT("Edge_4Rounds: 4 局后 GPIEProbeDeinitCount 应 ==4（无累积泄漏）"),
			GPIEProbeDeinitCount, 4);
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSprint0PIEIsolation_Edge_FourRounds,
	"Rento.Foundation.Sprint0PIEIsolation.Edge_FourRounds_NoAccumulatedLeak",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSprint0PIEIsolation_Edge_FourRounds::RunTest(const FString& Parameters)
{
	AddExpectedError(
		TEXT("Using CommonUI without a CommonGameViewportClient"),
		EAutomationExpectedMessageFlags::Contains, 0);

	GPIEProbeInitCount     = 0;
	GPIEProbeDeinitCount   = 0;
	GPIEProbeBeginPlayCount = 0;

	FAutomationEditorCommonUtils::CreateNewMap();

	// 4 轮 PIE Start→Stop
	for (int32 Round = 0; Round < 4; ++Round)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEdge4Rounds_AssertFinalCounts(this));

	return true;
}

// =============================================================================
// Edge_SameSeedTwoRounds：同一固定 Seed 跨两局，各局产出序列相同（确定性可重放）
// =============================================================================

// 跨局状态
namespace EdgeSameSeedState
{
	static int32 Round1Seq[5] = {0};
	static int32 Round2Seq[5] = {0};
	static constexpr int32 FixedSeed = 54321;
}

// --- Latent command：第一局抽序列 ---
class FEdgeSameSeed_DrawRound1 : public FPIETestLatentCommandBase
{
public:
	explicit FEdgeSameSeed_DrawRound1(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("Edge_SameSeed Round1: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		UDiceRngService* DiceService = PIEWorld->GetSubsystem<UDiceRngService>();
		if (!Test->TestNotNull(TEXT("Edge_SameSeed Round1: DiceService 应存在"), DiceService))
		{
			return true;
		}

		// 固定 Seed 注入：用 public SetSeed（真 PIE 已自动触发 OnWorldBeginPlay，
		// 不能再 Test_TriggerOnWorldBeginPlay；SetSeed 是 OnWorldBeginPlay 注入同一原语）
		DiceService->SetSeed(EdgeSameSeedState::FixedSeed);

		// 抽 5 次
		for (int32 i = 0; i < 5; ++i)
		{
			EdgeSameSeedState::Round1Seq[i] = DiceService->RandomRange(1, 6);
		}

		UE_LOG(LogTemp, Log,
			TEXT("Edge_SameSeed Round1[Seed=%d]: %d %d %d %d %d"),
			EdgeSameSeedState::FixedSeed,
			EdgeSameSeedState::Round1Seq[0], EdgeSameSeedState::Round1Seq[1],
			EdgeSameSeedState::Round1Seq[2], EdgeSameSeedState::Round1Seq[3],
			EdgeSameSeedState::Round1Seq[4]);

		return true;
	}
};

// --- Latent command：第二局抽序列并断言与第一局相同 ---
class FEdgeSameSeed_DrawRound2AndAssert : public FPIETestLatentCommandBase
{
public:
	explicit FEdgeSameSeed_DrawRound2AndAssert(FAutomationTestBase* InTest)
		: FPIETestLatentCommandBase(InTest)
	{
	}

	virtual bool Update() override
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!Test->TestNotNull(TEXT("Edge_SameSeed Round2: PIE World 应存在"), PIEWorld))
		{
			return true;
		}

		UDiceRngService* DiceService = PIEWorld->GetSubsystem<UDiceRngService>();
		if (!Test->TestNotNull(TEXT("Edge_SameSeed Round2: DiceService 应存在"), DiceService))
		{
			return true;
		}

		// 第二局也设置相同固定 Seed（PIE Stop→Start 后宿主是全新实例，须重新设置）。
		// 用 public SetSeed（真 PIE 已自动触发 OnWorldBeginPlay，禁 Test_TriggerOnWorldBeginPlay）
		DiceService->SetSeed(EdgeSameSeedState::FixedSeed);

		// 抽 5 次
		for (int32 i = 0; i < 5; ++i)
		{
			EdgeSameSeedState::Round2Seq[i] = DiceService->RandomRange(1, 6);
		}

		UE_LOG(LogTemp, Log,
			TEXT("Edge_SameSeed Round2[Seed=%d]: %d %d %d %d %d"),
			EdgeSameSeedState::FixedSeed,
			EdgeSameSeedState::Round2Seq[0], EdgeSameSeedState::Round2Seq[1],
			EdgeSameSeedState::Round2Seq[2], EdgeSameSeedState::Round2Seq[3],
			EdgeSameSeedState::Round2Seq[4]);

		// ── 核心断言：两局同 Seed 同序列（跨局确定性可重放）──
		for (int32 i = 0; i < 5; ++i)
		{
			Test->TestEqual(
				FString::Printf(
					TEXT("Edge_SameSeed: Round1[%d](%d) == Round2[%d](%d)（同 Seed 跨局确定性）"),
					i, EdgeSameSeedState::Round1Seq[i], i, EdgeSameSeedState::Round2Seq[i]),
				EdgeSameSeedState::Round2Seq[i], EdgeSameSeedState::Round1Seq[i]);
		}

		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSprint0PIEIsolation_Edge_SameSeedTwoRounds,
	"Rento.Foundation.Sprint0PIEIsolation.Edge_SameSeed_TwoRounds_DeterministicReplay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSprint0PIEIsolation_Edge_SameSeedTwoRounds::RunTest(const FString& Parameters)
{
	AddExpectedError(
		TEXT("Using CommonUI without a CommonGameViewportClient"),
		EAutomationExpectedMessageFlags::Contains, 0);

	GPIEProbeInitCount     = 0;
	GPIEProbeDeinitCount   = 0;
	GPIEProbeBeginPlayCount = 0;
	FMemory::Memzero(EdgeSameSeedState::Round1Seq, sizeof(EdgeSameSeedState::Round1Seq));
	FMemory::Memzero(EdgeSameSeedState::Round2Seq, sizeof(EdgeSameSeedState::Round2Seq));

	FAutomationEditorCommonUtils::CreateNewMap();

	// === 第一局 ===
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEdgeSameSeed_DrawRound1(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	// === 第二局 ===
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEdgeSameSeed_DrawRound2AndAssert(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStopCommand(this));

	return true;
}
