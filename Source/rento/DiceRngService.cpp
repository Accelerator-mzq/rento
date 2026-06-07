// DiceRngService.cpp
// =============================================================================
// RNG 服务宿主实现（story-001：宿主 + 种子注入 + 通用原语封装）
//                   （story-002：RollDice 执行序铁律 + OnDiceRolled 最小 seam）
//
// 规范依据：
//   - ADR-0004（primary）— 确定性 RNG 服务 + 执行序铁律
//   - ADR-0001            — UObject 宿主与生命周期边界
//   - ADR-0003            — 事件总线：OnDiceRolled payload 形态
//   - ADR-0007            — BP/C++ 边界：含 gameplay 随机 → C++ UFUNCTION
//   - control-manifest Foundation Layer §确定性 RNG (ADR-0004)
//   - story-001（dice-rng epic）AC-23/18/13/12c + [Host] AC
//   - story-002（dice-rng epic）AC-1/2/6/7/16c
//
// 引擎事实（Sprint0 spike 核验，docs/architecture/sprint0-engine-verification-2026-06-06.md）：
//   ① RandRange 仍走 Min+TruncToInt(FRand()*Range) 浮点中介 —— CONFIRMED（同平台重放安全）
//   ③ FRandomStream() 默构种子恒 0 —— CONFIRMED（lazy-init 兜底安全前提，属 story-004）
//   ④ 5.7 无新官方确定性/网络 RNG 子系统 —— CONFIRMED（手搓单权威流方案 verified）
// =============================================================================
#include "DiceRngService.h"
#include "Engine/World.h"

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
		// 防 seed=0 footgun 的 lazy-init 兜底（未 SetSeed 路径用 FPlatformTime::Cycles()）
		// 属 story-004，本 story 不实现。
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
// RandomRange — 从权威流取 [Min, Max] 随机整数（本 story 正常路径，直通）
// ----------------------------------------------------------------------------

int32 UDiceRngService::RandomRange(int32 Min, int32 Max)
{
	// 本 story 只实现正常路径：直通 FRandomStream::RandRange
	//
	// 退化契约（story-003 实现，本 story 不写，防止模糊归属）：
	//   - Min==Max：early-return Min，不调流（Seed 不推进）
	//   - Min>Max ：ensure + UE_LOG(Warning)，返回 Min，绝不 swap
	//   - Range >= 2^24：ensure（浮点中介精度崩）
	//
	// 引擎事实①（spike CONFIRMED）：RandRange 走 FRand() 浮点中介，
	// 同平台 / 同编译配置重放安全；跨平台 off-by-one 降 Full Vision OQ-3。
	return AuthoritativeStream.RandRange(Min, Max);
}

// ----------------------------------------------------------------------------
// RandomFloat01 — 从权威流取 [0.0, 1.0) 半开区间浮点数
// ----------------------------------------------------------------------------

float UDiceRngService::RandomFloat01()
{
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
