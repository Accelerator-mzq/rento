// PlayerTurnSubsystem.cpp
// =============================================================================
// UPlayerTurnSubsystem — 回合系统 per-match 宿主实现（story pt-001）
//
// 实现 scope（story pt-001）：
//   - InitializeFromConfig：建队（分配 PlayerId/TokenColor，校验唯一性）+ 调生产定序
//   - AssignTurnOrderByRollTotals：排名纯方法（降序，TC-3 测试 mock 注入点）
//   - PerformInitialTurnOrder：生产定序（走 DiceRngService 权威流，ADR-0004）
//   - ValidateTurnOrderUniqueness：AC-5 TurnOrderIndex 唯一性校验
//   - GetPlayerStates / FindPlayerById：只读访问接口
//
// 规范依据：
//   - story pt-001 AC-1~5，TC-1~4，Implementation Notes 1~7
//   - ADR-0001（per-match UWorldSubsystem，不持状态写语义）
//   - ADR-0004（定序掷骰走骰子3 权威流，禁旁路引擎 RNG）
//   - control-manifest Foundation Layer §宿主与生命周期（ADR-0001）
// =============================================================================

#include "PlayerTurnSubsystem.h"
#include "DiceRngService.h"

// ===========================================================================
// Initialize — 宿主初始化（World BeginPlay 前调用）
// ===========================================================================

void UPlayerTurnSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // 调用基类（UMatchSubsystemBase → UWorldSubsystem 内部初始化）
    Super::Initialize(Collection);

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::Initialize — 回合宿主初始化（per-match World Subsystem）。"
             "World: %s"),
        GetWorld() ? *GetWorld()->GetName() : TEXT("null"));

    // 建队在 InitializeFromConfig（StartNewGame → OnWorldBeginPlay 后），不在此处。
}

// ===========================================================================
// Deinitialize — 宿主销毁（World 销毁时调用，ADR-0001 §3 解绑/清理）
// ===========================================================================

void UPlayerTurnSubsystem::Deinitialize()
{
    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::Deinitialize — 回合宿主销毁，清空 PlayerStates (Num=%d)。"),
        PlayerStates.Num());

    // 清空 TObjectPtr 引用，允许 GC 回收 URentoPlayerState 实例
    PlayerStates.Empty();

    // 调用基类（ADR-0001 §必须调 Super）
    Super::Deinitialize();
}

// ===========================================================================
// InitializeFromConfig — 建队 + 定序主入口
// ===========================================================================

bool UPlayerTurnSubsystem::InitializeFromConfig(const FGameSetupConfig& Config)
{
    const int32 P = Config.Players.Num();

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::InitializeFromConfig — 开始建队，P=%d。"), P);

    // -------------------------------------------------------------------------
    // 1. 清空旧 PlayerStates（幂等，防重复调用）
    // -------------------------------------------------------------------------
    PlayerStates.Empty();

    // -------------------------------------------------------------------------
    // 2. 锁定开局旋钮（story Note 5）
    //    DoublesJailThreshold <= 0 时使用系统默认值 3
    //    MaxTiebreakRounds <= 0 时使用系统默认值 5
    // -------------------------------------------------------------------------
    DoublesJailThreshold = (Config.DoublesJailThreshold > 0)
        ? Config.DoublesJailThreshold : 3;
    MaxTiebreakRounds = (Config.MaxTiebreakRounds > 0)
        ? Config.MaxTiebreakRounds : 5;

    // -------------------------------------------------------------------------
    // 3. 校验 TokenColor 唯一性（预校验，开局前拒绝重复/None）
    //    与 board-data 索引唯一性校验同模式（BoardValidator 惯例）
    // -------------------------------------------------------------------------
    TSet<EPlayerColor> UsedColors;
    for (int32 i = 0; i < P; ++i)
    {
        const FPlayerSetupEntry& Entry = Config.Players[i];

        // EPlayerColor::None 不合法（None=哨兵，不应出现在对局玩家中，AC-5）
        if (Entry.TokenColor == EPlayerColor::None)
        {
            UE_LOG(LogTemp, Error,
                TEXT("UPlayerTurnSubsystem::InitializeFromConfig — "
                     "玩家 [%d] TokenColor=None 非法（None 为哨兵值，不可分配给对局玩家）。"
                     "开局拒绝（AC-5）。"), i);
            PlayerStates.Empty();
            return false;
        }

        // 颜色重复检测
        if (UsedColors.Contains(Entry.TokenColor))
        {
            UE_LOG(LogTemp, Error,
                TEXT("UPlayerTurnSubsystem::InitializeFromConfig — "
                     "玩家 [%d] TokenColor 重复（AC-5 唯一性校验失败）。"
                     "开局拒绝。"), i);
            PlayerStates.Empty();
            return false;
        }
        UsedColors.Add(Entry.TokenColor);
    }

    // -------------------------------------------------------------------------
    // 4. 建队：为每位玩家创建 URentoPlayerState，复制字段
    // -------------------------------------------------------------------------
    PlayerStates.Reserve(P);
    for (int32 i = 0; i < P; ++i)
    {
        const FPlayerSetupEntry& Entry = Config.Players[i];

        // 用 NewObject<>（GC 管理，UObject 规范，禁 new/delete）
        URentoPlayerState* State = NewObject<URentoPlayerState>(this);
        check(State);

        // 分配唯一 PlayerId（0..P-1，开局后恒定，GDD CR-1）
        State->PlayerId = i;

        // 复制大厅配置字段
        State->DisplayName = Entry.DisplayName;
        State->TokenColor  = Entry.TokenColor;
        State->bIsAI       = Entry.bIsAI;

        // bIsAI=false 时 AIDifficulty 强制为 None（AC-1 字段默认值契约）
        State->AIDifficulty = Entry.bIsAI ? Entry.AIDifficulty : EAIDifficulty::None;

        // 确保 ConsecutiveDoubles=0（GDD CR-2，定序不累加，AC-1/TC-1）
        State->ConsecutiveDoubles = 0;

        // TurnOrderIndex 初始化为 -1（定序前未分配，防止 ValidateTurnOrderUniqueness 误判）
        State->TurnOrderIndex = -1;

        PlayerStates.Add(State);
    }

    // -------------------------------------------------------------------------
    // 5. 执行生产定序（走 DiceRngService 权威流，ADR-0004）
    // -------------------------------------------------------------------------
    UDiceRngService* DiceService = GetWorld()
        ? GetWorld()->GetSubsystem<UDiceRngService>() : nullptr;

    if (DiceService)
    {
        // pt-001 seed 接线契约（DiceRngService.h L257 / ADR-0004 重放意图）：
        //   生产每局种子真实来源 = StartNewGame 玩家配置（Config.RandomSeed），
        //   由本系统在 OnWorldBeginPlay 之后、定序掷骰之前注入权威流。
        //   这使开局定序可重放，并消除未注入种子时 DiceService 的 lazy-init 兜底 Error。
        //   0 是合法种子（dice story-004 AC-18，非哨兵），确定性测试可复现。
        DiceService->SetSeed(Config.RandomSeed);

        // 生产路径：经权威流掷骰定序
        PerformInitialTurnOrder(DiceService);
    }
    else
    {
        // 纯防御兜底分支（实证恒不触发）：DiceService 与本系统同为 per-match UWorldSubsystem，
        // 引擎在 Game/PIE World（含 headless UWorld::CreateWorld(EWorldType::Game)）下自动创建二者
        // ——pt-001 测试实证：修 seed 前本路径从未走到，DiceService 的 lazy-init Error 真实触发，
        //   坐实 DiceService 在 headless 恒存在（故上方 if 分支恒命中）。
        // 保留本 else 仅为防御未来非常规 World 装配（无 DiceService 的极端场景）：退化为座位序号顺序，
        // 使 ValidateTurnOrderUniqueness 仍能产出唯一 0..P-1（不静默崩）。
        UE_LOG(LogTemp, Warning,
            TEXT("UPlayerTurnSubsystem::InitializeFromConfig — "
                 "UDiceRngService 不可用（非常规 World 装配，防御路径），"
                 "定序退化为座位序号顺序（seat 0 = TurnOrderIndex 0）。"));

        // 退化定序：seat i → TurnOrderIndex i（测试可覆盖）
        for (int32 i = 0; i < PlayerStates.Num(); ++i)
        {
            PlayerStates[i]->TurnOrderIndex = i;
        }
    }

    // -------------------------------------------------------------------------
    // 6. AC-5 唯一性校验（TurnOrderIndex 0..P-1 唯一，AC-5）
    // -------------------------------------------------------------------------
    if (!ValidateTurnOrderUniqueness())
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::InitializeFromConfig — "
                 "TurnOrderIndex 唯一性校验失败，开局拒绝（AC-5）。"));
        PlayerStates.Empty();
        return false;
    }

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::InitializeFromConfig — 建队完成，PlayerStates.Num()=%d。"),
        PlayerStates.Num());

    return true;
}

// ===========================================================================
// AssignTurnOrderByRollTotals — 排名纯方法（TC-3 mock 注入点）
// ===========================================================================

void UPlayerTurnSubsystem::AssignTurnOrderByRollTotals(const TArray<int32>& RollTotals)
{
    const int32 P = PlayerStates.Num();

    // 防御：rolls 数量与玩家数须一致
    if (RollTotals.Num() != P)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::AssignTurnOrderByRollTotals — "
                 "RollTotals.Num()=%d 与 PlayerStates.Num()=%d 不匹配，排名跳过。"),
            RollTotals.Num(), P);
        return;
    }

    if (P == 0)
    {
        return;
    }

    // -------------------------------------------------------------------------
    // 按点数和降序排名，高者 TurnOrderIndex=0（先手）
    //
    // 算法：构建 seat index 数组，按对应 roll 值降序排序，
    //       排序后第 rank 位的 seat 获得 TurnOrderIndex=rank。
    //
    // TC-3 验证：rolls=[5,9,3]（seat0=5, seat1=9, seat2=3）
    //   降序排序后：[seat1(9), seat0(5), seat2(3)]
    //   seat1→TurnOrderIndex=0, seat0→TurnOrderIndex=1, seat2→TurnOrderIndex=2
    // -------------------------------------------------------------------------
    TArray<int32> SeatIndices;
    SeatIndices.Reserve(P);
    for (int32 i = 0; i < P; ++i)
    {
        SeatIndices.Add(i);
    }

    // 按点数和降序排序 seat 序列（稳定排序不保证，平手处理归 story-002）
    SeatIndices.Sort([&RollTotals](int32 A, int32 B)
    {
        return RollTotals[A] > RollTotals[B];  // 降序
    });

    // 按排名分配 TurnOrderIndex
    for (int32 Rank = 0; Rank < P; ++Rank)
    {
        const int32 SeatIdx = SeatIndices[Rank];
        PlayerStates[SeatIdx]->TurnOrderIndex = Rank;
    }

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::AssignTurnOrderByRollTotals — 定序完成（P=%d）。"), P);
}

// ===========================================================================
// PerformInitialTurnOrder — 生产定序（走骰子3 权威流，ADR-0004）
// ===========================================================================

void UPlayerTurnSubsystem::PerformInitialTurnOrder(UDiceRngService* DiceService)
{
    check(DiceService);

    const int32 P = PlayerStates.Num();

    if (P == 0)
    {
        return;
    }

    // -------------------------------------------------------------------------
    // 对每位玩家掷一次骰，取点数和（GDD CR-2 定序形态）：
    //   - 走骰子3 唯一权威 FRandomStream（ADR-0004 Required）
    //   - 定序只取 Total（FDiceRollResult.Total）
    //   - 忽略 bIsDouble（GDD CR-2："定序掷骰只取点数和、忽略 bIsDouble"）
    //   - ConsecutiveDoubles 保持 0（GDD CR-2："不进双点链"）
    // -------------------------------------------------------------------------
    TArray<int32> RollTotals;
    RollTotals.Reserve(P);

    for (int32 i = 0; i < P; ++i)
    {
        // 调 DiceRngService::RollDice（执行序铁律：流抽→固化→广播→返回同一固化值）
        // 定序不进双点链，ConsecutiveDoubles 保持 0（已在建队时置 0）
        const FDiceRollResult Roll = DiceService->RollDice();
        RollTotals.Add(Roll.Total);

        UE_LOG(LogTemp, Verbose,
            TEXT("UPlayerTurnSubsystem::PerformInitialTurnOrder — "
                 "玩家 [%d] 定序掷骰：Die1=%d, Die2=%d, Total=%d (bIsDouble=%s，定序忽略)。"),
            i, Roll.Die1, Roll.Die2, Roll.Total,
            Roll.bIsDouble ? TEXT("true") : TEXT("false"));
    }

    // 喂排名纯方法（与 TC-3 测试注入路径复用同一逻辑，保证生产/测试一致）
    AssignTurnOrderByRollTotals(RollTotals);
}

// ===========================================================================
// ValidateTurnOrderUniqueness — AC-5 唯一性校验
// ===========================================================================

bool UPlayerTurnSubsystem::ValidateTurnOrderUniqueness() const
{
    const int32 P = PlayerStates.Num();
    if (P == 0)
    {
        return true;  // 空局允许（P=0 边界）
    }

    // TurnOrderIndex 须精确覆盖 0..P-1 各一次
    // 方法：计数 bitmap，复杂度 O(P)
    TArray<bool> Seen;
    Seen.Init(false, P);

    for (int32 i = 0; i < P; ++i)
    {
        const int32 Idx = PlayerStates[i]->TurnOrderIndex;

        // 越界（<0 或 >=P）
        if (Idx < 0 || Idx >= P)
        {
            UE_LOG(LogTemp, Error,
                TEXT("UPlayerTurnSubsystem::ValidateTurnOrderUniqueness — "
                     "PlayerStates[%d].TurnOrderIndex=%d 越界（有效范围 0..%d-1）。"),
                i, Idx, P);
            return false;
        }

        // 重复
        if (Seen[Idx])
        {
            UE_LOG(LogTemp, Error,
                TEXT("UPlayerTurnSubsystem::ValidateTurnOrderUniqueness — "
                     "TurnOrderIndex=%d 重复（AC-5 唯一性违反）。"), Idx);
            return false;
        }
        Seen[Idx] = true;
    }

    return true;
}

// ===========================================================================
// 只读访问接口
// ===========================================================================

const TArray<TObjectPtr<URentoPlayerState>>& UPlayerTurnSubsystem::GetPlayerStates() const
{
    return PlayerStates;
}

URentoPlayerState* UPlayerTurnSubsystem::FindPlayerById(int32 PlayerId) const
{
    for (const TObjectPtr<URentoPlayerState>& State : PlayerStates)
    {
        if (State && State->PlayerId == PlayerId)
        {
            return State.Get();
        }
    }
    return nullptr;
}
