// BoardValidator.cpp
// =============================================================================
// 棋盘加载期完整性校验实现（story-005）
//
// 所有拒绝类校验均在 ValidateTiles() 中按固定顺序 Fail-Fast 执行：
//   板级检查（顺序见 BoardValidator.h 文件头注释）→ 逐格检查（按数组序）
//
// 无副作用：纯函数，不打 UE_LOG，不修改全局状态。
// 错误信息通过返回值 FBoardValidationError 传递（结构化错误码即契约）。
// =============================================================================

#include "BoardValidator.h"

// =============================================================================
// 内部辅助：判断格子类型是否为「可购买格」（Property / Railroad / Utility）
// =============================================================================
static bool IsPurchasableTile(ETileType TileType)
{
    return TileType == ETileType::Property
        || TileType == ETileType::Railroad
        || TileType == ETileType::Utility;
}

// =============================================================================
// ValidateTiles — 核心拒绝类校验（载体解耦，ADR-0002 R6）
//
// 检查顺序（固定，测试 fixture 依赖此序）：
// ---- 板级 ----
//  1. N < 4                        → BoardTooSmall
//  2. 索引覆盖性（缺号/重号）          → Duplicate/MissingTileIndex
//  3. Go 格数量 != 1                → GoTileCountInvalid（须先于 Index0NotGo，GAP-1）
//  4. TileIndex==0 格 TileType!=Go  → Index0NotGo
//  5. 有 GoToJail 但无 Jail          → NoJailTile
// ---- 逐格（按数组序，每格按以下子序）----
//  1. Property && ColorGroup==None  → PropertyMissingColorGroup
//  2. 非Property && ColorGroup!=None→ ColorGroupOnNonProperty
//  3. 可购买格 PurchasePrice<=0      → PurchasePriceInvalid
//  4. 可购买格 Mortgage>Purchase     → MortgageExceedsPurchase
//  5. Property RentTable.Num()!=6   → RentTableLengthInvalid(Expected=6)
//     Railroad RentTable.Num()!=4   → RentTableLengthInvalid(Expected=4)
//  6. Utility DiceMultiplierTable.Num()!=2 → DiceMultiplierLengthInvalid
//  7. Utility DiceMult 元素<1或>MAX → DiceMultiplierOutOfRange
//  8. Property/Railroad 带 DiceMult → DiceMultiplierTableNotAllowed
//  9. Utility 带 RentTable           → RentTableNotAllowed
// 10. 非可购买格带任一数组            → RentArrayOnNonPurchasable
// 11. Chance/CC && EventDeck==None  → EventDeckMissing
// 12. 非Go && SalaryAmount!=0       → SalaryOnNonGoTile
// 13. 非Tax && TaxAmount!=0         → TaxOnNonTaxTile
// =============================================================================
FBoardValidationError UBoardValidator::ValidateTiles(const TArray<FBoardTileData>& Tiles)
{
    const int32 N = Tiles.Num();

    // =========================================================================
    // 板级检查 1：N < 4 → BoardTooSmall（AC-18）
    // 环路至少需容纳四角格（Go/Jail/FreeParking/GoToJail），少于 4 格无意义
    // =========================================================================
    if (N < 4)
    {
        FBoardValidationError Err;
        Err.Code      = EBoardValidationError::BoardTooSmall;
        Err.TileIndex = -1; // 板级错误，无单格定位
        return Err;
    }

    // =========================================================================
    // 板级检查 2：索引覆盖性（AC-19a / AC-19b）
    // TileIndex 须恰好覆盖 [0, N-1] 各一次（不缺不重）
    //
    // 算法：
    //   Seen[i] = true 表示值 i 已被某格占用。
    //   遍历时：
    //     - t < 0 || t >= N → 跳过（不标记，留缺号供后续扫描，出现 DuplicateTileIndex 的优先级高于此）
    //     - Seen[t] 已 true → DuplicateTileIndex{t}，立即返回
    //     - 否则 Seen[t] = true
    //   遍历后：首个 Seen[i]==false → MissingTileIndex{i}
    // =========================================================================
    {
        TArray<bool> Seen;
        Seen.Init(false, N);

        for (const FBoardTileData& Tile : Tiles)
        {
            const int32 t = Tile.TileIndex;
            if (t < 0 || t >= N)
            {
                // 越界值：不标记，让后续扫描发现缺号
                continue;
            }
            if (Seen[t])
            {
                // 重号（AC-19b）
                FBoardValidationError Err;
                Err.Code      = EBoardValidationError::DuplicateTileIndex;
                Err.TileIndex = t; // 重号的索引值本身
                return Err;
            }
            Seen[t] = true;
        }

        // 扫描缺号（AC-19a）
        for (int32 i = 0; i < N; ++i)
        {
            if (!Seen[i])
            {
                FBoardValidationError Err;
                Err.Code      = EBoardValidationError::MissingTileIndex;
                Err.TileIndex = i; // 缺失的索引值本身
                return Err;
            }
        }
    }

    // =========================================================================
    // 板级检查 3：Go 类型格数量必须恰好为 1（AC-23d：0 个或 ≥2 个均拒绝）
    // F-2 的 GO 计数假设 Go 唯一。
    //
    // ⚠ 顺序契约（code-review BD-005 GAP-1）：本检查须**先于** Index0NotGo（板级检查 4）。
    //   否则「0 个 Go」会被 Index0NotGo 抢先（index0 必非 Go），致 AC-23d「0 个 Go →
    //   GoTileCountInvalid」永远不可达。本序下职责切分清晰：
    //     - 计数错误（0 或 ≥2）→ GoTileCountInvalid（板级 3）
    //     - 计数恰 1 但该唯一 Go 不在索引 0 → Index0NotGo（板级 4）
    // =========================================================================
    {
        int32 GoCount = 0;
        for (const FBoardTileData& Tile : Tiles)
        {
            if (Tile.TileType == ETileType::Go)
            {
                ++GoCount;
            }
        }
        if (GoCount != 1)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::GoTileCountInvalid;
            Err.TileIndex = -1; // 板级错误，无单格定位
            return Err;
        }
    }

    // =========================================================================
    // 板级检查 4：TileIndex==0 的格子必须是 Go（AC-20）
    // 覆盖性 + Go 计数==1 已通过，故 Go 唯一存在；此处校验它是否落在索引 0。
    // 用查找而非 Tiles[0]，因数组顺序与 TileIndex 一致性是 story-006 警告域，本函数不假设。
    // =========================================================================
    {
        const FBoardTileData* GoZero = Tiles.FindByPredicate(
            [](const FBoardTileData& T){ return T.TileIndex == 0; });

        // FindByPredicate 不会返回 nullptr（索引覆盖性已证 0 存在），但防御性检查
        if (GoZero && GoZero->TileType != ETileType::Go)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::Index0NotGo;
            Err.TileIndex = -1; // 板级语义，TileIndex 字段设 -1
            return Err;
        }
    }

    // =========================================================================
    // 板级检查 5：存在 GoToJail 格但全盘无 Jail 格 → NoJailTile（AC-23c）
    // GoToJail 目标未定义时拒绝（若全盘无 GoToJail 则无需检查）
    // =========================================================================
    {
        const bool bHasGoToJail = Tiles.ContainsByPredicate(
            [](const FBoardTileData& T){ return T.TileType == ETileType::GoToJail; });
        const bool bHasJail = Tiles.ContainsByPredicate(
            [](const FBoardTileData& T){ return T.TileType == ETileType::Jail; });

        if (bHasGoToJail && !bHasJail)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::NoJailTile;
            Err.TileIndex = -1; // 板级错误，无单格定位
            return Err;
        }
    }

    // =========================================================================
    // 逐格检查（按数组序；每格按固定子序 Fail-Fast）
    // =========================================================================
    for (const FBoardTileData& Tile : Tiles)
    {
        const int32  Idx           = Tile.TileIndex;
        const ETileType Type       = Tile.TileType;
        const bool bIsPurchasable  = IsPurchasableTile(Type);
        const bool bIsProperty     = (Type == ETileType::Property);
        const bool bIsRailroad     = (Type == ETileType::Railroad);
        const bool bIsUtility      = (Type == ETileType::Utility);

        // ---------------------------------------------------------------------
        // 逐格子序 1：Property 格缺色组（AC-21）
        // 地产必须归属某色组，否则集组/建房无法判定
        // ---------------------------------------------------------------------
        if (bIsProperty && Tile.ColorGroup == EColorGroup::None)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::PropertyMissingColorGroup;
            Err.TileIndex = Idx;
            return Err;
        }

        // ---------------------------------------------------------------------
        // 逐格子序 2：非 Property 格带色组（AC-32，反向校验）
        // 仅地产格可有色组；车站/公用/角格/事件格等带色组属数据污染
        // ---------------------------------------------------------------------
        if (!bIsProperty && Tile.ColorGroup != EColorGroup::None)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::ColorGroupOnNonProperty;
            Err.TileIndex = Idx;
            return Err;
        }

        // ---------------------------------------------------------------------
        // 逐格子序 3：可购买格 PurchasePrice <= 0（AC-23）
        // 可购买格必须有正价格
        // ---------------------------------------------------------------------
        if (bIsPurchasable && Tile.PurchasePrice <= 0)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::PurchasePriceInvalid;
            Err.TileIndex = Idx;
            return Err;
        }

        // ---------------------------------------------------------------------
        // 逐格子序 4：MortgageValue > PurchasePrice（AC-23e）
        // 抵押所得不应超过买价，MortgageValue <= PurchasePrice 是硬数据约束
        // 注：仅可购买格有意义；非可购买格 PurchasePrice/MortgageValue 均 0，不触发
        // ---------------------------------------------------------------------
        if (bIsPurchasable && Tile.MortgageValue > Tile.PurchasePrice)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::MortgageExceedsPurchase;
            Err.TileIndex = Idx;
            return Err;
        }

        // ---------------------------------------------------------------------
        // 逐格子序 5：RentTable 长度（AC-22 / AC-22b）
        // Property 须 6 项；Railroad 须 4 项；错长下游越界读取
        // ---------------------------------------------------------------------
        if (bIsProperty)
        {
            const int32 ActualLen = Tile.RentTable.Num();
            if (ActualLen != 6)
            {
                FBoardValidationError Err;
                Err.Code           = EBoardValidationError::RentTableLengthInvalid;
                Err.TileIndex      = Idx;
                Err.ExpectedLength = 6;
                Err.ActualLength   = ActualLen;
                return Err;
            }
        }
        else if (bIsRailroad)
        {
            const int32 ActualLen = Tile.RentTable.Num();
            if (ActualLen != 4)
            {
                FBoardValidationError Err;
                Err.Code           = EBoardValidationError::RentTableLengthInvalid;
                Err.TileIndex      = Idx;
                Err.ExpectedLength = 4;
                Err.ActualLength   = ActualLen;
                return Err;
            }
        }

        // ---------------------------------------------------------------------
        // 逐格子序 6：Utility DiceMultiplierTable 长度（AC-23f）
        // Utility 倍率数组固定 2 项：[单持倍率, 双持倍率]
        // ---------------------------------------------------------------------
        if (bIsUtility)
        {
            const int32 ActualLen = Tile.DiceMultiplierTable.Num();
            if (ActualLen != 2)
            {
                FBoardValidationError Err;
                Err.Code           = EBoardValidationError::DiceMultiplierLengthInvalid;
                Err.TileIndex      = Idx;
                Err.ExpectedLength = 2;
                Err.ActualLength   = ActualLen;
                return Err;
            }

            // -----------------------------------------------------------------
            // 逐格子序 7：Utility DiceMultiplierTable 元素越界（AC-23j）
            // 元素须在 [1, DICE_MULT_MAX] 内；<1 非法（须为正），>MAX 防 int32 溢出
            // 长度已通过（上方校验），此处元素必存在
            // -----------------------------------------------------------------
            for (const int32 MultValue : Tile.DiceMultiplierTable)
            {
                if (MultValue < 1 || MultValue > DICE_MULT_MAX)
                {
                    FBoardValidationError Err;
                    Err.Code           = EBoardValidationError::DiceMultiplierOutOfRange;
                    Err.TileIndex      = Idx;
                    Err.OffendingValue = MultValue; // 触发越界的具体元素值
                    return Err;
                }
            }
        }

        // ---------------------------------------------------------------------
        // 逐格子序 8：Property/Railroad 带 DiceMultiplierTable（AC-23g-a，反向校验）
        // Property/Railroad 使用 RentTable，DiceMultiplierTable 误填属数据污染
        // ---------------------------------------------------------------------
        if ((bIsProperty || bIsRailroad) && Tile.DiceMultiplierTable.Num() > 0)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::DiceMultiplierTableNotAllowed;
            Err.TileIndex = Idx;
            return Err;
        }

        // ---------------------------------------------------------------------
        // 逐格子序 9：Utility 带 RentTable（AC-23g-b，反向校验）
        // Utility 使用 DiceMultiplierTable，RentTable 误填属数据污染
        // ---------------------------------------------------------------------
        if (bIsUtility && Tile.RentTable.Num() > 0)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::RentTableNotAllowed;
            Err.TileIndex = Idx;
            return Err;
        }

        // ---------------------------------------------------------------------
        // 逐格子序 10：非可购买格带任一数组（AC-23g-c，反向校验）
        // 完全不可购买格不应携带任何租金/倍率数组（数据污染）
        // ---------------------------------------------------------------------
        if (!bIsPurchasable
            && (Tile.RentTable.Num() > 0 || Tile.DiceMultiplierTable.Num() > 0))
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::RentArrayOnNonPurchasable;
            Err.TileIndex = Idx;
            return Err;
        }

        // ---------------------------------------------------------------------
        // 逐格子序 11：Chance/CommunityChest 格 EventDeck == None（AC-23b）
        // 事件格必须绑定一个牌堆，否则事件系统(7)收到空引用
        // ---------------------------------------------------------------------
        if ((Type == ETileType::Chance || Type == ETileType::CommunityChest)
            && Tile.EventDeck == EEventDeck::None)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::EventDeckMissing;
            Err.TileIndex = Idx;
            return Err;
        }

        // ---------------------------------------------------------------------
        // 逐格子序 12：非 Go 格 SalaryAmount != 0（AC-23h，反向校验）
        // SalaryAmount 仅 Go 格适用；非 Go 格携带薪额是数据污染
        // 移动(4)只读 GetTileData(0).SalaryAmount，误填值虽不被读取但应在加载期拒绝
        // ---------------------------------------------------------------------
        if (Type != ETileType::Go && Tile.SalaryAmount != 0)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::SalaryOnNonGoTile;
            Err.TileIndex = Idx;
            return Err;
        }

        // ---------------------------------------------------------------------
        // 逐格子序 13：非 Tax 格 TaxAmount != 0（AC-23i，反向校验）
        // TaxAmount 仅 Tax 格适用，与 SalaryAmount 对称
        // ---------------------------------------------------------------------
        if (Type != ETileType::Tax && Tile.TaxAmount != 0)
        {
            FBoardValidationError Err;
            Err.Code      = EBoardValidationError::TaxOnNonTaxTile;
            Err.TileIndex = Idx;
            return Err;
        }
    }

    // 全部检查通过，返回成功
    return FBoardValidationError{}; // Code == None，IsValid() == true
}

// =============================================================================
// ValidateBoard — Blueprint 入口（转发给 ValidateTiles）
//
// null Board → 传空数组 → N=0 → BoardTooSmall（合理兜底，无 AC 冲突）
// =============================================================================
FBoardValidationError UBoardValidator::ValidateBoard(const UBoardDataAsset* Board)
{
    if (!Board)
    {
        // null DA：等价于 N=0，触发 BoardTooSmall
        static const TArray<FBoardTileData> EmptyTiles;
        return ValidateTiles(EmptyTiles);
    }
    return ValidateTiles(Board->Tiles);
}

// =============================================================================
// ValidateTilesWarnings — 警告类全收集校验（story-006，AC-24a/24b/24c/24d/35/36）
//
// 语义：全收集（非 fail-fast）。遍历结束后返回所有警告，空数组=无警告。
// 警告不阻止加载——本函数只返回警告列表，不返回 LoadFailed（状态机 → story-002）。
// 纯函数：无 UE_LOG 副作用。
//
// 收集顺序（确定性，测试可依赖）：
//   Pass 1（逐格）：per-tile 警告
//     1a. AC-24a EmptyDisplayName     — DisplayName 为空
//     1b. AC-24c MortgageRateHigh     — 可购买格抵押率 > 60%
//     1c. AC-35  RentTableNotMonotonic — Property RentTable 非单调不减
//   Pass 2（聚合）：group/corner 级警告
//     2a. AC-24d CornerTileCountUnusual         — Jail/FreeParking/GoToJail 各自数量 ≠ 1
//     2b. AC-36  BuildingCostMismatchInGroup    — 同色组 Property 格 BuildingCost 不一致
//     2c. AC-24b ColorGroupMemberCountMismatch  — 色组成员数与声明不符（仅当 ExpectedGroupSizes 非空）
// =============================================================================
TArray<FBoardValidationWarning> UBoardValidator::ValidateTilesWarnings(
    const TArray<FBoardTileData>& Tiles,
    const TMap<EColorGroup, int32>& ExpectedGroupSizes)
{
    TArray<FBoardValidationWarning> Warnings;

    // =========================================================================
    // Pass 1：逐格收集 per-tile 警告
    // =========================================================================
    for (const FBoardTileData& Tile : Tiles)
    {
        const int32 Idx     = Tile.TileIndex;
        const ETileType Type = Tile.TileType;

        // ---------------------------------------------------------------------
        // AC-24a：DisplayName 为空 → EmptyDisplayName{TileIndex}
        //
        // GDD §Edge Cases line 275：空名称发出警告，回退显示 "Tile {index}"；
        // 回退显示逻辑是 story-004 GetTileData 职责，本函数只产警告码。
        // ---------------------------------------------------------------------
        if (Tile.DisplayName.IsEmpty())
        {
            FBoardValidationWarning W;
            W.Code      = EBoardValidationWarning::EmptyDisplayName;
            W.TileIndex = Idx;
            // ColorGroup/TileType/Count 保持默认值（无语义）
            Warnings.Add(W);
        }

        // ---------------------------------------------------------------------
        // AC-24c：可购买格 MortgageValue > floor(PurchasePrice × 0.6) → MortgageRateHigh{TileIndex}
        //
        // 仅检查 Property/Railroad/Utility 格（非可购买格 PurchasePrice=0，无意义）。
        //
        // 整数运算实现 floor(PurchasePrice × 0.6)：
        //   (PurchasePrice * 6) / 10  ← C++ 整数除法下截断 = floor(price×0.6)
        //   等价于 FMath::FloorToInt32(PurchasePrice * 0.6f)，但避免浮点精度问题。
        //
        // 阈值锁定（story Implementation Notes 2）：
        //   (PurchasePrice=100, MortgageValue=61) → 100*6/10=60 → 61>60 → 警告 ✓
        //   (PurchasePrice=100, MortgageValue=60) → 60>60 → false → 不警告 ✓（`>` 非 `>=`）
        //
        // 注：MortgageValue > PurchasePrice 由 bd-005 AC-23e 拒绝，不会到达此检查；
        //     但本函数独立运行，不依赖拒绝先跑，仅在 MortgageValue ≤ PurchasePrice 时才会有意义。
        // ---------------------------------------------------------------------
        const bool bIsPurchasable = (Type == ETileType::Property
                                  || Type == ETileType::Railroad
                                  || Type == ETileType::Utility);
        if (bIsPurchasable)
        {
            // 整数截断：floor(PurchasePrice × 0.6)
            // int64 中间量（code-review W-1 硬化）：自定义盘 PurchasePrice 理论可超
            // INT32_MAX/6（约 3.58 亿）致 *6 溢出 int32；用 int64 中间量彻底消除，零成本。
            const int64 Threshold60Pct = (static_cast<int64>(Tile.PurchasePrice) * 6) / 10;
            if (static_cast<int64>(Tile.MortgageValue) > Threshold60Pct)
            {
                FBoardValidationWarning W;
                W.Code      = EBoardValidationWarning::MortgageRateHigh;
                W.TileIndex = Idx;
                Warnings.Add(W);
            }
        }

        // ---------------------------------------------------------------------
        // AC-35：Property 格 RentTable 非单调不减 → RentTableNotMonotonic{TileIndex}
        //
        // 存在 RentTable[i] > RentTable[i+1] 则触发。空表/单元素不触发。
        // 注：RentTable 长度合规性由 bd-005 AC-22 拒绝；本函数独立，不假设长度。
        // ---------------------------------------------------------------------
        if (Type == ETileType::Property)
        {
            const TArray<int32>& RT = Tile.RentTable;
            bool bNotMonotonic = false;
            for (int32 i = 0; i + 1 < RT.Num(); ++i)
            {
                if (RT[i] > RT[i + 1])
                {
                    bNotMonotonic = true;
                    break;
                }
            }
            if (bNotMonotonic)
            {
                FBoardValidationWarning W;
                W.Code      = EBoardValidationWarning::RentTableNotMonotonic;
                W.TileIndex = Idx;
                Warnings.Add(W);
            }
        }
    } // 逐格 Pass 1 结束

    // =========================================================================
    // Pass 2a：AC-24d — CornerTileCountUnusual{TileType, Count}
    //
    // 对 Jail / FreeParking / GoToJail 各自统计全盘数量。
    // 每类型独立：数量 ≠ 1 → 一条警告（Code=CornerTileCountUnusual, TileType=该类型, Count=实际数）。
    // 三类型各自独立产警告（如 2 个 FreeParking + 0 个 Jail → 产 2 条）。
    //
    // 注：GoToJail 存在但无 Jail 的硬拒绝是 bd-005 AC-23c（NoJailTile），本函数不重复。
    //     此处覆盖「数量异常但不致命」警告（如 2 个 FreeParking、0 个 Jail）。
    // =========================================================================
    {
        // 统计三类角格各自数量
        int32 JailCount        = 0;
        int32 FreeParkingCount = 0;
        int32 GoToJailCount    = 0;

        for (const FBoardTileData& Tile : Tiles)
        {
            switch (Tile.TileType)
            {
                case ETileType::Jail:        ++JailCount;        break;
                case ETileType::FreeParking: ++FreeParkingCount; break;
                case ETileType::GoToJail:    ++GoToJailCount;    break;
                default:                                         break;
            }
        }

        // 各自独立判断，数量 ≠ 1 → 产警告
        auto AddCornerWarningIfUnusual = [&](ETileType CornerType, int32 Count)
        {
            if (Count != 1)
            {
                FBoardValidationWarning W;
                W.Code      = EBoardValidationWarning::CornerTileCountUnusual;
                W.TileIndex = -1;     // 无单格定位
                W.TileType  = CornerType;
                W.Count     = Count;
                Warnings.Add(W);
            }
        };

        AddCornerWarningIfUnusual(ETileType::Jail,        JailCount);
        AddCornerWarningIfUnusual(ETileType::FreeParking, FreeParkingCount);
        AddCornerWarningIfUnusual(ETileType::GoToJail,    GoToJailCount);
    }

    // =========================================================================
    // Pass 2b：AC-36 — BuildingCostMismatchInGroup{ColorGroup}
    //
    // 按色组聚合所有 Property 格：同一色组（非 None）内各格 BuildingCost 不完全相同
    // → 该色组产一条警告。每个不一致色组一条。
    //
    // 算法：TMap<EColorGroup, TArray<int32>> 记录各色组已见的 BuildingCost 集合。
    //   遍历结束后对每个色组，若 TArray 中不是所有元素相同 → 产警告。
    // =========================================================================
    {
        // 聚合各色组的 BuildingCost 值集合
        TMap<EColorGroup, TArray<int32>> GroupBuildingCosts;

        for (const FBoardTileData& Tile : Tiles)
        {
            if (Tile.TileType == ETileType::Property
                && Tile.ColorGroup != EColorGroup::None)
            {
                GroupBuildingCosts.FindOrAdd(Tile.ColorGroup).Add(Tile.BuildingCost);
            }
        }

        // 对每个色组检查 BuildingCost 是否一致
        for (const auto& Pair : GroupBuildingCosts)
        {
            const EColorGroup Group       = Pair.Key;
            const TArray<int32>& Costs    = Pair.Value;

            // 单元素色组不存在不一致（无需检查）
            if (Costs.Num() < 2)
            {
                continue;
            }

            const int32 FirstCost = Costs[0];
            bool bMismatch        = false;
            for (int32 i = 1; i < Costs.Num(); ++i)
            {
                if (Costs[i] != FirstCost)
                {
                    bMismatch = true;
                    break;
                }
            }

            if (bMismatch)
            {
                FBoardValidationWarning W;
                W.Code       = EBoardValidationWarning::BuildingCostMismatchInGroup;
                W.TileIndex  = -1;    // 无单格定位，色组级别
                W.ColorGroup = Group;
                Warnings.Add(W);
            }
        }
    }

    // =========================================================================
    // Pass 2c：AC-24b — ColorGroupMemberCountMismatch{ColorGroup, Count}
    //
    // 仅当 ExpectedGroupSizes 非空时检查。
    // 对 map 中每个 (Group, ExpectedCount)：统计盘上该色组的 Property 成员数（actual）。
    // 若 actual != ExpectedCount → 警告（Code=ColorGroupMemberCountMismatch, ColorGroup=Group,
    //   Count=actual）。
    //
    // 数据驱动理由（ADR 裁定）：经典精确计数（棕2/浅蓝3/…）归 story-007 资产层（AC-6），
    // 不硬编码进本 generic 校验器；「声明」= 调用方注入的 ExpectedGroupSizes。
    // 空 map = 跳过此检查（自定义盘允许，GDD「自定义棋盘允许非经典组构成」）。
    // =========================================================================
    if (ExpectedGroupSizes.Num() > 0)
    {
        // 先统计盘上各色组的 Property 成员实际数量
        TMap<EColorGroup, int32> ActualGroupCounts;
        for (const FBoardTileData& Tile : Tiles)
        {
            if (Tile.TileType == ETileType::Property
                && Tile.ColorGroup != EColorGroup::None)
            {
                int32& Count = ActualGroupCounts.FindOrAdd(Tile.ColorGroup);
                ++Count;
            }
        }

        // 与声明对照，不符者产警告
        for (const auto& Expected : ExpectedGroupSizes)
        {
            const EColorGroup Group     = Expected.Key;
            const int32 ExpectedCount   = Expected.Value;

            // 若该色组在盘上不存在，ActualCount = 0
            const int32* ActualPtr  = ActualGroupCounts.Find(Group);
            const int32 ActualCount = ActualPtr ? *ActualPtr : 0;

            if (ActualCount != ExpectedCount)
            {
                FBoardValidationWarning W;
                W.Code       = EBoardValidationWarning::ColorGroupMemberCountMismatch;
                W.TileIndex  = -1;         // 无单格定位，色组级别
                W.ColorGroup = Group;
                W.Count      = ActualCount; // 实际成员数
                Warnings.Add(W);
            }
        }
    }

    return Warnings;
}

// =============================================================================
// ValidateBoardWarnings — Blueprint 入口（转发给 ValidateTilesWarnings）
//
// null Board → 返回空数组（无警告，合理兜底）
// =============================================================================
TArray<FBoardValidationWarning> UBoardValidator::ValidateBoardWarnings(
    const UBoardDataAsset* Board,
    const TMap<EColorGroup, int32>& ExpectedGroupSizes)
{
    if (!Board)
    {
        // null DA：视为无数据，返回空警告数组（不产 BoardTooSmall，语义上"没有棋盘可警告"）
        return TArray<FBoardValidationWarning>{};
    }
    return ValidateTilesWarnings(Board->Tiles, ExpectedGroupSizes);
}
