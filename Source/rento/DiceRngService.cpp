// DiceRngService.cpp
// =============================================================================
// RNG 服务宿主实现（story-001：宿主 + 种子注入 + 通用原语封装）
//
// 规范依据：
//   - ADR-0004（primary）— 确定性 RNG 服务
//   - ADR-0001            — UObject 宿主与生命周期边界
//   - control-manifest Foundation Layer §确定性 RNG (ADR-0004)
//   - story-001（dice-rng epic）AC-23/18/13/12c + [Host] AC
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
