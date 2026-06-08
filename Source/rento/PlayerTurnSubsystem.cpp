// PlayerTurnSubsystem.cpp
// =============================================================================
// UPlayerTurnSubsystem — 回合系统 per-match 宿主实现（story pt-001 / pt-002 / pt-003）
//
// pt-001 scope（开局入口 + 建队 + 定序）：
//   - InitializeFromConfig：建队（AC-10 P 边界 + TokenColor 唯一）+ 调定序
//   - AssignTurnOrderByRollTotals：排名纯方法（降序，TC-3 测试 mock 注入点）
//   - ResolveInitialTurnOrderWithTiebreak：生产定序 + F-4 平手重掷（走 DiceRngService 权威流，ADR-0004）
//   - ValidateTurnOrderUniqueness：AC-5 TurnOrderIndex 唯一性校验
//   - GetPlayerStates / FindPlayerById：只读访问接口
//
// pt-002 scope（回合阶段状态机，禁 Latent，F-3/F-4/AC-10）：
//   - SetPhase：状态机推进唯一入口（设置 CurrentPhase + 广播 OnPhaseChanged）
//   - StartTurn：行动权移交，路由正常/在狱分支
//   - ProcessRollResult：消费 bIsDouble，驱动 F-3 双点链计数
//   - AdvanceFromMovePhase / AdvanceFromResolvePhase：阶段推进
//   - EndTurn：PostRollAction → TurnEnd，判定额外回合 or 移交（pt-003 接管完整 F-1 寻路）
//   - AdvanceFromJailTurn：JailTurn → TurnEnd
//   - ShouldGoToJail：F-3 纯函数（static，AC-7 可单测）
//
// pt-003 scope（F-1 NextActivePlayer + 破产移出 + OnGameWon + IResolveBankruptcy DI）：
//   - NextActivePlayer：静态纯函数，守卫先行，入口规范化（AC-1/2/3）
//   - ActivePlayerCount：静态计数辅助（AC-4；纯计数，绝非胜负裁决器）
//   - SetBankruptcyResolver：注入 IResolveBankruptcy（DI 注入面）
//   - HandlePlayerBankruptcy：调注入接口读返回值，驱动 bIsBankrupt + OnGameWon
//   - FindNextActivePlayerId：改为调真 F-1（NextActivePlayer，守卫先行）
//
// 禁 Latent 强约束（ADR-0001 §4）：
//   绝不使用 FTimerHandle / Blueprint Delay / WaitForEvent。
//   状态机仅通过 ETurnPhase 枚举字段 + delegate 推进，保证读档 switch(CurrentPhase) 重入。
//
// 规范依据：
//   - story pt-001 AC-1~5，story pt-002 AC-1~11，story pt-003 AC-1~8
//   - ADR-0001（per-match UWorldSubsystem；禁 Latent；枚举字段 + delegate 推进）
//   - ADR-0003（OnGameWon 单一事件源：回合2 触发，9 return-only）
//   - ADR-0004（骰子权威流，F-3 时序契约）
//   - ADR-0007（回合状态机落 C++；[Logic] BLOCKING AC 被测逻辑落 C++）
//   - control-manifest Foundation Layer
// =============================================================================

#include "PlayerTurnSubsystem.h"
#include "DiceRngService.h"
#include "LandingResolverInterface.h"   // ILandingResolver（story-006 落地结算 DI 接缝）
#include "PawnMovementInterface.h"      // IPawnMover（story-006 移动 seam）

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
// SetPhase — 状态机推进唯一入口（pt-002 禁 Latent，ADR-0001 §4）
// ===========================================================================

void UPlayerTurnSubsystem::SetPhase(ETurnPhase NewPhase)
{
    // 记录阶段变更（Verbose 级别，避免 hot path 噪音）
    UE_LOG(LogTemp, Verbose,
        TEXT("UPlayerTurnSubsystem::SetPhase — %d → %d（ActivePlayerId=%d）。"),
        static_cast<int32>(CurrentPhase),
        static_cast<int32>(NewPhase),
        CurrentActivePlayerId);

    // 捕获旧阶段（story-004 AC-2 payload）
    const ETurnPhase OldPhase = CurrentPhase;

    // 设置枚举字段（禁 Latent，ADR-0001 §4 钉死）
    CurrentPhase = NewPhase;

    // 广播 FPhaseChangedInfo（story-004 AC-2/AC-3）：
    //   ConsecutiveDoubles 取当前行动玩家（无活跃玩家=0），供 HUD 双点链呈现。
    const URentoPlayerState* CurPS = FindPlayerById(CurrentActivePlayerId);
    FPhaseChangedInfo PhaseInfo;
    PhaseInfo.OldPhase           = OldPhase;
    PhaseInfo.NewPhase           = NewPhase;
    PhaseInfo.ConsecutiveDoubles = CurPS ? CurPS->ConsecutiveDoubles : 0;
    OnPhaseChanged.Broadcast(PhaseInfo);
}

// ===========================================================================
// InitializeFromConfig — 建队 + 定序主入口（pt-001 + pt-002 AC-10 P 边界）
// ===========================================================================

bool UPlayerTurnSubsystem::InitializeFromConfig(const FGameSetupConfig& Config)
{
    const int32 P = Config.Players.Num();

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::InitializeFromConfig — 开始建队，P=%d。"), P);

    // -------------------------------------------------------------------------
    // pt-002 AC-10 P 边界校验（补充 pt-001 遗漏的防守层）
    //   P<2 → 至少需要 2 名玩家（GDD Edge Cases L388）
    //   P>4 → MVP 上限 4（GDD CR-7 L207，AC-10）
    // -------------------------------------------------------------------------
    if (P < 2)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::InitializeFromConfig — "
                 "P=%d 不足（最小 2 人），开局拒绝（AC-10）。"), P);
        return false;
    }
    if (P > 4)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::InitializeFromConfig — "
                 "P=%d 超出 MVP 上限 4，开局拒绝（AC-10）。"), P);
        return false;
    }

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
                     "玩家 [%d] TokenColor=None 非法（None 为哨兵值），"
                     "开局拒绝（AC-5）。"), i);
            PlayerStates.Empty();
            return false;
        }

        // 颜色重复检测
        if (UsedColors.Contains(Entry.TokenColor))
        {
            UE_LOG(LogTemp, Error,
                TEXT("UPlayerTurnSubsystem::InitializeFromConfig — "
                     "玩家 [%d] TokenColor 重复（AC-5 唯一性校验失败），"
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

        // TurnOrderIndex 初始化为 -1（定序前未分配，防 ValidateTurnOrderUniqueness 误判）
        State->TurnOrderIndex = -1;

        PlayerStates.Add(State);
    }

    // -------------------------------------------------------------------------
    // 5. 执行定序（pt-002：走 F-4 平手重掷完整路径，ADR-0004）
    // -------------------------------------------------------------------------
    UDiceRngService* DiceService = GetWorld()
        ? GetWorld()->GetSubsystem<UDiceRngService>() : nullptr;

    if (DiceService)
    {
        // 注入种子（ADR-0004 接线契约）
        DiceService->SetSeed(Config.RandomSeed);

        // pt-002 F-4 完整定序（含平手重掷，AC-11）
        // 不传 InjectRolls → 走生产路径（DiceService 权威流）
        ResolveInitialTurnOrderWithTiebreak(DiceService);
    }
    else
    {
        // 防御兜底：退化为座位序号顺序（参 pt-001 注释）
        UE_LOG(LogTemp, Warning,
            TEXT("UPlayerTurnSubsystem::InitializeFromConfig — "
                 "UDiceRngService 不可用（防御路径），"
                 "定序退化为座位序号顺序。"));
        for (int32 i = 0; i < PlayerStates.Num(); ++i)
        {
            PlayerStates[i]->TurnOrderIndex = i;
        }
        // 退化座位序=席位裁定性质（story-004 AC-5 标志）
        bLastOrderResolvedBySeatTiebreak = true;
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

    // -------------------------------------------------------------------------
    // 6b. 广播 OnTurnOrderResolved（story-004 AC-5：无平手正常定序也广播；席位裁定后也广播）
    //     OrderedPlayerIds 按 TurnOrderIndex 排布；bResolvedBySeatTiebreak 来自定序函数标志。
    // -------------------------------------------------------------------------
    {
        FTurnOrderResult OrderResult;
        OrderResult.OrderedPlayerIds.SetNum(PlayerStates.Num());
        for (const TObjectPtr<URentoPlayerState>& S : PlayerStates)
        {
            if (S && OrderResult.OrderedPlayerIds.IsValidIndex(S->TurnOrderIndex))
            {
                OrderResult.OrderedPlayerIds[S->TurnOrderIndex] = S->PlayerId;
            }
        }
        OrderResult.bResolvedBySeatTiebreak = bLastOrderResolvedBySeatTiebreak;
        OnTurnOrderResolved.Broadcast(OrderResult);
    }

    // -------------------------------------------------------------------------
    // 7. 置初始阶段（pt-002：对局开始，状态机处于 TurnStart 待命）
    //    不广播——此时无活跃玩家，调用方通过 StartTurn(PlayerId) 正式启动第一回合
    // -------------------------------------------------------------------------
    CurrentPhase = ETurnPhase::TurnStart;
    CurrentActivePlayerId = -1;
    bLastRollWasDouble = false;
    bSentToJailThisTurnInternal = false;

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::InitializeFromConfig — 建队完成，P=%d，初始阶段 TurnStart。"),
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
    // TC-3 验证：rolls=[5,9,3]（seat0=5, seat1=9, seat2=3）
    //   降序：[seat1(9), seat0(5), seat2(3)]
    //   seat1→TurnOrderIndex=0, seat0→TurnOrderIndex=1, seat2→TurnOrderIndex=2
    // -------------------------------------------------------------------------
    TArray<int32> SeatIndices;
    SeatIndices.Reserve(P);
    for (int32 i = 0; i < P; ++i)
    {
        SeatIndices.Add(i);
    }

    // 按点数和降序排序 seat 序列
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
        TEXT("UPlayerTurnSubsystem::AssignTurnOrderByRollTotals — 排名完成（P=%d）。"), P);
}

// ===========================================================================
// ResolveInitialTurnOrderWithTiebreak — F-4 平手重掷完整定序（pt-002 AC-11）
//   注：pt-001 的 PerformInitialTurnOrder（无平手语义）已被本方法取代并删除（零调用）。
//   生产路径与测试均走本方法；AssignTurnOrderByRollTotals 仍为纯排名子步骤（TC-3 注入点）。
// ===========================================================================

void UPlayerTurnSubsystem::ResolveInitialTurnOrderWithTiebreak(
    UDiceRngService* DiceService,
    const TArray<int32>& InjectRolls)
{
    const int32 P = PlayerStates.Num();
    if (P == 0)
    {
        return;
    }

    // story-004 AC-5：默认非席位裁定（RNG 定序）；下方席位升序裁定分支置 true
    bLastOrderResolvedBySeatTiebreak = false;

    // -------------------------------------------------------------------------
    // 注入 rolls 游标（测试注入路径：每 P 个值对应一轮所有玩家）
    // -------------------------------------------------------------------------
    int32 InjectCursor = 0;

    // -------------------------------------------------------------------------
    // 第一轮：每位玩家各掷一次，收集初始点数
    // -------------------------------------------------------------------------
    TArray<int32> CurrentRolls;
    CurrentRolls.Reserve(P);

    for (int32 i = 0; i < P; ++i)
    {
        int32 RollVal = 0;
        if (InjectRolls.Num() > InjectCursor)
        {
            // 使用注入值（测试路径，确定性）
            RollVal = InjectRolls[InjectCursor++];
        }
        else if (DiceService)
        {
            // 生产路径：走权威流
            RollVal = DiceService->RollDice().Total;
        }
        CurrentRolls.Add(RollVal);
    }

    // -------------------------------------------------------------------------
    // 用 TArray 追踪每个 seat 已确定的 rank（-1 = 尚未确定）
    // -------------------------------------------------------------------------
    TArray<int32> AssignedRank;
    AssignedRank.Init(-1, P);

    // -------------------------------------------------------------------------
    // 迭代：最多 MaxTiebreakRounds 轮（含初始轮，round=0 即第一轮已取得 rolls）
    //
    // GDD L370 off-by-one 澄清：
    //   实际重掷发生在 round ∈ [0, MaxTiebreakRounds-1]（共 MaxTiebreakRounds 轮）。
    //   达 MaxTiebreakRounds 轮后剩余平手进入席位裁定。
    // -------------------------------------------------------------------------
    for (int32 Round = 0; Round < MaxTiebreakRounds; ++Round)
    {
        // ------------------------------------------------------------------
        // 1. 按当前 rolls 降序将 seat 排序，找出已可确定 rank 的 seat
        // ------------------------------------------------------------------
        // 构建 (roll, seat) 对，按 roll 降序排序
        TArray<TPair<int32, int32>> RollSeatPairs;
        RollSeatPairs.Reserve(P);

        for (int32 i = 0; i < P; ++i)
        {
            if (AssignedRank[i] < 0)  // 尚未确定 rank 的 seat 才参与本轮
            {
                RollSeatPairs.Add(TPair<int32, int32>(CurrentRolls[i], i));
            }
        }

        // 按 roll 降序排序（同 roll 保持稳定性，后续平手判断依赖此顺序）
        RollSeatPairs.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B)
        {
            if (A.Key != B.Key)
            {
                return A.Key > B.Key;  // 降序
            }
            return A.Value < B.Value;  // roll 相等时 seat 升序（稳定裁定）
        });

        // ------------------------------------------------------------------
        // 2. 计算本轮已确定 rank 的下一起始偏移
        //    （= 当前已分配 rank 数量，即本轮排名从此开始）
        // ------------------------------------------------------------------
        int32 RankOffset = 0;
        for (int32 i = 0; i < P; ++i)
        {
            if (AssignedRank[i] >= 0)
            {
                ++RankOffset;
            }
        }

        // ------------------------------------------------------------------
        // 3. 分组找平手子组，确定哪些可分配 rank，哪些需要继续重掷
        // ------------------------------------------------------------------
        int32 LocalPos = 0;
        const int32 TotalRemaining = RollSeatPairs.Num();

        while (LocalPos < TotalRemaining)
        {
            // 取当前 roll 值，找连续相同的 tie group
            const int32 TieRoll = RollSeatPairs[LocalPos].Key;
            int32 GroupStart = LocalPos;
            int32 GroupEnd = LocalPos;

            while (GroupEnd + 1 < TotalRemaining &&
                   RollSeatPairs[GroupEnd + 1].Key == TieRoll)
            {
                ++GroupEnd;
            }

            const int32 GroupSize = GroupEnd - GroupStart + 1;

            if (GroupSize == 1)
            {
                // 唯一值，直接分配 rank
                const int32 Seat = RollSeatPairs[GroupStart].Value;
                AssignedRank[Seat] = RankOffset;
                ++RankOffset;
            }
            // GroupSize > 1 且是最后一轮时由后续席位裁定处理
            // GroupSize > 1 且非最后一轮时本组继续参与下轮重掷（不分配 rank）

            LocalPos = GroupEnd + 1;
        }

        // ------------------------------------------------------------------
        // 4. 检查是否所有 seat 都已分配 rank
        // ------------------------------------------------------------------
        bool bAllAssigned = true;
        for (int32 i = 0; i < P; ++i)
        {
            if (AssignedRank[i] < 0)
            {
                bAllAssigned = false;
                break;
            }
        }

        if (bAllAssigned)
        {
            // 定序完成，应用 AssignedRank
            for (int32 i = 0; i < P; ++i)
            {
                PlayerStates[i]->TurnOrderIndex = AssignedRank[i];
            }
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::ResolveInitialTurnOrderWithTiebreak — "
                     "定序完成（Round=%d，无剩余平手）。"), Round);
            return;
        }

        // ------------------------------------------------------------------
        // 5. 仍有平手 seat → 若本轮为最后一轮，进行席位升序裁定；否则继续重掷
        // ------------------------------------------------------------------
        const bool bIsLastRound = (Round == MaxTiebreakRounds - 1);
        if (bIsLastRound)
        {
            // 经席位裁定（story-004 AC-5 标志 → OnTurnOrderResolved.bResolvedBySeatTiebreak=true）
            bLastOrderResolvedBySeatTiebreak = true;
            // 席位升序裁定（子组内按席位升序，GDD L376 澄清）
            // 当前仍在 RollSeatPairs 中且尚未分配 rank 的 seat，
            // 按 (roll 降序, seat 升序) 已排好——此时各平手子组内按 seat 升序分配
            int32 CurRankOffset2 = 0;
            for (int32 i = 0; i < P; ++i)
            {
                if (AssignedRank[i] >= 0)
                {
                    ++CurRankOffset2;
                }
            }

            // 重新整理未分配的 seat，按子组内席位升序裁定
            // 做法：遍历 RollSeatPairs（已按 roll 降序+seat 升序排好），
            // 连续相同 roll 的组内，按 seat 升序分配 rank（自然顺序已满足）
            for (const TPair<int32, int32>& Pair : RollSeatPairs)
            {
                const int32 Seat = Pair.Value;
                if (AssignedRank[Seat] < 0)
                {
                    // 找该 seat 所在子组的起始 rank（= 已分配 rank + 该子组在剩余中的偏移）
                    // 由于 RollSeatPairs 已排好 (roll 降序, seat 升序)，直接顺序分配
                    AssignedRank[Seat] = CurRankOffset2++;
                }
            }

            // 应用最终 AssignedRank
            for (int32 i = 0; i < P; ++i)
            {
                PlayerStates[i]->TurnOrderIndex = AssignedRank[i];
            }
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::ResolveInitialTurnOrderWithTiebreak — "
                     "达 MaxTiebreakRounds=%d，剩余平手按席位升序裁定。"),
                MaxTiebreakRounds);
            return;
        }

        // ------------------------------------------------------------------
        // 6. 未达上限 → 对仍未分配 rank 的 seat 重掷
        // ------------------------------------------------------------------
        for (int32 i = 0; i < P; ++i)
        {
            if (AssignedRank[i] < 0)
            {
                // 未确定 rank 的 seat 重掷
                int32 NewRoll = 0;
                if (InjectRolls.Num() > InjectCursor)
                {
                    NewRoll = InjectRolls[InjectCursor++];
                }
                else if (DiceService)
                {
                    NewRoll = DiceService->RollDice().Total;
                }
                CurrentRolls[i] = NewRoll;
            }
        }
    }

    // -------------------------------------------------------------------------
    // 兜底：确保所有 seat 都有 rank（不应到达此处，防御性补全）
    // -------------------------------------------------------------------------
    int32 FallbackRank = 0;
    for (int32 i = 0; i < P; ++i)
    {
        if (AssignedRank[i] < 0)
        {
            AssignedRank[i] = FallbackRank++;
        }
    }
    for (int32 i = 0; i < P; ++i)
    {
        PlayerStates[i]->TurnOrderIndex = AssignedRank[i];
    }
}

// ===========================================================================
// ValidateTurnOrderUniqueness — AC-5 唯一性校验
// ===========================================================================

bool UPlayerTurnSubsystem::ValidateTurnOrderUniqueness() const
{
    const int32 P = PlayerStates.Num();
    if (P == 0)
    {
        return true;  // 空局允许
    }

    // TurnOrderIndex 须精确覆盖 0..P-1 各一次（bitmap 法，O(P)）
    TArray<bool> Seen;
    Seen.Init(false, P);

    for (int32 i = 0; i < P; ++i)
    {
        const int32 Idx = PlayerStates[i]->TurnOrderIndex;

        // 越界
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
// ShouldGoToJail — F-3 纯函数（static，AC-7 可单测）
// ===========================================================================

/*static*/
bool UPlayerTurnSubsystem::ShouldGoToJail(int32 ConsecutiveCount, int32 Threshold)
{
    // 用 >= 非 ==（额外防御层，GDD L354 澄清：
    //   正常路径 ConsecutiveDoubles ∈ [0, threshold]，>=逻辑等价==；
    //   异常路径内存损坏/读档越级，>= 兜住越级情形）
    return ConsecutiveCount >= Threshold;
}

// ===========================================================================
// StartTurn — 启动指定玩家回合（pt-002 状态机入口）
// ===========================================================================

void UPlayerTurnSubsystem::StartTurn(int32 PlayerId)
{
    // 设置当前行动玩家
    CurrentActivePlayerId = PlayerId;

    // 重置本回合状态
    bLastRollWasDouble = false;
    bSentToJailThisTurnInternal = false;

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::StartTurn — PlayerId=%d 开始回合。"), PlayerId);

    // 广播 OnTurnStarted（story-004 AC-2，回合级事件先于阶段级 OnPhaseChanged(TurnStart)）
    {
        const URentoPlayerState* StartingPS = FindPlayerById(PlayerId);
        FTurnStartedInfo TSInfo;
        TSInfo.PlayerId = PlayerId;
        TSInfo.bIsAI    = StartingPS ? StartingPS->bIsAI : false;
        OnTurnStarted.Broadcast(TSInfo);
    }

    // TurnStart 阶段广播
    SetPhase(ETurnPhase::TurnStart);

    // 路由：在狱 → JailTurn；正常 → RollPhase（GDD (b) L224）
    URentoPlayerState* PlayerState = FindPlayerById(PlayerId);
    if (PlayerState && PlayerState->bIsInJail)
    {
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::StartTurn — PlayerId=%d 在狱，路由到 JailTurn。"),
            PlayerId);
        SetPhase(ETurnPhase::JailTurn);
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::StartTurn — PlayerId=%d 正常，路由到 RollPhase。"),
            PlayerId);
        SetPhase(ETurnPhase::RollPhase);
    }
}

// ===========================================================================
// ProcessRollResult — 消费 bIsDouble，驱动 F-3 双点链（pt-002 AC-4/5/6/7/9）
// ===========================================================================

void UPlayerTurnSubsystem::ProcessRollResult(bool bIsDouble, bool& OutSentToJail)
{
    OutSentToJail = false;

    // 非法转移检测（AC-2）：仅 RollPhase 可调用
    if (CurrentPhase != ETurnPhase::RollPhase)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::ProcessRollResult — "
                 "非法转移：当前阶段为 %d（非 RollPhase=1），"
                 "ProcessRollResult 被拒绝，阶段保持不变（AC-2）。"),
            static_cast<int32>(CurrentPhase));
        return;
    }

    // 获取当前玩家 State（校验 bIsInJail 与 bIsBankrupt）
    URentoPlayerState* PlayerState = FindPlayerById(CurrentActivePlayerId);

    // ------------------------------------------------------------------
    // F-3 时序契约（GDD L356，pt-002 Note 4）：
    //   ① 检测 bIsDouble
    //   ② 若 T → ConsecutiveDoubles+=1（先累加）
    //   ③ 判 DoublesToJail（>= DoublesJailThreshold）
    //      T → 送监狱（取消移动，无额外回合，ConsecutiveDoubles 归零）
    //      F → 正常移动（推进 MovePhase）
    //   ④ 若 bIsDouble==F → ConsecutiveDoubles=0
    // ------------------------------------------------------------------
    if (bIsDouble)
    {
        // ② 先累加
        if (PlayerState)
        {
            PlayerState->ConsecutiveDoubles += 1;
        }

        // ③ 判 DoublesToJail
        const int32 ConsecNow = PlayerState ? PlayerState->ConsecutiveDoubles : 0;
        if (ShouldGoToJail(ConsecNow, DoublesJailThreshold))
        {
            // 触发 F-3 送监狱：取消移动，无额外回合，ConsecutiveDoubles 归零
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::ProcessRollResult — "
                     "F-3 触发：ConsecutiveDoubles=%d >= Threshold=%d，"
                     "PlayerId=%d 送监狱。"),
                ConsecNow, DoublesJailThreshold, CurrentActivePlayerId);

            if (PlayerState)
            {
                PlayerState->ConsecutiveDoubles = 0;
            }
            bLastRollWasDouble = false;    // 无额外回合（AC-5 / AC-9）
            bSentToJailThisTurnInternal = true;
            OutSentToJail = true;

            // 跳过 MovePhase/ResolvePhase，直接 PostRollAction → TurnEnd
            // （本系统只发送监狱意图，实际入狱由事件格7负责，pt-002 不改 bIsInJail）
            SetPhase(ETurnPhase::PostRollAction);
        }
        else
        {
            // 正常双点：记录双点标记，推进 MovePhase
            bLastRollWasDouble = true;
            SetPhase(ETurnPhase::MovePhase);
        }
    }
    else
    {
        // ④ 非双点 → ConsecutiveDoubles=0（GDD L356 步骤④）
        if (PlayerState)
        {
            PlayerState->ConsecutiveDoubles = 0;
        }
        bLastRollWasDouble = false;
        SetPhase(ETurnPhase::MovePhase);
    }
}

// ===========================================================================
// AdvanceFromMovePhase — MovePhase → ResolvePhase
// ===========================================================================

void UPlayerTurnSubsystem::AdvanceFromMovePhase()
{
    // 非法转移检测（AC-2）
    if (CurrentPhase != ETurnPhase::MovePhase)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::AdvanceFromMovePhase — "
                 "非法转移：当前阶段为 %d（非 MovePhase=2），"
                 "拒绝推进，阶段保持不变（AC-2）。"),
            static_cast<int32>(CurrentPhase));
        return;
    }
    SetPhase(ETurnPhase::ResolvePhase);
}

// ===========================================================================
// AdvanceFromResolvePhase — ResolvePhase → PostRollAction
// ===========================================================================

void UPlayerTurnSubsystem::AdvanceFromResolvePhase()
{
    // 非法转移检测（AC-2）
    if (CurrentPhase != ETurnPhase::ResolvePhase)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::AdvanceFromResolvePhase — "
                 "非法转移：当前阶段为 %d（非 ResolvePhase=3），"
                 "拒绝推进，阶段保持不变（AC-2）。"),
            static_cast<int32>(CurrentPhase));
        return;
    }
    SetPhase(ETurnPhase::PostRollAction);
}

// ===========================================================================
// EndTurn — PostRollAction → TurnEnd，判定额外回合 or 移交（pt-002 AC-4/6/9）
// ===========================================================================

void UPlayerTurnSubsystem::EndTurn(bool bSentToJailThisTurn)
{
    // 非法转移检测（AC-2）
    if (CurrentPhase != ETurnPhase::PostRollAction)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::EndTurn — "
                 "非法转移：当前阶段为 %d（非 PostRollAction=4），"
                 "拒绝推进，阶段保持不变（AC-2）。"),
            static_cast<int32>(CurrentPhase));
        return;
    }

    // TurnEnd 广播
    SetPhase(ETurnPhase::TurnEnd);

    // ------------------------------------------------------------------
    // 判定额外回合（CR-4 / AC-4 / AC-6 / AC-9）：
    //   bLastRollWasDouble=true
    //   && 本回合未被送监狱（bSentToJailThisTurn=false 且内部标记也 false）
    //   && 当前玩家未破产
    // ------------------------------------------------------------------
    const bool bEffectiveSentToJail = bSentToJailThisTurn || bSentToJailThisTurnInternal;

    URentoPlayerState* PlayerState = FindPlayerById(CurrentActivePlayerId);
    const bool bBankrupt = PlayerState ? PlayerState->bIsBankrupt : false;

    // 出狱双点不触发额外回合（AC-9）：
    //   若本回合玩家 bIsInJail=true（入狱状态进入 JailTurn），双点出狱不算 CR-4
    //   注意：ProcessRollResult 不处理 JailTurn 路径，JailTurn 走 AdvanceFromJailTurn
    //   此处 bLastRollWasDouble 由 ProcessRollResult 设置（仅 RollPhase 分支），
    //   JailTurn 路径不经 ProcessRollResult，故 bLastRollWasDouble=false 自然成立

    // 计算额外回合判据（story-004 AC-4，命名变量供 OnTurnEnded 广播 + 分支共用；控制流与原 if 等价）
    const bool bGrantsExtraTurn = bLastRollWasDouble && !bEffectiveSentToJail && !bBankrupt;

    // 广播 OnTurnEnded（PlayerId=回合结束者=移交前 CurrentActivePlayerId，AC-2/AC-4）
    {
        FTurnEndedInfo TEInfo;
        TEInfo.PlayerId         = CurrentActivePlayerId;
        TEInfo.bGrantsExtraTurn = bGrantsExtraTurn;
        OnTurnEnded.Broadcast(TEInfo);
    }

    if (bGrantsExtraTurn)
    {
        // 双点额外回合：同玩家继续，ConsecutiveDoubles 不归零（AC-6）
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::EndTurn — "
                 "双点额外回合：PlayerId=%d 继续，ConsecutiveDoubles=%d（不归零，AC-6）。"),
            CurrentActivePlayerId,
            PlayerState ? PlayerState->ConsecutiveDoubles : 0);

        // 重置本回合临时状态（保留 ConsecutiveDoubles）
        bLastRollWasDouble = false;
        bSentToJailThisTurnInternal = false;

        // 回到同玩家 RollPhase（不通过 StartTurn，直接推进，不重置 ConsecutiveDoubles）
        SetPhase(ETurnPhase::RollPhase);
    }
    else
    {
        // 无额外回合：移交下一玩家
        // ConsecutiveDoubles 归零（AC-6：行动权实际移交时归零）
        if (PlayerState)
        {
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::EndTurn — "
                     "移交下一玩家：PlayerId=%d ConsecutiveDoubles 归零（AC-6）。"),
                CurrentActivePlayerId);
            PlayerState->ConsecutiveDoubles = 0;
        }

        // 找下一个未破产玩家（story-003 接管完整 F-1；F-1 守卫 |A|<=1 返回 INDEX_NONE）
        const int32 NextPlayerId = FindNextActivePlayerId();
        if (NextPlayerId != INDEX_NONE)
        {
            StartTurn(NextPlayerId);
        }
        else
        {
            // F-1 返回 INDEX_NONE（|A|<=1）：对局结束，停止轮转
            // OnGameWon 由 HandlePlayerBankruptcy 触发（AC-8 封堵：此处不独立广播）
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::EndTurn — "
                     "F-1 返回 INDEX_NONE（|A|<=1），停止轮转。"
                     "OnGameWon 由 HandlePlayerBankruptcy 触发（AC-8 封堵）。"));
        }
    }
}

// ===========================================================================
// AdvanceFromJailTurn — JailTurn → TurnEnd（在狱玩家出狱决策完成）
// ===========================================================================

void UPlayerTurnSubsystem::AdvanceFromJailTurn(bool bRemainsInJail)
{
    // 非法转移检测（AC-2）
    if (CurrentPhase != ETurnPhase::JailTurn)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::AdvanceFromJailTurn — "
                 "非法转移：当前阶段为 %d（非 JailTurn=6），"
                 "拒绝推进，阶段保持不变（AC-2）。"),
            static_cast<int32>(CurrentPhase));
        return;
    }

    // AC-8：留狱时 JailTurnsServed+=1；出狱时不增加
    URentoPlayerState* PlayerState = FindPlayerById(CurrentActivePlayerId);
    if (PlayerState)
    {
        if (bRemainsInJail)
        {
            PlayerState->JailTurnsServed += 1;
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::AdvanceFromJailTurn — "
                     "PlayerId=%d 留狱，JailTurnsServed=%d。"),
                CurrentActivePlayerId, PlayerState->JailTurnsServed);
        }
        else
        {
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::AdvanceFromJailTurn — "
                     "PlayerId=%d 出狱，JailTurnsServed 不增加（=%d）。"),
                CurrentActivePlayerId, PlayerState->JailTurnsServed);
        }
    }

    // 推进到 TurnEnd，然后由 EndTurn 的逻辑处理移交
    // 注意：JailTurn 路径不经 ProcessRollResult，bLastRollWasDouble=false，
    //   EndTurn 自然走「移交下一玩家」路径（无额外回合，AC-9 出狱双点不累加）
    SetPhase(ETurnPhase::TurnEnd);

    // JailTurn → TurnEnd 后移交下一玩家（ConsecutiveDoubles 归零，AC-6）
    if (PlayerState)
    {
        PlayerState->ConsecutiveDoubles = 0;
    }
    bLastRollWasDouble = false;
    bSentToJailThisTurnInternal = false;

    // 找下一玩家（调完整 F-1，F-1 守卫 |A|<=1 返回 INDEX_NONE）
    const int32 NextPlayerId = FindNextActivePlayerId();
    if (NextPlayerId != INDEX_NONE)
    {
        StartTurn(NextPlayerId);
    }
    else
    {
        // F-1 返回 INDEX_NONE（|A|<=1）：对局结束，停止轮转
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::AdvanceFromJailTurn — "
                 "F-1 返回 INDEX_NONE，停止轮转（对局结束）。"));
    }
}

// ===========================================================================
// NextActivePlayer — F-1 静态纯函数（story pt-003 AC-1/2/3）
//
// 守卫先行（GDD L289-293）：
//   |A|=0 → INDEX_NONE + ensureMsgf（正常不可达）
//   |A|=1 → INDEX_NONE（不返回唯一存活者；对局已由 CR-6 终结）
//   |A|≥2 → 入口规范化后枚举
//
// 入口规范化（GDD L295-297）：
//   cur_safe = ((cur % P) + P) % P（欧几里得取模，保证 cur_safe ∈ [0,P-1]）
//   k = min{k ∈ [1,P-1] | (cur_safe+k)%P ∈ ActiveIndices}
// ===========================================================================

/*static*/
int32 UPlayerTurnSubsystem::NextActivePlayer(int32 cur, int32 P, const TSet<int32>& ActiveIndices)
{
    const int32 ActiveCount = ActiveIndices.Num();

    // -------------------------------------------------------------------------
    // 守卫 1：|A|=0 → 正常流程不可达，ensureMsgf + 返回 INDEX_NONE
    // -------------------------------------------------------------------------
    if (ActiveCount == 0)
    {
        // |A|=0 正常流程不可达（MVP 单线程，退化局由破产9内部兜底）。
        // 项目约定（active.md logged-decision）：验证型错误路径用 UE_LOG(Error) 替 ensure
        // ——headless Automation 中 ensure callstack 产生整片 Error 行致 AddExpectedError 计数不可靠。
        UE_LOG(LogTemp, Error,
            TEXT("NextActivePlayer: ActiveIndices is empty (|A|=0), "
                 "unreachable in normal game flow. cur=%d, P=%d. 返回 INDEX_NONE。"),
            cur, P);
        return INDEX_NONE;
    }

    // -------------------------------------------------------------------------
    // 守卫 2：|A|=1 → 对局已由 CR-6 终结，不返回唯一存活者索引
    // 守卫判据仅看 |A|，与 cur 是否∈A 无关（GDD L290 澄清）
    // -------------------------------------------------------------------------
    if (ActiveCount == 1)
    {
        return INDEX_NONE;
    }

    // -------------------------------------------------------------------------
    // dev 诊断：检查原始 cur 是否在合法范围（规范化前，GDD L315）
    // ⚠ 此越界是 handled 情形（下方欧几里得取模规范化兜住），故用 Warning 而非 Error：
    //    越界 cur 是上游异常输入信号，但已被安全处理；Warning 不致 Automation FAIL，
    //    TC3 验证规范化输出（无需 AddExpectedError 体操）。
    // -------------------------------------------------------------------------
    if (!(cur >= 0 && cur < P))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("NextActivePlayer: cur=%d is out of range [0,%d), "
                 "will normalize via Euclidean modulo (handled)。"),
            cur, P - 1);
    }

    // -------------------------------------------------------------------------
    // 入口规范化（GDD L295）：欧几里得取模保证 cur_safe ∈ [0,P-1]
    // 即使 cur 为负数，((cur%P)+P)%P 仍正确（C++ 负数取模可为负，+P 再%P 修正）
    // -------------------------------------------------------------------------
    const int32 CurSafe = ((cur % P) + P) % P;

    // -------------------------------------------------------------------------
    // 枚举：k = min{k ∈ [1,P-1] | (cur_safe+k)%P ∈ ActiveIndices}
    // k 从 1 开始（跳过自身），最大 P-1（|A|≥2 保证必有命中）
    // -------------------------------------------------------------------------
    for (int32 k = 1; k < P; ++k)
    {
        const int32 Candidate = (CurSafe + k) % P;
        if (ActiveIndices.Contains(Candidate))
        {
            return Candidate;
        }
    }

    // -------------------------------------------------------------------------
    // 不可达兜底（|A|≥2 且枚举 P-1 个候选，必有命中）
    // -------------------------------------------------------------------------
    ensureMsgf(false,
        TEXT("NextActivePlayer: No active player found after full enumeration. "
             "cur=%d, P=%d, ActiveCount=%d. This should be unreachable."),
        cur, P, ActiveCount);
    return INDEX_NONE;
}

// ===========================================================================
// ActivePlayerCount — F-2 静态计数辅助（story pt-003 AC-4）
//
// 数 BankruptFlags 中 false 的个数（false=活跃，true=已破产）。
// ⚠ 纯计数辅助，绝非胜负裁决器（GDD L331 / story-003 §2）
// ===========================================================================

/*static*/
int32 UPlayerTurnSubsystem::ActivePlayerCount(const TArray<bool>& BankruptFlags)
{
    // 遍历标志，计数 false（未破产=活跃）
    int32 Count = 0;
    for (bool bBankrupt : BankruptFlags)
    {
        if (!bBankrupt)
        {
            ++Count;
        }
    }

    // AC-4：全员破产（APC=0）正常流程不可达。项目约定用 UE_LOG(Error) 替 ensure（headless 可吞）。
    if (Count <= 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ActivePlayerCount: All players are bankrupt (APC=0), "
                 "unreachable in normal game flow."));
    }

    return Count;
}

// ===========================================================================
// SetBankruptcyResolver — DI 注入接口（story pt-003）
// ===========================================================================

void UPlayerTurnSubsystem::SetBankruptcyResolver(TSharedPtr<IResolveBankruptcy> Resolver)
{
    BankruptcyResolver = Resolver;

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::SetBankruptcyResolver — "
             "注入 IResolveBankruptcy: %s"),
        Resolver.IsValid() ? TEXT("有效") : TEXT("nullptr（已清除）"));
}

// ===========================================================================
// HandlePlayerBankruptcy — 破产移出 + OnGameWon 触发（story pt-003 AC-5/6/7/8）
//
// 执行顺序（CR-6 / story-003 §5/6）：
//   1. 调注入的 IResolveBankruptcy::ResolveBankruptcy（return-only，绝不回调）
//   2. bDebtorEliminated=true → 置 debtor.bIsBankrupt=true + 移出轮转
//   3. WinnerId!=INDEX_NONE 且 !bGameWon → 广播 OnGameWon(WinnerId) 一次 + bGameWon=true
//
// ⚠ AC-8 封堵 2↔9：本方法是 OnGameWon 唯一触发来源
//   正常 TurnEnd→F-1 移交路径绝不广播（即便 APC==1 也不触发）
// ===========================================================================

void UPlayerTurnSubsystem::HandlePlayerBankruptcy(int32 DebtorId, int32 CreditorId)
{
    // -------------------------------------------------------------------------
    // 防御：注入接口必须有效（生产中 bankruptcy epic 保证注入；测试中 stub 注入）
    // -------------------------------------------------------------------------
    if (!BankruptcyResolver.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::HandlePlayerBankruptcy — "
                 "BankruptcyResolver 未注入（nullptr），"
                 "无法处理破产事件（DebtorId=%d, CreditorId=%d）。"
                 "请在对局初始化后注入 IResolveBankruptcy 实现。"),
            DebtorId, CreditorId);
        return;
    }

    // -------------------------------------------------------------------------
    // 1. 计算注入前的活跃玩家数（debtor 排除后的快照值）
    //    按 GDD CR-6：破产9 在 activePlayersSnapshot 上算 APC，显式排除 debtor
    //    本系统构建快照传给接口，不依赖9 算 APC——让接口实现自由裁决
    // -------------------------------------------------------------------------
    // 构建活跃玩家数（排除 debtor）：数 bIsBankrupt=false 且 PlayerId!=DebtorId 的数量
    int32 ActiveCountForResolver = 0;
    for (const TObjectPtr<URentoPlayerState>& State : PlayerStates)
    {
        if (State && !State->bIsBankrupt && State->PlayerId != DebtorId)
        {
            ++ActiveCountForResolver;
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::HandlePlayerBankruptcy — "
             "DebtorId=%d, CreditorId=%d, ActiveCountForResolver=%d。"),
        DebtorId, CreditorId, ActiveCountForResolver);

    // -------------------------------------------------------------------------
    // 2. 调注入的 IResolveBankruptcy（return-only，绝不回调，AC-6 AC-8 安全阀）
    // -------------------------------------------------------------------------
    const FBankruptcyResolution Result = BankruptcyResolver->ResolveBankruptcy(
        DebtorId, CreditorId, ActiveCountForResolver);

    // -------------------------------------------------------------------------
    // 3. 据 bDebtorEliminated 置 bIsBankrupt + 移出轮转（GDD CR-6）
    // -------------------------------------------------------------------------
    if (Result.bDebtorEliminated)
    {
        URentoPlayerState* DebtorState = FindPlayerById(DebtorId);
        if (DebtorState)
        {
            // 时序：先写 bIsBankrupt（先同步算定权威结果），再触发事件（ADR-0003 结算同步先行）
            // story-005 AC-4：经受控写接口 SetBankrupt(true) 写入，而非直写字段——
            // 确保 SetBankrupt 是唯一路径，破产9 不直写（return-only 物理防环）。
            DebtorState->SetBankrupt(true);

            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::HandlePlayerBankruptcy — "
                     "DebtorId=%d bIsBankrupt 已经 SetBankrupt(true) 置位（移出轮转，"
                     "story-005 AC-4 受控写路径）。"),
                DebtorId);
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("UPlayerTurnSubsystem::HandlePlayerBankruptcy — "
                     "找不到 DebtorId=%d 对应的 PlayerState，SetBankrupt 未调用。"),
                DebtorId);
        }
    }

    // -------------------------------------------------------------------------
    // 4. 据 WinnerId 广播 OnGameWon（边沿触发，bGameWon 守卫，AC-7）
    //
    // ⚠ AC-8 封堵：此处是系统内 OnGameWon 的唯一触发路径。
    //   正常 TurnEnd→F-1 移交路径（无本方法调用）即便 APC==1 也绝不广播。
    // -------------------------------------------------------------------------
    if (Result.WinnerId != INDEX_NONE)
    {
        if (!bGameWon)
        {
            // 进入 Winner 终态（边沿触发，先置标志防重入）
            bGameWon = true;

            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::HandlePlayerBankruptcy — "
                     "广播 OnGameWon(WinnerId=%d)（首次触发，bGameWon 已置 true）。"),
                Result.WinnerId);

            // 广播（ADR-0003：结算同步先行——bIsBankrupt 已写，OnGameWon 后随）
            OnGameWon.Broadcast(Result.WinnerId);
        }
        else
        {
            // 已进 Winner 终态，边沿守卫拦截（AC-7）
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::HandlePlayerBankruptcy — "
                     "WinnerId=%d 被边沿守卫拦截（bGameWon=true 已进 Winner 终态，"
                     "AC-7 幂等保证）。"),
                Result.WinnerId);
        }
    }
    else
    {
        // WinnerId==INDEX_NONE → 对局继续，不触发（AC-6 第二分支）
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::HandlePlayerBankruptcy — "
                 "WinnerId==INDEX_NONE，对局继续，OnGameWon 不触发。"));
    }
}

// ===========================================================================
// FindNextActivePlayerId — 找下一个未破产玩家（story-003 接管完整 F-1）
//
// 构建 ActiveIndices TSet（TurnOrderIndex 空间），调 static NextActivePlayer。
// F-1 返回 INDEX_NONE（|A|<=1）时返回 INDEX_NONE（轮转停止，对局结束）。
// ===========================================================================

int32 UPlayerTurnSubsystem::FindNextActivePlayerId() const
{
    const int32 P = PlayerStates.Num();
    if (P == 0)
    {
        return INDEX_NONE;
    }

    // -------------------------------------------------------------------------
    // 找当前活跃玩家的 TurnOrderIndex（F-1 的 cur 参数）
    // -------------------------------------------------------------------------
    int32 CurrentTurnOrder = -1;
    for (const TObjectPtr<URentoPlayerState>& State : PlayerStates)
    {
        if (State && State->PlayerId == CurrentActivePlayerId)
        {
            CurrentTurnOrder = State->TurnOrderIndex;
            break;
        }
    }

    if (CurrentTurnOrder < 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UPlayerTurnSubsystem::FindNextActivePlayerId — "
                 "CurrentActivePlayerId=%d 找不到对应 TurnOrderIndex，"
                 "退化为 cur=0。"),
            CurrentActivePlayerId);
        CurrentTurnOrder = 0;
    }

    // -------------------------------------------------------------------------
    // 构建 ActiveIndices：未破产玩家的 TurnOrderIndex 集合
    // -------------------------------------------------------------------------
    TSet<int32> ActiveIndices;
    for (const TObjectPtr<URentoPlayerState>& State : PlayerStates)
    {
        if (State && !State->bIsBankrupt && State->TurnOrderIndex >= 0)
        {
            ActiveIndices.Add(State->TurnOrderIndex);
        }
    }

    // -------------------------------------------------------------------------
    // 调 F-1 静态纯函数（守卫先行，入口规范化）
    // -------------------------------------------------------------------------
    const int32 NextTurnOrder = NextActivePlayer(CurrentTurnOrder, P, ActiveIndices);

    if (NextTurnOrder == INDEX_NONE)
    {
        // F-1 返回 INDEX_NONE（|A|<=1）：对局结束，轮转停止
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::FindNextActivePlayerId — "
                 "F-1 返回 INDEX_NONE（|A|=%d），对局结束，轮转停止。"),
            ActiveIndices.Num());
        return INDEX_NONE;
    }

    // -------------------------------------------------------------------------
    // 从 TurnOrderIndex 反查 PlayerId
    // -------------------------------------------------------------------------
    for (const TObjectPtr<URentoPlayerState>& State : PlayerStates)
    {
        if (State && State->TurnOrderIndex == NextTurnOrder)
        {
            return State->PlayerId;
        }
    }

    // 不可达兜底
    UE_LOG(LogTemp, Error,
        TEXT("UPlayerTurnSubsystem::FindNextActivePlayerId — "
             "F-1 返回 NextTurnOrder=%d 但找不到对应 PlayerState（内部错误）。"),
        NextTurnOrder);
    return INDEX_NONE;
}

// ===========================================================================
// GetCurrentPhase — 获取当前阶段
// ===========================================================================

ETurnPhase UPlayerTurnSubsystem::GetCurrentPhase() const
{
    return CurrentPhase;
}

// ===========================================================================
// GetCurrentActivePlayerId — 获取当前行动玩家 ID
// ===========================================================================

int32 UPlayerTurnSubsystem::GetCurrentActivePlayerId() const
{
    return CurrentActivePlayerId;
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

// ===========================================================================
// pt-006: ConsumeRollResult — 消费完整 FDiceRollResult（LOCKED A / AC-1）
//
// 执行顺序（LOCKED 设计决策 A）：
//   1. 写当前行动玩家 CurrentRollContext = Result（holder 写，回合2 = owner）
//   2. 调现有 ProcessRollResult(Result.bIsDouble, OutSentToJail) 复用 F-3 逻辑
//      （ProcessRollResult 内有 RollPhase 守卫，非法阶段早返不执行）
// ===========================================================================

void UPlayerTurnSubsystem::ConsumeRollResult(const FDiceRollResult& Result, bool& OutSentToJail)
{
    OutSentToJail = false;

    // 阶段守卫：仅 RollPhase 合法（由内部调用的 ProcessRollResult 守卫负责）
    // 提前检查以避免写 holder 后 ProcessRollResult 又因非法阶段拒绝执行（不一致状态）
    if (CurrentPhase != ETurnPhase::RollPhase)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::ConsumeRollResult — "
                 "非法阶段：当前阶段为 %d（非 RollPhase=1），"
                 "ConsumeRollResult 被拒绝，holder 不更新（AC-2 对齐）。"),
            static_cast<int32>(CurrentPhase));
        return;
    }

    // 步骤 1：写当前行动玩家的 holder（回合2 = 逻辑 owner，直写字段合法）
    // C++ 直写 UPROPERTY(BlueprintReadOnly) 字段：BlueprintReadOnly 只限 BP 图写，
    // C++ 层无限制（ADR-0007 / story-006 LOCKED A）。
    URentoPlayerState* PlayerState = FindPlayerById(CurrentActivePlayerId);
    if (PlayerState)
    {
        // 正常程写路径：ConsumeRollResult 直写（非 SetCurrentRollContext setter）
        // setter 唯一合法调用方仍为事件格(7)，此处 C++ 直写不破坏 setter 语义
        PlayerState->CurrentRollContext = Result;

        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::ConsumeRollResult — "
                 "holder 写入：PlayerId=%d, Die1=%d, Die2=%d, Total=%d, bIsDouble=%d。"),
            CurrentActivePlayerId,
            Result.Die1, Result.Die2, Result.Total,
            Result.bIsDouble ? 1 : 0);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UPlayerTurnSubsystem::ConsumeRollResult — "
                 "找不到 CurrentActivePlayerId=%d 对应 PlayerState，holder 写入跳过。"),
            CurrentActivePlayerId);
    }

    // 步骤 2：复用现有 ProcessRollResult 驱动 F-3 双点链 + 阶段推进
    // ProcessRollResult 内部有 RollPhase 守卫（上方已提前校验，此处一定在 RollPhase）
    ProcessRollResult(Result.bIsDouble, OutSentToJail);
}

// ===========================================================================
// pt-006: GetCurrentRollContext — PULL accessor（AC-2）
// ===========================================================================

FDiceRollResult UPlayerTurnSubsystem::GetCurrentRollContext() const
{
    // 读当前行动玩家的 CurrentRollContext
    const URentoPlayerState* PlayerState = FindPlayerById(CurrentActivePlayerId);
    if (PlayerState)
    {
        return PlayerState->CurrentRollContext;
    }

    // 无活跃玩家：返回零值默认 FDiceRollResult（Die1=0/Die2=0/Total=0/bIsDouble=false）
    UE_LOG(LogTemp, Verbose,
        TEXT("UPlayerTurnSubsystem::GetCurrentRollContext — "
             "无活跃玩家（CurrentActivePlayerId=%d），返回零值 FDiceRollResult。"),
        CurrentActivePlayerId);
    return FDiceRollResult{};
}

// ===========================================================================
// pt-006: GetCurrentRollTotal — PULL 快捷路径（AC-2）
// ===========================================================================

int32 UPlayerTurnSubsystem::GetCurrentRollTotal() const
{
    return GetCurrentRollContext().Total;
}

// ===========================================================================
// pt-006: SetLandingResolver — DI 注入 ILandingResolver
// ===========================================================================

void UPlayerTurnSubsystem::SetLandingResolver(TSharedPtr<ILandingResolver> Resolver)
{
    LandingResolver = Resolver;

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::SetLandingResolver — "
             "注入 ILandingResolver: %s"),
        Resolver.IsValid() ? TEXT("有效") : TEXT("nullptr（已清除）"));
}

// ===========================================================================
// pt-006: SetPawnMover — DI 注入 IPawnMover
// ===========================================================================

void UPlayerTurnSubsystem::SetPawnMover(TSharedPtr<IPawnMover> Mover)
{
    PawnMover = Mover;

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::SetPawnMover — "
             "注入 IPawnMover: %s"),
        Mover.IsValid() ? TEXT("有效") : TEXT("nullptr（已清除）"));
}

// ===========================================================================
// pt-006: OrchestrateMove — 程间非重入蹦床（LOCKED D / AC-47）
//
// 蹦床机制（杜绝同步嵌套重入）：
//   - 顶层调用（bInOrchestration=false）进入 while 循环，逐程发起 Advance；
//     每程 Advance 同步触发 HandlePawnLanded（→ResolveArrival），监听者可能
//     在回调内再次调 OrchestrateMove 请求下一程。
//   - 回调内的 OrchestrateMove 调用（bInOrchestration=true）只排队
//     （置 bPendingNextLeg/PendingLegSteps）并立即返回，绝不在回调栈内递归 Advance。
//   - 顶层 while 循环消费 bPendingNextLeg，在前一程 Advance 完全返回后发起下一程。
//
// 结构不变式：每程 Advance 都由顶层蹦床循环发起，发起时不处于任何落地回调内
//   → spy 的 bInLandedCallback==false（AC-47）。
// 变异自检：若去掉 bInOrchestration 排队、改为回调内直接递归 Advance，
//   则第二程在 bInLandedCallback==true 时发起 → TC-3 断言 FAIL（非 vacuous）。
// ===========================================================================

void UPlayerTurnSubsystem::OrchestrateMove(int32 Steps)
{
    // 排队本程请求（顶层与回调内调用统一经此排队语义）
    PendingLegSteps = Steps;
    bPendingNextLeg = true;

    // 若已在蹦床循环中（被某程落地回调栈内调用）：仅排队，立即返回，不递归发起
    if (bInOrchestration)
    {
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::OrchestrateMove — "
                 "落地回调内请求下一程（Steps=%d），排队延迟至当前程返回后由蹦床发起（非重入）。"),
            Steps);
        return;
    }

    // 顶层蹦床循环：逐程发起 Advance，直到无待发起程
    bInOrchestration = true;
    while (bPendingNextLeg)
    {
        bPendingNextLeg = false;
        const int32 LegSteps = PendingLegSteps;

        if (!PawnMover.IsValid())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("UPlayerTurnSubsystem::OrchestrateMove — "
                     "PawnMover 未注入，无法发起 Advance(Steps=%d)；蹦床退出。"),
                LegSteps);
            break;
        }

        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::OrchestrateMove — "
                 "蹦床发起一程 Advance(PlayerId=%d, Steps=%d)。"),
            CurrentActivePlayerId, LegSteps);

        // 同步发起：Advance 内同步触发 HandlePawnLanded（→ResolveArrival），
        // 监听者可能在回调内调 OrchestrateMove 请求下一程（仅排队，见上方分支）。
        PawnMover->Advance(CurrentActivePlayerId, LegSteps);
    }
    bInOrchestration = false;
}

// ===========================================================================
// pt-006: HandlePawnLanded — public 落地回调（LOCKED D / AC-47）
//
// 在前一程 Advance 的同步调用栈内被调用，仅委派 ResolveArrival 做本程落地结算路由。
// 不在此发起下一程——下一程由 OrchestrateMove 蹦床在本回调返回后发起（结构性非重入）。
// ===========================================================================

void UPlayerTurnSubsystem::HandlePawnLanded(EArrivalContext ArrivalContext)
{
    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::HandlePawnLanded — "
             "PlayerId=%d, ArrivalContext=%d（0=DiceMove, 1=SentToJail）。"),
        CurrentActivePlayerId,
        static_cast<int32>(ArrivalContext));

    // 仅执行本程落地结算路由；下一程发起归 OrchestrateMove 蹦床（非重入）
    ResolveArrival(ArrivalContext);
}

// ===========================================================================
// pt-006: ResolveArrival — 据 EArrivalContext 路由落地结算（LOCKED C / AC-4）
//
// 路由规则（GDD Edge Cases L392 / AC-46）：
//   SentToJail → 跳过全部落地结算分支（DecideBuyProperty/SettleRent 均不调）
//                → 推进 PostRollAction
//   DiceMove    → 调 ILandingResolver::DecideBuyProperty 恰 1 次（MVP 均视为无主地产路径）
//                → 推进 PostRollAction
//
// ⚠ Out of Scope 严守：
//   已有主地产→SettleRent 路由、事件格结算、完整地产状态查询归各 epic；
//   本 story 只实现 SentToJail 抑制 + DiceMove DecideBuyProperty seam。
// ===========================================================================

void UPlayerTurnSubsystem::ResolveArrival(EArrivalContext ArrivalContext)
{
    if (ArrivalContext == EArrivalContext::SentToJail)
    {
        // SentToJail 路径：跳过全部落地结算（GDD Edge Cases L392 / AC-46 抑制）
        // 进/出狱规则归事件格(7)；本 story 只保证「不进结算分支」
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::ResolveArrival — "
                 "SentToJail：跳过全部落地结算分支（AC-46 抑制）。"
                 "进/出狱状态更新归事件格(7)。"));

        // 直接推进 PostRollAction → TurnEnd（标准路径）
        SetPhase(ETurnPhase::PostRollAction);
    }
    else
    {
        // DiceMove（及未来其他正常落地上下文）：执行买地决策 seam
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::ResolveArrival — "
                 "DiceMove：调 DecideBuyProperty（PlayerId=%d）。"),
            CurrentActivePlayerId);

        // MVP seam：调 ILandingResolver::DecideBuyProperty 恰 1 次
        // （本 story 均视为落无主地产；已有主地产→SettleRent 路由归 property/economy epic）
        if (LandingResolver.IsValid())
        {
            // 获取当前玩家落点（MVP：读 CurrentTileIndex）
            int32 TileIndex = 0;
            const URentoPlayerState* PlayerState = FindPlayerById(CurrentActivePlayerId);
            if (PlayerState)
            {
                TileIndex = PlayerState->CurrentTileIndex;
            }

            const bool bWillBuy = LandingResolver->DecideBuyProperty(
                CurrentActivePlayerId, TileIndex);

            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::ResolveArrival — "
                     "DecideBuyProperty(PlayerId=%d, TileIndex=%d) = %d。"),
                CurrentActivePlayerId, TileIndex, bWillBuy ? 1 : 0);
        }
        else
        {
            // 未注入 LandingResolver：MVP 阶段无买地逻辑（软约束）
            UE_LOG(LogTemp, Verbose,
                TEXT("UPlayerTurnSubsystem::ResolveArrival — "
                     "LandingResolver 未注入，跳过 DecideBuyProperty seam（MVP 无买地逻辑）。"));
        }

        // 推进 PostRollAction
        SetPhase(ETurnPhase::PostRollAction);
    }
}

// ===========================================================================
// pt-007 簇 A：SetAIDecisionMaker + RunAiPostRollActions
// ===========================================================================

/**
 * 注入 IAIDecisionMaker（AI 决策 hook DI，pt-007 AC-3）。
 *
 * 仿 SetBankruptcyResolver / SetLandingResolver 模式，TSharedPtr 持有。
 * nullptr = 清除注入（RunAiPostRollActions 记 Error + EndTurn 兜底）。
 */
void UPlayerTurnSubsystem::SetAIDecisionMaker(TSharedPtr<IAIDecisionMaker> DecisionMaker)
{
    // 直接赋值，TSharedPtr 自动管理生命周期（非 UObject，GC 不追踪，TSharedPtr 正确释放）
    AIDecisionMaker = DecisionMaker;

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::SetAIDecisionMaker — "
             "AIDecisionMaker %s。"),
        AIDecisionMaker.IsValid() ? TEXT("已注入") : TEXT("已清除（nullptr）"));
}

/**
 * AI PostRollAction 执行路径（pt-007 AC-3 / AC-37d）。
 *
 * 执行流程（LOCKED 设计决策 C）：
 *   ① 阶段守卫：仅 PostRollAction 合法（非则 UE_LOG Error + return，G2 约束）
 *   ② 调 AIDecisionMaker->DecidePostRollActions(Snapshot) 取动作列表
 *   ③ 逐 FTurnAction 执行 — 本簇最小占位：计数 + UE_LOG
 *      （真实动作执行 + 可行性重校验归簇 B/C，Out of Scope 严守）
 *   ④ 执行完（含空数组 []）→ 调 EndTurn(false) 推进 TurnEnd + 移交下一未破产玩家
 *
 * ⚠ AIDecisionMaker 为 nullptr 时 UE_LOG Error + EndTurn 兜底，不崩溃。
 * （story-004 起：每实际执行动作广播 OnAIActionExecuted，见 loop 内。）
 */
// EAIActionType（簇A AI 动作）→ EActionType（story-004 HUD 可观察）映射
static EActionType MapAIActionTypeToActionType(EAIActionType In)
{
    switch (In)
    {
    case EAIActionType::BuyProperty:        return EActionType::BuyProperty;
    case EAIActionType::BuildHouse:         return EActionType::BuildHouse;
    case EAIActionType::MortgageProperty:   return EActionType::Mortgage;
    case EAIActionType::UnmortgageProperty: return EActionType::Unmortgage;
    case EAIActionType::None:
    default:                                return EActionType::None;
    }
}

void UPlayerTurnSubsystem::RunAiPostRollActions(int32 AiPlayerId, const FGameStateSnapshot& Snapshot)
{
    // ① 阶段守卫：仅 PostRollAction 合法（G2：验证型错误路径用 UE_LOG Error，不用 ensure）
    if (CurrentPhase != ETurnPhase::PostRollAction)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::RunAiPostRollActions — "
                 "非法调用：当前阶段 %d 非 PostRollAction（4）。"
                 "AiPlayerId=%d 被忽略，回合状态未变更。"),
            static_cast<int32>(CurrentPhase),
            AiPlayerId);
        return;
    }

    // AIDecisionMaker 未注入时兜底（Error + EndTurn，不崩溃）
    if (!AIDecisionMaker.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::RunAiPostRollActions — "
                 "AIDecisionMaker 未注入（nullptr）。"
                 "AiPlayerId=%d，执行 0 条动作并 EndTurn 兜底。"),
            AiPlayerId);
        EndTurn(/*bSentToJailThisTurn=*/false);
        return;
    }

    // ② 调 DecidePostRollActions，获取 AI 决策动作列表（本簇 snapshot 由调用方装配并传入）
    TArray<FTurnAction> Actions = AIDecisionMaker->DecidePostRollActions(Snapshot);

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::RunAiPostRollActions — "
             "AiPlayerId=%d，DecidePostRollActions 返回 %d 条动作。"),
        AiPlayerId, Actions.Num());

    // ③ 逐 FTurnAction 执行：执行前经 IActionValidator 重校验可行性（AC-6 / GDD CR-8 批处理失败策略）
    LastExecutedActions.Reset();   // 清空本回合执行记录（AC-42 oracle）
    for (int32 i = 0; i < Actions.Num(); ++i)
    {
        const FTurnAction& Action = Actions[i];

        // 重校验：ActionValidator 未注入时退化为"全部可行"（向后兼容簇A TC-3/TC-37d 无 validator 注入路径）
        const bool bFeasible = !ActionValidator.IsValid()
            ? true
            : ActionValidator->IsActionFeasible(Action);

        if (!bFeasible)
        {
            // 不可行 → 静默跳过 + UE_LOG Warning（G2：错误/降级路径用 UE_LOG，不 ensure）+ continue
            // 被跳过动作不进 LastExecutedActions → 其目标状态未被改变（AC-42①）
            // 后续合法动作仍继续执行（不中止整批，AC-42②）
            UE_LOG(LogTemp, Warning,
                TEXT("UPlayerTurnSubsystem::RunAiPostRollActions — "
                     "  [%d/%d] ActionType=%d TargetTileIndex=%d 执行期不可行，静默跳过（AC-6）。"),
                i + 1, Actions.Num(),
                static_cast<int32>(Action.ActionType), Action.TargetTileIndex);
            continue;
        }

        // 可行 → 执行（本簇占位执行=记录到 LastExecutedActions + UE_LOG；
        //   真实经济/建房/所有权调用归簇C/规则 epic，本簇严守 Out of Scope 不调任何规则系统）
        LastExecutedActions.Add(Action);

        // story-004 AC-6：每实际执行动作广播 OnAIActionExecuted，ActionIndex 按执行序 0..M-1 自增
        //   （被跳过的不可行动作走上方 continue，不到此处，故不广播、不占号）
        {
            FAIActionDetails Details;
            Details.ActionIndex     = LastExecutedActions.Num() - 1; // 0-based 执行序
            Details.ActingPlayerId  = AiPlayerId;
            Details.TargetTileIndex = Action.TargetTileIndex;
            Details.Amount          = 0; // 金额归经济5，本层未知（占位 0）
            Details.ActionType      = MapAIActionTypeToActionType(Action.ActionType);
            OnAIActionExecuted.Broadcast(Details);
        }

        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::RunAiPostRollActions — "
                 "  [%d/%d] ActionType=%d TargetTileIndex=%d 可行，执行（簇B 占位执行=记录）。"),
            i + 1, Actions.Num(),
            static_cast<int32>(Action.ActionType), Action.TargetTileIndex);
    }

    // 空数组 [] → 执行 0 条（AC-37d：仍需走 EndTurn 移交下一玩家）
    // ④ 执行完（含空数组）→ EndTurn 推进 TurnEnd + 移交下一未破产玩家（复用 pt-002/003）
    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::RunAiPostRollActions — "
             "AiPlayerId=%d，LastExecutedActions.Num()=%d（共 %d 条请求），调 EndTurn 推进 TurnEnd。"),
        AiPlayerId, LastExecutedActions.Num(), Actions.Num());

    EndTurn(/*bSentToJailThisTurn=*/false);
}

// ===========================================================================
// pt-007 簇 B：SetActionValidator — DI 注入 IActionValidator（AC-6 / AC-42）
// ===========================================================================

void UPlayerTurnSubsystem::SetActionValidator(TSharedPtr<IActionValidator> Validator)
{
    // TSharedPtr 赋值，自动管理生命周期（非 UObject，GC 不追踪，TSharedPtr 正确释放）
    ActionValidator = Validator;

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::SetActionValidator — "
             "ActionValidator %s。"),
        ActionValidator.IsValid() ? TEXT("已注入") : TEXT("已清除（nullptr）"));
}

// ===========================================================================
// pt-007 簇 B：RunAiJailAction — AI 出狱决策执行路径（AC-5 / AC-39 / AC-39b）
//
// 可行性判据单一来源 = Snapshot（决策视图，满足 AC-39b fixture 可替换 mock 前提）。
// 实际扣款/消卡/掷骰归下游 epic（Out of Scope 严守）。
//
// 关键不变式：AiPlayerId 须 == CurrentActivePlayerId（AdvanceFromJailTurn 内部用此 ID）。
// ===========================================================================

void UPlayerTurnSubsystem::RunAiJailAction(int32 AiPlayerId, const FGameStateSnapshot& Snapshot)
{
    // ① 阶段守卫：仅 JailTurn 合法（G2：验证型错误路径用 UE_LOG Error，不 ensure）
    if (CurrentPhase != ETurnPhase::JailTurn)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::RunAiJailAction — "
                 "非法调用：当前阶段 %d 非 JailTurn(6)。"
                 "AiPlayerId=%d 被忽略，回合状态未变更。"),
            static_cast<int32>(CurrentPhase), AiPlayerId);
        return;
    }

    // ② AIDecisionMaker 未注入兜底：保守降级留狱（AdvanceFromJailTurn(true)，不崩）
    if (!AIDecisionMaker.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::RunAiJailAction — "
                 "AIDecisionMaker 未注入（nullptr）。"
                 "AiPlayerId=%d，保守降级留狱并推进。"),
            AiPlayerId);
        AdvanceFromJailTurn(/*bRemainsInJail=*/true);
        return;
    }

    // ③ 同步调 DecideJailAction 取 AI 出狱决策
    const EJailAction Choice = AIDecisionMaker->DecideJailAction(Snapshot);

    // ④ 按返回值发起出狱/留狱路径（可行性自判 from Snapshot）
    switch (Choice)
    {
    case EJailAction::PayBail:
        // 保释意图：Cash >= 保释金 → 出狱路径（实际扣款归经济5，Out of Scope）；
        //   不足 → 降级留狱（AC-39b①：不扣成负现金——本系统从不扣款，故 Cash 结构性不变）
        if (Snapshot.SelfCash >= Snapshot.JailBailAmount)
        {
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::RunAiJailAction — "
                     "PlayerId=%d PayBail 可行（Cash=%d >= BailAmount=%d），出狱路径。"),
                AiPlayerId, Snapshot.SelfCash, Snapshot.JailBailAmount);
            AdvanceFromJailTurn(/*bRemainsInJail=*/false);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("UPlayerTurnSubsystem::RunAiJailAction — "
                     "PlayerId=%d PayBail 但现金不足（Cash=%d < BailAmount=%d），"
                     "降级留狱（AC-39b①）。"),
                AiPlayerId, Snapshot.SelfCash, Snapshot.JailBailAmount);
            AdvanceFromJailTurn(/*bRemainsInJail=*/true);
        }
        break;

    case EJailAction::UseCard:
        // 用卡意图：有卡 → 出狱（实际消卡归事件格7，Out of Scope）；
        //   无卡 → 降级留狱（AC-39b②：不凭空消卡——本系统无卡存储，结构性保证）
        if (Snapshot.bHasJailCard)
        {
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::RunAiJailAction — "
                     "PlayerId=%d UseCard 可行（bHasJailCard=true），出狱路径。"),
                AiPlayerId);
            AdvanceFromJailTurn(/*bRemainsInJail=*/false);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("UPlayerTurnSubsystem::RunAiJailAction — "
                     "PlayerId=%d UseCard 但无出狱卡（bHasJailCard=false），"
                     "降级留狱（AC-39b②）。"),
                AiPlayerId);
            AdvanceFromJailTurn(/*bRemainsInJail=*/true);
        }
        break;

    case EJailAction::RollDouble:
        // 掷骰意图：真实"掷双点出狱"执行归 dice(3)/事件格(7)（Out of Scope）。
        //   本簇最小语义 = 本回合留狱待掷（AdvanceFromJailTurn(true)，JailTurnsServed+1）；
        //   dice/事件7 落地后此路改走真实掷骰判定（契约不变）。
        UE_LOG(LogTemp, Log,
            TEXT("UPlayerTurnSubsystem::RunAiJailAction — "
                 "PlayerId=%d RollDouble 掷骰意图（真实掷骰归 dice3/事件7，本簇留狱待掷）。"),
            AiPlayerId);
        AdvanceFromJailTurn(/*bRemainsInJail=*/true);
        break;

    default:
        // 未知枚举值（防御性兜底，正常路径不可达）
        UE_LOG(LogTemp, Warning,
            TEXT("UPlayerTurnSubsystem::RunAiJailAction — "
                 "PlayerId=%d 未知 EJailAction(%d)，降级留狱。"),
            AiPlayerId, static_cast<int32>(Choice));
        AdvanceFromJailTurn(/*bRemainsInJail=*/true);
        break;
    }
}

// ===========================================================================
// pt-007 簇 B：ResolveAuctionBid — 拍卖出价 sentinel 值域解析（AC-10 / AC-45，Alpha）
//
// 纯函数，无副作用，无 World 依赖（可直接 static 调用，不需 Subsystem 实例）。
// 出价合法性最终校验归拍卖(12)（MVP 不触发）；本函数仅钉死 int32 sentinel 值域约定。
// ===========================================================================

/*static*/
int32 UPlayerTurnSubsystem::ResolveAuctionBid(
    int32 RawBid,
    int32 CurrentHighest,
    int32 MinIncrement,
    int32 Cash)
{
    // 负数 / INDEX_NONE(-1) / 0 → 放弃（0 非法，不作"出价0"解）
    if (RawBid <= 0)
    {
        return 0;
    }

    // 合法：> 当前最高价 且 <= Cash 且 >= 最低加价步长（三条均须满足）
    if (RawBid > CurrentHighest && RawBid <= Cash && RawBid >= MinIncrement)
    {
        return RawBid;
    }

    // 违反任一值域条件 → 放弃（0）
    return 0;
}

// ===========================================================================
// pt-007 簇 C1：SetOwnershipProvider / SetBuildingProvider / SetEconomyResolver
// ===========================================================================

/**
 * 注入 IOwnershipProvider（所有权6 接缝 DI，pt-007 AC-2/4/7）。
 * 仿 SetActionValidator 赋值 + UE_LOG 模式。
 */
void UPlayerTurnSubsystem::SetOwnershipProvider(TSharedPtr<IOwnershipProvider> Provider)
{
    OwnershipProvider = Provider;
    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::SetOwnershipProvider — "
             "IOwnershipProvider %s。"),
        OwnershipProvider.IsValid() ? TEXT("已注入") : TEXT("已清除（nullptr）"));
}

/**
 * 注入 IBuildingProvider（建房8 接缝 DI，pt-007 AC-2/4/7）。
 * 仿 SetActionValidator 赋值 + UE_LOG 模式。
 */
void UPlayerTurnSubsystem::SetBuildingProvider(TSharedPtr<IBuildingProvider> Provider)
{
    BuildingProvider = Provider;
    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::SetBuildingProvider — "
             "IBuildingProvider %s。"),
        BuildingProvider.IsValid() ? TEXT("已注入") : TEXT("已清除（nullptr）"));
}

/**
 * 注入 IEconomyResolver（经济5 接缝 DI，pt-007 AC-2/4/7）。
 * 仿 SetActionValidator 赋值 + UE_LOG 模式。
 */
void UPlayerTurnSubsystem::SetEconomyResolver(TSharedPtr<IEconomyResolver> Resolver)
{
    EconomyResolver = Resolver;
    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::SetEconomyResolver — "
             "IEconomyResolver %s。"),
        EconomyResolver.IsValid() ? TEXT("已注入") : TEXT("已清除（nullptr）"));
}

// ===========================================================================
// pt-007 簇 C1：AssembleSnapshot — 装配 AI 只读快照（AC-2，ADR-0006 IG#1/#2/#3）
//
// 设计决策（写进注释供 code-review 核，ADR-0006 Key Interfaces）：
//   PreaggregatedNlv 语义 = per-tile 清算贡献（AI 自有未抵押格：MV；AI 自有建筑：house_count * BuildingCost/2）。
//   FGameStateSnapshot 无 top-level nlv 字段，AI 求全组合 nlv 时对自身格 PreaggregatedNlv 求和——
//   纯求 snapshot 标量、零 provider 访问，不破"AI 不自算 MV/house"的 5→8 反向环纪律。
//   C2 破产 NLV（AC-9）用相同公式但作单一聚合值传 is_insolvent（不存 snapshot），口径一致。
// ===========================================================================

FGameStateSnapshot UPlayerTurnSubsystem::AssembleSnapshot(int32 AiPlayerId) const
{
    FGameStateSnapshot S;

    // —— 决策主体自身 ——（FindPlayerById 已 const）
    const URentoPlayerState* PS = FindPlayerById(AiPlayerId);
    S.SelfPlayerId    = AiPlayerId;
    S.SelfCash        = PS ? PS->Cash : 0;
    S.JailTurnsServed = PS ? PS->JailTurnsServed : 0;
    // 全局常量保留 struct 默认（StartingCash=1500/BoardTileCountClassic=40/JailBailAmount=50/MaxJailTurns=3）。
    // bHasJailCard：owner=事件格7，C1 无 seam → 保留默认 false（jail card 注入归事件7，Out of Scope）。

    // 缺 provider 降级（ADR-0006 IG#7）：未全注入 → 返回最小 snapshot + UE_LOG Warning（hook 端会降保守默认）
    if (!OwnershipProvider.IsValid() || !BuildingProvider.IsValid() || !EconomyResolver.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UPlayerTurnSubsystem::AssembleSnapshot — provider 未全注入（O=%d B=%d E=%d），"
                 "返回最小 snapshot（缺字段降级，ADR-0006 IG#7）。"),
            OwnershipProvider.IsValid(), BuildingProvider.IsValid(), EconomyResolver.IsValid());
        return S;
    }

    // ① 全盘 per-tile 明细：一次 GetBoardOwnership + 逐格 GetHouseCount/GetBuildingCost
    //    per-tile PreaggregatedNlv = 该格清算贡献（AI 自身格才计；防 5→8 反向环：MV/cost 装配期取，AI 不自算）
    const TArray<FOwnershipSnapshot> Board = OwnershipProvider->GetBoardOwnership(AiPlayerId);
    for (const FOwnershipSnapshot& OS : Board)
    {
        FTileSnapshotEntry E;
        E.TileIndex      = OS.TileIndex;
        E.OwnerId        = OS.OwnerId;
        E.HouseCount     = BuildingProvider->GetHouseCount(OS.TileIndex);
        E.bIsMortgaged   = OS.bIsMortgaged;
        E.bIsMonopoly    = OS.bIsMonopoly;
        E.ColorGroup     = OS.ColorGroup;
        E.PurchasePrice  = OS.PurchasePrice;
        E.MortgageValue  = OS.MortgageValue;
        E.BuildingCost   = BuildingProvider->GetBuildingCost(OS.TileIndex);
        E.UnmortgageCost = OS.MortgageValue + FMath::CeilToInt(OS.MortgageValue / 10.0f); // economy5 口径 MV+ceil(MV/10)
        // per-tile 清算贡献（AI 拥有的格才贡献；抵押地 MV 不计，建筑半价回收）
        E.PreaggregatedNlv = (OS.OwnerId == AiPlayerId)
            ? (E.HouseCount * (E.BuildingCost / 2) + (OS.bIsMortgaged ? 0 : OS.MortgageValue))
            : 0;
        S.Tiles.Add(E);
    }

    // ② Rent_top1/top2 派生（ADR-0006 IG#2）：遍历全盘对手未抵押地产，调经济算租取前两高
    //    （口径单一交经济5 CalculateRent，回合2 不重定义租金公式）
    TArray<int32> OpponentRents;
    for (const FOwnershipSnapshot& OS : Board)
    {
        if (OS.OwnerId != AiPlayerId && OS.OwnerId != INDEX_NONE && !OS.bIsMortgaged)
        {
            FRentInput RI;
            RI.PayerId    = AiPlayerId;
            RI.TileIndex  = OS.TileIndex;
            RI.Ownership  = OS;
            RI.HouseCount = BuildingProvider->GetHouseCount(OS.TileIndex);
            RI.DiceTotal  = 0; // Rent_top 是"潜在单次租金"派生量，无当前程 dice 上下文，置 0（economy F-4 Utility 退化）
            OpponentRents.Add(EconomyResolver->CalculateRent(RI));
        }
    }
    OpponentRents.Sort([](const int32& A, const int32& B){ return A > B; }); // 降序
    S.Rent_top1 = OpponentRents.Num() >= 1 ? OpponentRents[0] : 0;
    S.Rent_top2 = OpponentRents.Num() >= 2 ? OpponentRents[1] : 0;

    return S;
}

// ===========================================================================
// pt-007 簇 C1：RunAiBuyDecision — AI 买地决策执行路径（AC-4，GDD AC-38/AC-38b）
//
// 执行流程：
//   ① oracle 重置 + AIDecisionMaker 未注入兜底（降保守默认不买）
//   ② 装配只读快照（AssembleSnapshot，AC-2）
//   ③ 调 AIDecisionMaker->DecideBuyProperty(S, TileIndex) 取 AI 决策
//   ④ 决定不买 → false（不触发购买，AC-38）
//   ⑤ 决定买 → 委派 EconomyResolver->ExecutePurchase 执行期可行性校验+执行（AC-38b）
//      ExecutePurchase=false → 视同不买（不扣负/地产未变/不崩，AC-38b）
// ===========================================================================

bool UPlayerTurnSubsystem::RunAiBuyDecision(int32 AiPlayerId, int32 TileIndex)
{
    bLastPurchaseExecuted = false; // 重置 oracle

    // AI hook 未注入兜底（ADR-0006 IG#7：降保守默认 false=不买）
    if (!AIDecisionMaker.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::RunAiBuyDecision — AIDecisionMaker 未注入，降保守默认不买。"));
        return false;
    }

    // ① 装配只读快照（AC-2）→ ② 调 AI 买地 hook（AI 经 const& 只读消费，不写状态）
    const FGameStateSnapshot S = AssembleSnapshot(AiPlayerId);
    const bool bWantBuy = AIDecisionMaker->DecideBuyProperty(S, TileIndex);

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::RunAiBuyDecision — PlayerId=%d TileIndex=%d DecideBuyProperty=%d。"),
        AiPlayerId, TileIndex, bWantBuy ? 1 : 0);

    if (!bWantBuy)
    {
        return false; // AI 决定不买（AC-38：按返回值不触发购买）
    }

    // ③ 决定买 → 委派经济5/所有权6 执行期可行性校验+执行（AC-38b）
    if (!EconomyResolver.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::RunAiBuyDecision — EconomyResolver 未注入，无法执行购买，视同不买。"));
        return false;
    }
    const bool bExecuted = EconomyResolver->ExecutePurchase(AiPlayerId, TileIndex);
    if (!bExecuted)
    {
        // 不可行（Cash<Price / 地产已被买）→ 视同 false：不扣负（框架从不写 Cash）、地产未变更、回合不崩（AC-38b）
        UE_LOG(LogTemp, Warning,
            TEXT("UPlayerTurnSubsystem::RunAiBuyDecision — PlayerId=%d 决定买但执行期不可行，"
                 "视同不买（AC-38b：不扣负/地产未变/不崩）。"),
            AiPlayerId);
        return false;
    }

    bLastPurchaseExecuted = true;
    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::RunAiBuyDecision — PlayerId=%d 购买成功（TileIndex=%d）。"),
        AiPlayerId, TileIndex);
    return true;
}

// ===========================================================================
// pt-007 簇 C1：SettleRentOnArrival — 到达结算收租（AC-7，GDD AC-48/AC-49 / CR-3.2）
//
// 调用链（恰 N 次，供 TC-7 断言）：
//   ① BuildOwnershipSnapshot 恰 1 次（CR-3.2 ①）
//   ② GetHouseCount 恰 1 次（CR-3.2 ②，AC-49 缺省 0）
//   ③ CalculateRent 恰 1 次，传入 dice_total PULL（CR-3.1/CR-3.2 ③）
// ===========================================================================

int32 UPlayerTurnSubsystem::SettleRentOnArrival(int32 PayerId, int32 TileIndex)
{
    LastRentCharged = 0; // 重置 oracle

    if (!OwnershipProvider.IsValid() || !BuildingProvider.IsValid() || !EconomyResolver.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::SettleRentOnArrival — provider 未全注入，跳过算租。"));
        return 0;
    }

    // ① 调所有权6 BuildOwnershipSnapshot 恰 1 次（CR-3.2 ①）
    const FOwnershipSnapshot OS = OwnershipProvider->BuildOwnershipSnapshot(PayerId, TileIndex);

    // ② 读建房8 house_count（8 未实现 mock 返回缺省 0，GDD AC-49；CR-3.2 ②）
    const int32 HouseCount = BuildingProvider->GetHouseCount(TileIndex);

    // ③ 拼装算租输入 + 当前程 dice_total PULL（CR-3.1：读当前行动玩家 CurrentRollContext.Total）
    FRentInput RI;
    RI.PayerId    = PayerId;
    RI.TileIndex  = TileIndex;
    RI.Ownership  = OS;
    RI.HouseCount = HouseCount;
    RI.DiceTotal  = GetCurrentRollTotal(); // PULL 当前程总点（CR-3.1）

    // 调经济5 算租入口恰 1 次（CR-3.2 ③），参数与上游一致
    LastRentCharged = EconomyResolver->CalculateRent(RI);
    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::SettleRentOnArrival — PayerId=%d TileIndex=%d house_count=%d "
             "dice_total=%d → rent=%d。"),
        PayerId, TileIndex, HouseCount, RI.DiceTotal, LastRentCharged);
    return LastRentCharged;
}

// ===========================================================================
// pt-007 簇 C2：CheckInsolvencyWithNlv — 破产 NLV 聚合 + IsInsolvent 判据
//   (AC-9 / GDD AC-52 / CR-3.4)
//
// 注意：CheckInsolvencyWithNlv 在 RunForcedLiquidation 内调用，声明在 .h 中已先定义，
// 此处 cpp 实现顺序无要求。
// ===========================================================================

bool UPlayerTurnSubsystem::CheckInsolvencyWithNlv(int32 PlayerId, int32 AmountDue)
{
    if (!OwnershipProvider.IsValid() || !BuildingProvider.IsValid() || !EconomyResolver.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::CheckInsolvencyWithNlv — provider 未全注入，保守判破产。"));
        return true;
    }

    // ① GetPlayerBuildings 恰 1 次（全组合枚举，AC-9 ① / CR-3.4 ①）
    const TArray<FPlayerBuilding> Buildings = BuildingProvider->GetPlayerBuildings(PlayerId);

    // ② preaggregated_nlv = Σ house_count × floor(BuildingCost/2) + Σ MortgageValue(未抵押 owned 地)
    //    （AC-9 ② / CR-3.4 ③；floor 由 int 截断）
    int32 Nlv = 0;
    for (const FPlayerBuilding& B : Buildings)
    {
        Nlv += B.HouseCount * (BuildingProvider->GetBuildingCost(B.TileIndex) / 2);
    }
    const TArray<FOwnershipSnapshot> Board = OwnershipProvider->GetBoardOwnership(PlayerId);
    for (const FOwnershipSnapshot& OS : Board)
    {
        if (OS.OwnerId == PlayerId && !OS.bIsMortgaged)
        {
            Nlv += OS.MortgageValue;
        }
    }

    // ③ IsInsolvent 恰 1 次，传外部预聚合 NLV（economy 不反向调 8，AC-9 ③④ / CR-3.4 ②）
    const bool bInsolvent = EconomyResolver->IsInsolvent(PlayerId, AmountDue, Nlv);
    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::CheckInsolvencyWithNlv — PlayerId=%d AmountDue=%d nlv=%d → is_insolvent=%d。"),
        PlayerId, AmountDue, Nlv, bInsolvent ? 1 : 0);
    return bInsolvent;
}

// ===========================================================================
// pt-007 簇 C2：RunForcedLiquidation — 强制清算循环（AC-8 / GDD AC-50/AC-51 / CR-3.3）
//
// 循环 while(Cash < AmountDue)：
//   ① mortgage-empty-first（AC-51 ①②）：owner==player ∧ 未抵押 ∧ house==0 → Mortgage（选 MV 最小）
//   ② 否则若有建筑（house>0）→ ForcedSellNextBuilding（卖房腿）
//   ③ 资产耗尽 → CheckInsolvencyWithNlv → return !IsInsolvent
//   Cash >= AmountDue → return true（偿付成功早停，AC-51 ③）
//
// SafetyGuard（MaxIterations=1000）防 mock 不抬 Cash 死循环（dev 安全网）。
// ===========================================================================

bool UPlayerTurnSubsystem::RunForcedLiquidation(int32 PlayerId, int32 AmountDue)
{
    if (!OwnershipProvider.IsValid() || !BuildingProvider.IsValid() || !EconomyResolver.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::RunForcedLiquidation — provider 未全注入，保守判破产。"));
        return false;
    }
    URentoPlayerState* PS = FindPlayerById(PlayerId);
    if (!PS)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::RunForcedLiquidation — PlayerId=%d 无 PlayerState。"), PlayerId);
        return false;
    }

    // 有界终止守卫（CR-3.3 有界终止；防 mock 不抬 Cash 死循环的 dev 安全网）
    int32 SafetyGuard = 0;
    const int32 MaxIterations = 1000; // 远超任何合法资产数

    // 现金判据读 PlayerState->Cash（经济5/所有权6 经 Mortgage/ForcedSellNextBuilding 抬 Cash；框架从不写 Cash）
    while (PS->Cash < AmountDue)
    {
        if (++SafetyGuard > MaxIterations)
        {
            UE_LOG(LogTemp, Error,
                TEXT("UPlayerTurnSubsystem::RunForcedLiquidation — 超迭代上限 %d，强制终止（防死循环）。"),
                MaxIterations);
            break;
        }

        const TArray<FOwnershipSnapshot> Board = OwnershipProvider->GetBoardOwnership(PlayerId);

        // ① mortgage-empty-first（AC-51 ①②）：owner==player ∧ 未抵押 ∧ GetHouseCount==0
        //    （AC-50 抵押前置读：带房地 house>0 不可抵押，跳过），选 MV 最小
        int32 BestTile = INDEX_NONE;
        int32 BestMV   = MAX_int32;
        for (const FOwnershipSnapshot& OS : Board)
        {
            if (OS.OwnerId == PlayerId && !OS.bIsMortgaged
                && BuildingProvider->GetHouseCount(OS.TileIndex) == 0) // AC-50：带房地不可抵押
            {
                if (OS.MortgageValue < BestMV) { BestMV = OS.MortgageValue; BestTile = OS.TileIndex; }
            }
        }
        if (BestTile != INDEX_NONE)
        {
            OwnershipProvider->Mortgage(BestTile); // mock 标记抵押 + 抬 Cash
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::RunForcedLiquidation — 抵押空地 tile=%d（MV=%d）。"),
                BestTile, BestMV);
            continue;
        }

        // ② 否则若有建筑（任一 owned tile GetHouseCount>0）→ ForcedSellNextBuilding
        //    （AC-50 对照：带房地转卖房腿）
        bool bHasBuilding = false;
        for (const FOwnershipSnapshot& OS : Board)
        {
            if (OS.OwnerId == PlayerId && BuildingProvider->GetHouseCount(OS.TileIndex) > 0)
            {
                bHasBuilding = true;
                break;
            }
        }
        if (bHasBuilding)
        {
            BuildingProvider->ForcedSellNextBuilding(PlayerId); // mock 抬 Cash（+降 house_count）
            UE_LOG(LogTemp, Log,
                TEXT("UPlayerTurnSubsystem::RunForcedLiquidation — 卖房（ForcedSellNextBuilding）。"));
            continue;
        }

        // ③ 资产耗尽 → 破产判据 NLV 聚合 + IsInsolvent（AC-9）
        const bool bInsolvent = CheckInsolvencyWithNlv(PlayerId, AmountDue);
        UE_LOG(LogTemp, Warning,
            TEXT("UPlayerTurnSubsystem::RunForcedLiquidation — 资产耗尽，is_insolvent=%d。"),
            bInsolvent ? 1 : 0);
        return !bInsolvent; // 破产 → 返回 false
    }

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::RunForcedLiquidation — Cash=%d >= AmountDue=%d，偿付成功（早停）。"),
        PS->Cash, AmountDue);
    return true; // Cash >= AmountDue，偿付成功
}

// ===========================================================================
// story-004 AC-7：HandleBuildingChanged — 建房通告 beat（CR-3.5）
//
// 建房8 在 ResolvePhase/PostRollAction 广播 OnBuildingChanged(tile,newCount)（2 字段 owner 契约）；
// 本系统据当前回合上下文广播全玩家可见 OnBuildingAnnounced。建房只在这两阶段发生：
//   TurnStart/TurnEnd 期间收到（异常时机）→ 不广播（时机 guard）。
// 建筑归属玩家 id 取当前回合上下文 CurrentActivePlayerId（方案②，非事件第3字段）。
// 信息层事件，不修改任何游戏状态（AC-7 ③）。
// ===========================================================================

void UPlayerTurnSubsystem::HandleBuildingChanged(int32 TileIndex, int32 NewHouseCount)
{
    if (CurrentPhase != ETurnPhase::ResolvePhase && CurrentPhase != ETurnPhase::PostRollAction)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("UPlayerTurnSubsystem::HandleBuildingChanged — 当前阶段 %d 非 Resolve/PostRoll，"
                 "不发通告（AC-7 时机 guard）。"),
            static_cast<int32>(CurrentPhase));
        return;
    }

    FBuildingAnnouncedInfo Info;
    Info.TileIndex      = TileIndex;
    Info.NewHouseCount  = NewHouseCount;
    Info.ActingPlayerId = CurrentActivePlayerId; // 方案②：取当前回合上下文
    OnBuildingAnnounced.Broadcast(Info);

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::HandleBuildingChanged — 通告 tile=%d house=%d actor=%d。"),
        TileIndex, NewHouseCount, CurrentActivePlayerId);
}

// ===========================================================================
// pt-008：读档序列化 round-trip + delegate 重绑拓扑序 + 可存档点查询
// ===========================================================================

UPlayerTurnSaveData* UPlayerTurnSubsystem::CaptureSaveData() const
{
    // NewObject 于 transient package（存档对象与 World 无关；调用方防 GC）
    UPlayerTurnSaveData* Save = NewObject<UPlayerTurnSaveData>(GetTransientPackage());
    check(Save);

    // 顶层回合2 字段
    Save->CurrentPhase          = CurrentPhase;
    Save->CurrentActivePlayerId = CurrentActivePlayerId;
    Save->DoublesJailThreshold  = DoublesJailThreshold;

    // 逐玩家装记录（DV-1 值记录；DV-2 roll-context per-player 进记录）
    Save->PlayerRecords.Reserve(PlayerStates.Num());
    for (const TObjectPtr<URentoPlayerState>& PS : PlayerStates)
    {
        if (!PS) { continue; }
        FPlayerStateRecord R;
        R.PlayerId           = PS->PlayerId;
        R.DisplayName        = PS->DisplayName;
        R.TokenColor         = PS->TokenColor;
        R.bIsAI              = PS->bIsAI;
        R.AIDifficulty       = PS->AIDifficulty;
        R.CurrentTileIndex   = PS->CurrentTileIndex;
        R.Cash               = PS->Cash;
        R.bIsInJail          = PS->bIsInJail;
        R.JailTurnsServed    = PS->JailTurnsServed;
        R.bIsBankrupt        = PS->bIsBankrupt;
        R.TurnOrderIndex     = PS->TurnOrderIndex;
        R.ConsecutiveDoubles = PS->ConsecutiveDoubles;
        R.CurrentRollContext = PS->CurrentRollContext;
        Save->PlayerRecords.Add(R);
    }

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::CaptureSaveData — 采集 %d 玩家，阶段=%d，活跃玩家=%d。"),
        Save->PlayerRecords.Num(), static_cast<int32>(Save->CurrentPhase), Save->CurrentActivePlayerId);

    return Save;
}

void UPlayerTurnSubsystem::RestoreFromSaveData(const UPlayerTurnSaveData* SaveData)
{
    if (!SaveData)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::RestoreFromSaveData — SaveData 为 nullptr，恢复跳过。"));
        return;
    }

    // ── 读档恢复第①步（AC-4 回合2 腿）：反序列化全字段，NewObject 重建 PlayerStates ──
    // 拓扑序：DA→经济/地产/建房/牌堆→【回合2 此处】→骰子 SetSeed→重绑→switch。
    // 跨系统上游腿（DA/经济/…）由存档21 编排（OoS）；本方法只重建回合2 字段。
    PlayerStates.Empty();
    PlayerStates.Reserve(SaveData->PlayerRecords.Num());
    for (const FPlayerStateRecord& R : SaveData->PlayerRecords)
    {
        URentoPlayerState* PS = NewObject<URentoPlayerState>(this);
        check(PS);
        PS->PlayerId           = R.PlayerId;
        PS->DisplayName        = R.DisplayName;
        PS->TokenColor         = R.TokenColor;
        PS->bIsAI              = R.bIsAI;
        PS->AIDifficulty       = R.AIDifficulty;
        PS->CurrentTileIndex   = R.CurrentTileIndex;
        PS->Cash               = R.Cash;
        PS->bIsInJail          = R.bIsInJail;
        PS->JailTurnsServed    = R.JailTurnsServed;
        PS->bIsBankrupt        = R.bIsBankrupt;
        PS->TurnOrderIndex     = R.TurnOrderIndex;
        PS->ConsecutiveDoubles = R.ConsecutiveDoubles;
        PS->CurrentRollContext = R.CurrentRollContext;
        PlayerStates.Add(PS);
    }

    // ── 直写枚举/标量字段（禁经 SetPhase——SetPhase 广播，下游未重绑→丢失，AC-3/L401）──
    CurrentPhase          = SaveData->CurrentPhase;
    CurrentActivePlayerId = SaveData->CurrentActivePlayerId;
    DoublesJailThreshold  = SaveData->DoublesJailThreshold;

    // 瞬态非序列化字段复位（invocation list 不序列化；bLastRollWasDouble/送监狱标记不入存档）
    bLastRollWasDouble = false;
    bSentToJailThisTurnInternal = false;

    UE_LOG(LogTemp, Log,
        TEXT("UPlayerTurnSubsystem::RestoreFromSaveData — 还原 %d 玩家，精确阶段=%d，活跃玩家=%d（静默，未广播）。"),
        PlayerStates.Num(), static_cast<int32>(CurrentPhase), CurrentActivePlayerId);
}

void UPlayerTurnSubsystem::ResumeFromLoadedState()
{
    // ── 读档恢复第③步（AC-3/4）：字段已 Restore（①）+ 下游已重绑（②），此处才广播 ──
    // switch(CurrentPhase) 重入：FSM 禁 Latent（ADR-0001），无等待态恢复，各阶段 = 同阶段重广播，
    // 让重绑监听者同步到精确阶段；玩家/AI 从精确阶段续行。
    switch (CurrentPhase)
    {
    case ETurnPhase::TurnStart:
    case ETurnPhase::RollPhase:
    case ETurnPhase::MovePhase:
    case ETurnPhase::ResolvePhase:
    case ETurnPhase::PostRollAction:
    case ETurnPhase::TurnEnd:
    case ETurnPhase::JailTurn:
        // 经 SetPhase 同阶段重广播（OldPhase==NewPhase==CurrentPhase，CD 取活跃玩家）
        SetPhase(CurrentPhase);
        break;
    default:
        UE_LOG(LogTemp, Error,
            TEXT("UPlayerTurnSubsystem::ResumeFromLoadedState — 非法 CurrentPhase=%d，无法重入。"),
            static_cast<int32>(CurrentPhase));
        break;
    }
}

bool UPlayerTurnSubsystem::CanSaveNow() const
{
    // AC-6 / ADR-0005 CR-4：定序进行中/未开局禁存；进入首个 TurnStart 后可存。
    // 定序是 InitializeFromConfig 内同步完成（非持久 Latent 阶段），故外部可查询的"不安全"态
    // = 首个 TurnStart 尚未启动（CurrentActivePlayerId==-1）。enable-not-own：回合2 报告，存档21 GIVEN。
    return PlayerStates.Num() >= 2 && CurrentActivePlayerId != INDEX_NONE;
}
