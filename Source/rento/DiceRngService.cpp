// DiceRngService.cpp
// =============================================================================
// RNG 服务宿主实现（story-001：宿主 + 种子注入 + 通用原语封装）
//                   （story-002：RollDice 执行序铁律 + OnDiceRolled 最小 seam）
//                   （story-003：RandomRange 退化/边界三道门）
//                   （story-004：lazy-init 兜底种子安全 + GetInitialSeed accessor）
//
// 规范依据：
//   - ADR-0004（primary）— 确定性 RNG 服务 + 执行序铁律 + lazy-init 兜底
//   - ADR-0001            — UObject 宿主与生命周期边界
//   - ADR-0003            — 事件总线：OnDiceRolled payload 形态
//   - ADR-0007            — BP/C++ 边界：含 gameplay 随机 → C++ UFUNCTION
//   - control-manifest Foundation Layer §确定性 RNG (ADR-0004)
//   - story-001（dice-rng epic）AC-23/18/13/12c + [Host] AC
//   - story-002（dice-rng epic）AC-1/2/6/7/16c
//   - story-004（dice-rng epic）AC-17/17b
//
// 引擎事实（Sprint0 spike 核验，docs/architecture/sprint0-engine-verification-2026-06-06.md）：
//   ① RandRange 仍走 Min+TruncToInt(FRand()*Range) 浮点中介 —— CONFIRMED（同平台重放安全）
//   ③ FRandomStream() 默构种子恒 0 —— CONFIRMED（UE5.7 RandomStream.h L30-33，lazy-init 兜底安全前提）
//   ④ 5.7 无新官方确定性/网络 RNG 子系统 —— CONFIRMED（手搓单权威流方案 verified）
//   story-004 引擎核实（2026-06-07）：
//   - FRandomStream::GetInitialSeed() const 返回 int32 InitialSeed（RandomStream.h L97-100）
//   - FRandomStream::Initialize(int32 InSeed)：InitialSeed=InSeed; Seed=uint32(InSeed)（L63-67）
//   - FPlatformTime::Cycles() 返回 uint32，Windows 实现为 QueryPerformanceCounter 低 32 位（L32-37）
//   - Cycles()|1u 强制最低位为 1，确保 static_cast<int32> 后 InitialSeed 永远非 0（AC-17b 确定性）
// =============================================================================
#include "DiceRngService.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

// ----------------------------------------------------------------------------
// EnsureSeedInitialized — lazy-init 兜底（story-004，ADR-0004 Guideline 3）
// ----------------------------------------------------------------------------

void UDiceRngService::EnsureSeedInitialized()
{
	// 快速路径：已初始化则直返，无性能开销
	if (bSeedInitialized)
	{
		return;
	}

	// ── lazy-init 兜底路径 ───────────────────────────────────────────────────
	// 规范依据（ADR-0004 Guideline 3 / control-manifest Forbidden）：
	//   禁 FRandomStream() 默构 0——忘 SetSeed 的对局会掷出相同确定序列，
	//   砸穿「骰子不可预测」信任底线，且范围断言查不出。
	//
	// 用 FPlatformTime::Cycles()|1u 兜底：
	//   - Cycles() 返回 uint32（QPC 低 32 位，Windows L32-37）
	//   - |1u 强制最低位为 1，确保转 int32 后 InitialSeed 永远非 0（AC-17b 确定性断言）
	//   - static_cast<int32>：uint32→int32 位模式不变，FRandomStream::Initialize 接受全范围
	//
	// UE_LOG Error（非 ensure）：
	//   本项目既定惯例（DR-001→DR-003 系列）：headless ensure callstack 不稳致
	//   AddExpectedError 计数不可靠，改用 UE_LOG(LogTemp, Error) + 稳定子串。
	//   子串「lazy-init」供测试 AddExpectedError(Contains) 精确匹配。
	//
	// Shipping 不崩：兜底后 bSeedInitialized=true，后续调用走正常抽流路径。
	UE_LOG(LogTemp, Error,
		TEXT("UDiceRngService: 权威流未初始化即调用——lazy-init 兜底启动。"
		     "正确用法：对局开始前必须调用 SetSeed()（ADR-0004 / dice CR-3）。"
		     "Full Vision 重放模式下此路径禁止进入（OQ-3）。"));

	// 兜底种子：Cycles()|1u 保证非 0，static_cast 保留位模式
	const int32 FallbackSeed = static_cast<int32>(FPlatformTime::Cycles() | 1u);
	AuthoritativeStream.Initialize(FallbackSeed);
	bSeedInitialized = true;
}

// ----------------------------------------------------------------------------
// OnWorldBeginPlay — Seed 注入唯一时机（ADR-0001 §2 / ADR-0004）
// ----------------------------------------------------------------------------

void UDiceRngService::OnWorldBeginPlay(UWorld& InWorld)
{
	// 调用基类（设置 bHasCalledBeginPlay 标志，防重复触发 ensure）
	Super::OnWorldBeginPlay(InWorld);

	if (bUseFixedSeed)
	{
		// 测试 / 调试路径：使用固定种子，保证确定性序列可重放
		// 0 是合法种子，非哨兵（ADR-0004 §Key Interfaces，story-001 AC-18）
		// SetSeed 内部会置 bSeedInitialized=true（story-004 追加）
		SetSeed(FixedSeed);
	}
	else
	{
		// ⚠ 故意「未接线」——生产路径（bUseFixedSeed=false）此处不注入 Seed。
		//
		// 这是 story-001 锚定的结构事实：「OnWorldBeginPlay = 唯一合法 Seed 注入时机」。
		// 生产每局不同种子的真实来源为 StartNewGame 玩家配置，
		// 由后续 story（ff-004 / pt-001）在 World BeginPlay 完成后调用 SetSeed() 传入。
		//
		// 防 seed=0 footgun 的 lazy-init 兜底（story-004）：
		// 若调用方忘记调 SetSeed，首次抽流时 EnsureSeedInitialized() 会
		// 用 FPlatformTime::Cycles()|1u 兜底（非确定源，非 0），并记 Error 日志。
		//
		// 不添加此注释的读者可能误判为漏实现 bug——故保留此长注释。
	}
}

// ----------------------------------------------------------------------------
// RollDice — 掷出标准双骰，执行序铁律四步（story-002，ADR-0004 §Key Interfaces）
// ----------------------------------------------------------------------------

FDiceRollResult UDiceRngService::RollDice()
{
	// 执行序铁律（ADR-0004 §Key Interfaces，dice F-4 前提②，control-manifest Required）
	// 顺序严格不可乱——每步语义见下注释。
	//
	// story-004 注：lazy-init 兜底由 RandomRange 内调用的 EnsureSeedInitialized() 处理。
	// 首次 RandomRange(1,6) 时若 bSeedInitialized==false，触发一次 lazy-init（Error 日志）；
	// 第二次 RandomRange(1,6) 时 bSeedInitialized 已 true，直接抽流，不再触发。
	// RollDice 本体无需额外调用 EnsureSeedInitialized()。

	// ① 流抽：两次独立顺序抽取（各推进权威流游标一次）
	//    Die1 先抽，Die2 后抽，顺序固定（「两颗独立 d6」，非「先取 Total 再拆解」）
	//    经 RandomRange 封装（禁旁路 AuthoritativeStream.RandRange 直调，
	//    退化契约由 story-003 在 RandomRange 加守卫，本路径 [1,6] 无退化）
	const int32 Die1 = RandomRange(1, 6);
	const int32 Die2 = RandomRange(1, 6);

	// ② 本地固化：一次性构造完整 payload，不再触碰权威流
	//    Total == Die1 + Die2（构造时不变式）
	//    bIsDouble == (Die1 == Die2)（构造时不变式）
	FDiceRollResult Result;
	Result.Die1      = Die1;
	Result.Die2      = Die2;
	Result.Total     = Die1 + Die2;
	Result.bIsDouble = (Die1 == Die2);

	// ③ 广播固化值（ADR-0003：owner 先同步算定权威结果，再广播事件）
	//    订阅者回调内禁同步调任何抽取 API（重入会插入权威序列，story-005 加完整门控）
	OnDiceRolled.Broadcast(Result);

	// ④ 返回同一固化 Result（绝不广播后重读流）
	//    若此处改为 "return FDiceRollResult{RandomRange(1,6), RandomRange(1,6), ...}"
	//    则会额外消耗 Seed，导致返回值 != payload（AC-16c 必 FAIL），故禁之。
	return Result;
}

// ----------------------------------------------------------------------------
// RandomRange — 从权威流取 [Min, Max] 随机整数（story-003：退化/边界三道门）
// ----------------------------------------------------------------------------

int32 UDiceRngService::RandomRange(int32 Min, int32 Max)
{
	// ── 第一道门：Min==Max 退化早返，不调流（Seed 不推进）────────────────────
	// ADR-0004 Guideline 3 / dice F-2 / F-4④ / control-manifest Guardrail
	// 退化路径不消耗 Seed，守已固化 fixture 序列（story-006 依赖）。
	// 先于 Min>Max 判断：Min==Max 时两条分支互斥，先判 == 即正确短路。
	// story-004 注：此路径不抽流，不触发 EnsureSeedInitialized。
	if (Min == Max)
	{
		return Min;
	}

	// ── 第二道门：Min>Max 非法参数，记 Error，返回 Min，绝不 swap ─────────────
	// ADR-0004 Guideline 3 / control-manifest Forbidden §"Never Min>Max 自动 swap"
	// swap 静默掩盖参数反向 bug。返回 Min 是 fail-safe（明确告知调用方错误）。
	// 本门先于 2^24 检查：Min>Max 时 (int64)Max-(int64)Min+1 为负数或 0，
	// 先早返可避免后续 Range 计算的语义混淆。
	// story-004 注：此路径不抽流，不触发 EnsureSeedInitialized。
	if (Min > Max)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UDiceRngService::RandomRange: Min>Max（Min=%d，Max=%d），"
			     "返回 Min 不 swap。调用方传入参数反向，请检查调用点。"
			     "（ADR-0004 / control-manifest Forbidden）"),
			Min, Max);
		return Min;
	}

	// ── story-004 lazy-init 兜底检查（抽流前置）────────────────────────────────
	// 精确放置点：Min==Max/Min>Max 早返之后，2^24 检查及 RandRange 调用之前。
	// 只有走到此处（真正抽流路径），才触发未初始化兜底。
	// 首次调用时触发 lazy-init 并置 bSeedInitialized=true；后续调用直接跳过。
	EnsureSeedInitialized();

	// ── 第三道门：范围 >= 2^24 精度警告，记 Error，仍直通 ───────────────────
	// ADR-0004 §Key Interfaces / dice F-2 / control-manifest Required
	// RandRange 内部走 FRand()*Range 浮点中介（spike CONFIRMED：UE5.7 RandomStream.h L186-202）。
	// float 尾数 23 位，Range >= 2^24（=16777216）时浮点乘积精度崩溃，均匀性破坏。
	// 使用 int64 中间量防止 (Max-Min+1) 在 int32 层面溢出（如 Max=INT_MAX，Min=0）。
	// MVP 实际跨度极小（骰面 6 / AI 三选一），此路径属异常，降 Full Vision OQ-3 处理。
	const int64 Range = (int64)Max - (int64)Min + 1;
	if (Range >= (1 << 24))  // 2^24 = 16777216
	{
		UE_LOG(LogTemp, Error,
			TEXT("UDiceRngService::RandomRange: Range=%lld >= 2^24=%d，"
			     "FRand() 浮点中介精度崩溃（均匀性破坏）。"
			     "MVP 实际跨度极小，此路径异常，大跨度路径降 Full Vision OQ-3 处理。"
			     "（ADR-0004 / dice F-2）"),
			Range, (1 << 24));
		// 仍直通：告警调用方，不阻断返回——OQ-3 大跨度处理留 Full Vision
	}

	// ── 正常路径：直通权威流 ──────────────────────────────────────────────────
	// 引擎事实①（spike CONFIRMED）：RandRange 走 Min+TruncToInt(FRand()*Range) 浮点中介，
	// 同平台 / 同编译配置重放安全；跨平台 off-by-one 降 Full Vision OQ-3。
	return AuthoritativeStream.RandRange(Min, Max);
}

// ----------------------------------------------------------------------------
// RandomFloat01 — 从权威流取 [0.0, 1.0) 半开区间浮点数
// ----------------------------------------------------------------------------

float UDiceRngService::RandomFloat01()
{
	// story-004 lazy-init 兜底检查（FRand() 抽流前置）
	// 若未经 SetSeed/OnWorldBeginPlay 注入，EnsureSeedInitialized 用 Cycles()|1u 兜底。
	EnsureSeedInitialized();

	// FRandomStream::FRand() 返回 [0.0, 1.0)，1.0 严格排除
	// （ADR-0004 Guideline 3 / story-001 AC-13）
	//
	// OQ-4 登记：下游 floor(f * N) 仅 MVP 小 N（< 2^24）安全。
	// N >= 2^24 时浮点舍入可能使 floor(f*N) 返回 N（越界），
	// 该越界场景在 MVP 不可达，守门登记 OQ-4，story-003 不覆盖此约束。
	return AuthoritativeStream.FRand();
}

// ----------------------------------------------------------------------------
// SetSeed — 设置权威流种子，重置随机序列
// ----------------------------------------------------------------------------

void UDiceRngService::SetSeed(int32 InitialSeed)
{
	// FRandomStream::Initialize 接受 int32 全范围（含 0、负数）
	// 0 是合法种子，非哨兵（ADR-0004 §Key Interfaces / story-001 AC-18 Edge）
	AuthoritativeStream.Initialize(InitialSeed);

	// story-004 追加：置 bSeedInitialized=true，阻止后续 EnsureSeedInitialized 触发 lazy-init。
	// 这是 story-004 实现 lazy-init 兜底 feature 的固有范围——SetSeed 正常路径语义不变，
	// 仅追加一个 flag 标记"已初始化"。
	bSeedInitialized = true;
}

// ----------------------------------------------------------------------------
// GetCurrentSeed — 显式获取当前种子（禁靠 struct 反射，ADR-0004 R5 / OQ-2）
// ----------------------------------------------------------------------------

int32 UDiceRngService::GetCurrentSeed() const
{
	// 显式调用 FRandomStream::GetCurrentSeed() 而非依赖 UPROPERTY 反射序列化。
	// 原因（ADR-0004 OQ-2）：UPROPERTY 序列化可能只持久化 InitialSeed
	// 而丢失滚动 Seed（当前抽取位置），导致续算静默破坏。
	// Seed 序列化（story-008）须经此 accessor + SetSeed() 组合。
	return AuthoritativeStream.GetCurrentSeed();
}

// ----------------------------------------------------------------------------
// GetInitialSeed — 获取初始种子（story-004 AC-17b；FRandomStream::GetInitialSeed()）
// ----------------------------------------------------------------------------

int32 UDiceRngService::GetInitialSeed() const
{
	// FRandomStream::GetInitialSeed() 返回 InitialSeed 成员（int32）——
	// 即 Initialize() 时传入的值（UE5.7 RandomStream.h L97-100）。
	//
	// 与 GetCurrentSeed() 的区别：
	//   - GetCurrentSeed()：返回当前滚动 Seed（抽流后随每次 MutateSeed 推进）
	//   - GetInitialSeed()：返回 Initialize() 传入的原始值（抽流不改变）
	//
	// AC-17b 用途：RollDice() 抽流后 GetCurrentSeed() 已推进；
	//   GetInitialSeed() 读到 lazy-init 写入的 Cycles()|1u 值，
	//   断言 != 0 证明未退化为 FRandomStream 默构 0。
	//
	// 注意：本函数不调用 EnsureSeedInitialized()——不抽流，不应触发 lazy-init。
	return AuthoritativeStream.GetInitialSeed();
}
