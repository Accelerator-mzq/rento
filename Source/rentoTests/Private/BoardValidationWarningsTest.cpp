// BoardValidationWarningsTest.cpp
// =============================================================================
// 单元测试：UBoardValidator 加载期警告类校验（story-006）
// 覆盖 AC-24a（EmptyDisplayName）/ AC-24b（ColorGroupMemberCountMismatch）/
//         AC-24c（MortgageRateHigh）/ AC-24d（CornerTileCountUnusual）/
//         AC-35（RentTableNotMonotonic）/ AC-36（BuildingCostMismatchInGroup）
//
// 物理路径：Source/rentoTests/Private/BoardValidationWarningsTest.cpp
// 逻辑分类：tests/unit/board/board_validation_warnings_test.cpp
// Automation 类目：Rento.Board.ValidationWarnings
//
// 测试策略（非 vacuous 要求）：
//   1. MakeCleanBoard() — 构造无任何警告的最小基线棋盘（N≥4）。
//      TC_Baseline_NoWarnings 先断言 ValidateTilesWarnings(MakeCleanBoard()).Num()==0，
//      证基线真干净，否则后续克隆断言不可信。
//   2. 每条 AC：克隆基线 + 只破坏一个字段 → 断言「加载成功语义」（函数正常返回）
//      + 数组含预期警告码 + 定位字段。
//   3. AC-24c 边界双 case：(100,61)→有警告；(100,60)→无警告（证 `>` 非 `>=`）。
//   4. AC-24b：传 ExpectedGroupSizes 时产警告；不传（空 map）时无警告（证参数注入语义）。
//   5. 「加载成功语义」契约验证：对同一破坏后的 fixture，ValidateTiles（拒绝类）返回
//      IsValid()==true，ValidateTilesWarnings 返回非空——证明警告不拒绝。
//
// 基线棋盘布局（N=6，最小无警告盘）：
//   Tile[0]：Go，SalaryAmount=200，DisplayName 非空
//   Tile[1]：Property，ColorGroup=Red，PurchasePrice=100，MortgageValue=50，
//             BuildingCost=150，RentTable=[2,10,30,90,160,250]（单调不减），DisplayName 非空
//   Tile[2]：Property，ColorGroup=Red，PurchasePrice=120，MortgageValue=60，
//             BuildingCost=150（与 Tile[1] 相同，Red 组一致），
//             RentTable=[4,20,60,180,320,450]（单调不减），DisplayName 非空
//   Tile[3]：Property，ColorGroup=Red，PurchasePrice=140，MortgageValue=70，
//             BuildingCost=150（Red 组全部相同），
//             RentTable=[6,30,90,270,400,550]（单调不减），DisplayName 非空
//   Tile[4]：Jail，DisplayName 非空
//   Tile[5]：Tax，TaxAmount=100，DisplayName 非空
//
// 基线特性（全无警告前提）：
//   - DisplayName 全非空（AC-24a 不触发）
//   - 可购买格 MortgageValue ≤ PurchasePrice×0.6（AC-24c 不触发）：
//       Tile[1]: 50 <= 60（100×0.6）✓；Tile[2]: 60 <= 72（120×0.6）✓；
//       Tile[3]: 70 <= 84（140×0.6）✓
//   - 四角格各恰 1：Jail×1、FreeParking×0（盘无此格，但…）
//       ⚠ 注意：基线无 FreeParking/GoToJail，FreeParking=0 和 GoToJail=0 均 ≠1，
//          → AC-24d 会触发！为避免基线有警告，基线须包含各恰一个角格。
//   → 调整：N=8，加 FreeParking(6) 和 GoToJail(7)（有 Jail，不触发 NoJailTile 拒绝）
//   - Red 组 BuildingCost 全部=150（AC-36 不触发）
//   - RentTable 全单调不减（AC-35 不触发）
//
// 最终基线 N=8：
//   Tile[0]=Go / Tile[1]=Property(Red) / Tile[2]=Property(Red) / Tile[3]=Property(Red)
//   Tile[4]=Railroad / Tile[5]=Jail / Tile[6]=FreeParking / Tile[7]=GoToJail
//   （Tax 格无需包含，简化基线；Railroad 使 RentTable[4] 合规测试不影响警告）
//
// 全部确定性，无随机/时间/文件 I/O。
// 警告函数为纯函数，无 UE_LOG 副作用，不需要 AddExpectedError。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "BoardValidator.h"

// =============================================================================
// 辅助函数：在警告数组中查找指定 Code 的第一条警告
//
// @param Warnings  ValidateTilesWarnings 返回的警告数组
// @param Code      要查找的警告码
// @return          指向找到的警告的指针（nullptr 表示未找到）
// =============================================================================
static const FBoardValidationWarning* FindWarning(
    const TArray<FBoardValidationWarning>& Warnings,
    EBoardValidationWarning Code)
{
    for (const FBoardValidationWarning& W : Warnings)
    {
        if (W.Code == Code)
        {
            return &W;
        }
    }
    return nullptr;
}

// =============================================================================
// MakeCleanBoard() — 构造无任何警告的最小基线棋盘（N=8）
//
// 精确保证所有警告检查均不触发（非 vacuous 要求，TC_Baseline_NoWarnings 将验证）：
//   - DisplayName 全非空                 → AC-24a 不触发
//   - MortgageValue ≤ PurchasePrice×0.6 → AC-24c 不触发（详见头注释）
//   - Jail×1 / FreeParking×1 / GoToJail×1 → AC-24d 不触发
//   - Red 组 BuildingCost 全=150         → AC-36 不触发
//   - RentTable 全单调不减               → AC-35 不触发
//   - 同时满足拒绝类规则（ValidateTiles 通过），以验证「加载成功+警告」契约测试
// =============================================================================
static TArray<FBoardTileData> MakeCleanBoard()
{
    TArray<FBoardTileData> Tiles;
    Tiles.SetNum(8);

    // ---- Tile[0]：起点格（Go，index=0）----
    Tiles[0].TileIndex     = 0;
    Tiles[0].TileType      = ETileType::Go;
    Tiles[0].ColorGroup    = EColorGroup::None;
    Tiles[0].PurchasePrice = 0;
    Tiles[0].MortgageValue = 0;
    Tiles[0].SalaryAmount  = 200;
    Tiles[0].TaxAmount     = 0;
    Tiles[0].EventDeck     = EEventDeck::None;
    Tiles[0].DisplayName   = FText::FromString(TEXT("Go"));

    // ---- Tile[1]：地产格（Property，Red 组，index=1）----
    // MortgageValue=50 <= 100×0.6=60 ✓（无 AC-24c 警告）
    Tiles[1].TileIndex     = 1;
    Tiles[1].TileType      = ETileType::Property;
    Tiles[1].ColorGroup    = EColorGroup::Red;
    Tiles[1].PurchasePrice = 100;
    Tiles[1].MortgageValue = 50;   // 50 <= 60（100×0.6）✓
    Tiles[1].BuildingCost  = 150;  // Red 组全部=150，AC-36 不触发
    Tiles[1].RentTable     = {2, 10, 30, 90, 160, 250}; // 6 项，单调不减 ✓
    Tiles[1].SalaryAmount  = 0;
    Tiles[1].TaxAmount     = 0;
    Tiles[1].EventDeck     = EEventDeck::None;
    Tiles[1].DisplayName   = FText::FromString(TEXT("Mediterranean Avenue"));

    // ---- Tile[2]：地产格（Property，Red 组，index=2）----
    // MortgageValue=60 <= 120×0.6=72 ✓（无 AC-24c 警告）
    Tiles[2].TileIndex     = 2;
    Tiles[2].TileType      = ETileType::Property;
    Tiles[2].ColorGroup    = EColorGroup::Red;
    Tiles[2].PurchasePrice = 120;
    Tiles[2].MortgageValue = 60;   // 60 <= 72（120×0.6）✓
    Tiles[2].BuildingCost  = 150;  // Red 组全部=150 ✓
    Tiles[2].RentTable     = {4, 20, 60, 180, 320, 450}; // 6 项，单调不减 ✓
    Tiles[2].SalaryAmount  = 0;
    Tiles[2].TaxAmount     = 0;
    Tiles[2].EventDeck     = EEventDeck::None;
    Tiles[2].DisplayName   = FText::FromString(TEXT("Baltic Avenue"));

    // ---- Tile[3]：地产格（Property，Red 组，index=3）----
    // MortgageValue=70 <= 140×0.6=84 ✓（无 AC-24c 警告）
    Tiles[3].TileIndex     = 3;
    Tiles[3].TileType      = ETileType::Property;
    Tiles[3].ColorGroup    = EColorGroup::Red;
    Tiles[3].PurchasePrice = 140;
    Tiles[3].MortgageValue = 70;   // 70 <= 84（140×0.6）✓
    Tiles[3].BuildingCost  = 150;  // Red 组全部=150 ✓
    Tiles[3].RentTable     = {6, 30, 90, 270, 400, 550}; // 6 项，单调不减 ✓
    Tiles[3].SalaryAmount  = 0;
    Tiles[3].TaxAmount     = 0;
    Tiles[3].EventDeck     = EEventDeck::None;
    Tiles[3].DisplayName   = FText::FromString(TEXT("Oriental Avenue"));

    // ---- Tile[4]：铁路格（Railroad，index=4）----
    // PurchasePrice=200, MortgageValue=100 ≤ 120（200×0.6）✓
    Tiles[4].TileIndex     = 4;
    Tiles[4].TileType      = ETileType::Railroad;
    Tiles[4].ColorGroup    = EColorGroup::None;
    Tiles[4].PurchasePrice = 200;
    Tiles[4].MortgageValue = 100;  // 100 <= 120（200×0.6）✓
    Tiles[4].RentTable     = {25, 50, 100, 200}; // 4 项 ✓
    Tiles[4].SalaryAmount  = 0;
    Tiles[4].TaxAmount     = 0;
    Tiles[4].EventDeck     = EEventDeck::None;
    Tiles[4].DisplayName   = FText::FromString(TEXT("Reading Railroad"));

    // ---- Tile[5]：监狱格（Jail，index=5）----
    // Jail×1 ✓（AC-24d 不触发）
    Tiles[5].TileIndex     = 5;
    Tiles[5].TileType      = ETileType::Jail;
    Tiles[5].ColorGroup    = EColorGroup::None;
    Tiles[5].PurchasePrice = 0;
    Tiles[5].MortgageValue = 0;
    Tiles[5].SalaryAmount  = 0;
    Tiles[5].TaxAmount     = 0;
    Tiles[5].EventDeck     = EEventDeck::None;
    Tiles[5].DisplayName   = FText::FromString(TEXT("Jail"));

    // ---- Tile[6]：免费停车格（FreeParking，index=6）----
    // FreeParking×1 ✓（AC-24d 不触发）
    Tiles[6].TileIndex     = 6;
    Tiles[6].TileType      = ETileType::FreeParking;
    Tiles[6].ColorGroup    = EColorGroup::None;
    Tiles[6].PurchasePrice = 0;
    Tiles[6].MortgageValue = 0;
    Tiles[6].SalaryAmount  = 0;
    Tiles[6].TaxAmount     = 0;
    Tiles[6].EventDeck     = EEventDeck::None;
    Tiles[6].DisplayName   = FText::FromString(TEXT("Free Parking"));

    // ---- Tile[7]：前往监狱格（GoToJail，index=7）----
    // GoToJail×1 ✓；盘内有 Jail（index=5），不触发 NoJailTile 拒绝 ✓
    Tiles[7].TileIndex     = 7;
    Tiles[7].TileType      = ETileType::GoToJail;
    Tiles[7].ColorGroup    = EColorGroup::None;
    Tiles[7].PurchasePrice = 0;
    Tiles[7].MortgageValue = 0;
    Tiles[7].SalaryAmount  = 0;
    Tiles[7].TaxAmount     = 0;
    Tiles[7].EventDeck     = EEventDeck::None;
    Tiles[7].DisplayName   = FText::FromString(TEXT("Go To Jail"));

    return Tiles;
}

// =============================================================================
// TC_Baseline_NoWarnings — 正路径：证基线棋盘无任何警告（非 vacuous 前提）
//
// 若此测试失败，所有后续克隆测试的结论均不可信（基线本身已有警告）。
// 必须在所有警告类测试之前通过。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_Baseline_NoWarnings,
    "Rento.Board.ValidationWarnings.TC_Baseline_NoWarnings",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_Baseline_NoWarnings::RunTest(const FString& Parameters)
{
    const TArray<FBoardTileData> Tiles = MakeCleanBoard();
    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    // 基线棋盘必须零警告，否则后续 AC 测试结论不可信
    TestEqual(
        TEXT("Baseline: MakeCleanBoard() 应产生 0 条警告"),
        Warnings.Num(), 0);

    // 同时验证基线也通过拒绝类校验（否则「加载成功」语义测试的前提不成立）
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("Baseline: MakeCleanBoard() 应同时通过拒绝类校验（IsValid()==true）"),
        RejectResult.IsValid());

    return true;
}

// =============================================================================
// TC_AC24a_EmptyDisplayName — AC-24a：DisplayName 为空 → 警告 + 加载成功
//
// GIVEN：基线克隆，Tile[1]（Property，TileIndex=1）DisplayName 改为空
// WHEN：ValidateTilesWarnings
// THEN：返回数组含 EmptyDisplayName{TileIndex=1}；ValidateTiles（拒绝类）仍 IsValid()==true
//
// 非 vacuous：基线 TC_Baseline_NoWarnings 证所有 DisplayName 非空无警告，
//   此处精确触发一格空名称。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC24a_EmptyDisplayName,
    "Rento.Board.ValidationWarnings.TC_AC24a_EmptyDisplayName",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC24a_EmptyDisplayName::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // 破坏：Property 格（数组序 Tiles[1]，TileIndex==1）DisplayName 改为空
    Tiles[1].DisplayName = FText::GetEmpty();

    // ---- 警告类断言 ----
    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    // 数组非空（至少有一条警告）
    TestTrue(TEXT("AC-24a: 警告数组应非空（至少 1 条）"), Warnings.Num() >= 1);

    // 在数组中找到 EmptyDisplayName 警告
    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::EmptyDisplayName);
    TestNotNull(TEXT("AC-24a: 警告数组应含 EmptyDisplayName"), W);

    if (W)
    {
        // 验证定位字段：TileIndex 应为 1
        TestEqual(TEXT("AC-24a: EmptyDisplayName.TileIndex 应为 1"), W->TileIndex, 1);
    }

    // ---- 「加载成功」语义断言：同一破坏后，拒绝类 ValidateTiles 仍通过 ----
    // 这坐实「警告不拒绝」契约：DisplayName 为空只产警告，不拒绝加载
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("AC-24a: 「加载成功」语义——DisplayName 为空不触发拒绝类，ValidateTiles 仍 IsValid()==true"),
        RejectResult.IsValid());

    return true;
}

// =============================================================================
// TC_AC24c_MortgageRateHigh_Above — AC-24c 正路径：(PurchasePrice=100, MortgageValue=61)
//                                    → 产生 MortgageRateHigh 警告
//
// GIVEN：基线克隆，Tile[1]（Property，TileIndex=1，PurchasePrice=100）
//        MortgageValue 改为 61（> 100×0.6=60，超阈值）
// WHEN：ValidateTilesWarnings
// THEN：返回含 MortgageRateHigh{TileIndex=1}；ValidateTiles 仍 IsValid()==true
//
// 非 vacuous：61 > floor(100×0.6)=60，精确超界。基线 Tile[1] MortgageValue=50（不触发）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC24c_MortgageRateHigh_Above,
    "Rento.Board.ValidationWarnings.TC_AC24c_MortgageRateHigh_Above",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC24c_MortgageRateHigh_Above::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // 破坏：Tile[1] PurchasePrice=100，MortgageValue 改为 61（> 60=floor(100×0.6)）
    Tiles[1].PurchasePrice = 100;
    Tiles[1].MortgageValue = 61; // 61 > 60 → 触发 MortgageRateHigh

    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::MortgageRateHigh);
    TestNotNull(TEXT("AC-24c 上界: (100,61) 应产生 MortgageRateHigh 警告"), W);

    if (W)
    {
        TestEqual(TEXT("AC-24c 上界: MortgageRateHigh.TileIndex 应为 1"), W->TileIndex, 1);
    }

    // 「加载成功」语义：MortgageValue=61 < PurchasePrice=100，不触发拒绝类 MortgageExceedsPurchase
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("AC-24c: 「加载成功」语义——MortgageValue=61 不触发拒绝类，ValidateTiles 仍 IsValid()==true"),
        RejectResult.IsValid());

    return true;
}

// =============================================================================
// TC_AC24c_MortgageRateHigh_Boundary_NoWarning — AC-24c 边界正路径（证 `>` 非 `>=`）
//
// GIVEN：基线克隆，Tile[1]（PurchasePrice=100）MortgageValue 改为 60（= 100×0.6）
// WHEN：ValidateTilesWarnings
// THEN：**不**产生 MortgageRateHigh 警告（60 == floor(100×0.6)=60，非 >，不触发）
//
// 非 vacuous：若实现误用 `>=` 而非 `>`，此 case (100,60) 会产生 MortgageRateHigh → 测试 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC24c_MortgageRateHigh_Boundary_NoWarning,
    "Rento.Board.ValidationWarnings.TC_AC24c_MortgageRateHigh_Boundary_NoWarning",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC24c_MortgageRateHigh_Boundary_NoWarning::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // 边界值：MortgageValue = floor(PurchasePrice × 0.6) = 60（恰好等于阈值，`>` 不触发）
    Tiles[1].PurchasePrice = 100;
    Tiles[1].MortgageValue = 60; // 60 == 60 → `60 > 60` = false → 不触发

    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    // 断言：不应含 MortgageRateHigh（验证 `>` 而非 `>=`）
    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::MortgageRateHigh);
    TestNull(TEXT("AC-24c 边界正路径: (100,60) 不应产生 MortgageRateHigh（边界不触发，`>` 非 `>=`）"), W);

    return true;
}

// =============================================================================
// TC_AC24d_CornerTileCountUnusual_FreeParking2 — AC-24d：2 个 FreeParking → 警告
//
// GIVEN：基线克隆，追加一格 FreeParking（index=8），使全盘有 2 个 FreeParking
// WHEN：ValidateTilesWarnings
// THEN：返回含 CornerTileCountUnusual{TileType=FreeParking, Count=2}
//        ValidateTiles 仍 IsValid()==true（不拒绝）
//
// 非 vacuous：基线 FreeParking×1（TC_Baseline_NoWarnings 验证不触发）；
//   追加一格后 FreeParking×2 → 精确触发，Count==2。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC24d_CornerTileCountUnusual_FreeParking2,
    "Rento.Board.ValidationWarnings.TC_AC24d_CornerTileCountUnusual_FreeParking2",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC24d_CornerTileCountUnusual_FreeParking2::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard(); // N=8

    // 追加第二个 FreeParking 格（index=8，N 变为 9）
    FBoardTileData ExtraFP;
    ExtraFP.TileIndex     = 8;
    ExtraFP.TileType      = ETileType::FreeParking;
    ExtraFP.ColorGroup    = EColorGroup::None;
    ExtraFP.PurchasePrice = 0;
    ExtraFP.MortgageValue = 0;
    ExtraFP.SalaryAmount  = 0;
    ExtraFP.TaxAmount     = 0;
    ExtraFP.EventDeck     = EEventDeck::None;
    ExtraFP.DisplayName   = FText::FromString(TEXT("Free Parking 2"));
    Tiles.Add(ExtraFP);

    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    // 查找 CornerTileCountUnusual 警告
    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::CornerTileCountUnusual);
    TestNotNull(TEXT("AC-24d: 2 个 FreeParking 应产生 CornerTileCountUnusual"), W);

    if (W)
    {
        // 断言类型和数量定位字段
        TestEqual(TEXT("AC-24d: CornerTileCountUnusual.TileType 应为 FreeParking"),
            W->TileType, ETileType::FreeParking);
        TestEqual(TEXT("AC-24d: CornerTileCountUnusual.Count 应为 2 (实际格子数)"),
            W->Count, 2);
        TestEqual(TEXT("AC-24d: CornerTileCountUnusual.TileIndex 应为 -1（无单格定位）"),
            W->TileIndex, -1);
    }

    // 「加载成功」语义：多一个 FreeParking 不触发拒绝类
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("AC-24d: 「加载成功」语义——2 个 FreeParking 不触发拒绝类，ValidateTiles 仍 IsValid()==true"),
        RejectResult.IsValid());

    return true;
}

// =============================================================================
// TC_AC35_RentTableNotMonotonic — AC-35：RentTable 非单调不减 → 警告 + 加载成功
//
// GIVEN：基线克隆，Tile[1]（Property，TileIndex=1）
//        RentTable 改为 [2, 10, 8, 90, 160, 250]（i=1=10 > i=2=8，非单调）
// WHEN：ValidateTilesWarnings
// THEN：返回含 RentTableNotMonotonic{TileIndex=1}；ValidateTiles 仍 IsValid()==true
//
// 非 vacuous：基线 TC_Baseline_NoWarnings 证单调 RentTable 无警告；
//   此处精确引入 RentTable[1]=10 > RentTable[2]=8 的单调性违反。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC35_RentTableNotMonotonic,
    "Rento.Board.ValidationWarnings.TC_AC35_RentTableNotMonotonic",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC35_RentTableNotMonotonic::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // 破坏：Tile[1] RentTable 引入非单调点：[2, 10, 8, 90, 160, 250]（i=1 > i=2）
    Tiles[1].RentTable = {2, 10, 8, 90, 160, 250};

    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::RentTableNotMonotonic);
    TestNotNull(TEXT("AC-35: RentTable[1]=10 > RentTable[2]=8 应产生 RentTableNotMonotonic"), W);

    if (W)
    {
        TestEqual(TEXT("AC-35: RentTableNotMonotonic.TileIndex 应为 1"), W->TileIndex, 1);
    }

    // 「加载成功」语义：非单调 RentTable 不触发拒绝类（拒绝类只检查长度，不检查单调性）
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("AC-35: 「加载成功」语义——非单调 RentTable 不触发拒绝类，ValidateTiles 仍 IsValid()==true"),
        RejectResult.IsValid());

    return true;
}

// =============================================================================
// TC_AC35_RentTableMonotonic_NoWarning — AC-35 正路径（无警告验证）
//
// GIVEN：基线（RentTable 全单调不减）
// WHEN：ValidateTilesWarnings
// THEN：不产生 RentTableNotMonotonic 警告
//
// 此 case 由 TC_Baseline_NoWarnings 隐含覆盖，但独立明示更清晰。
// 考虑到 TC_Baseline_NoWarnings 已覆盖，本 case 作为单调正路径显式记录。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC35_RentTableMonotonic_NoWarning,
    "Rento.Board.ValidationWarnings.TC_AC35_RentTableMonotonic_NoWarning",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC35_RentTableMonotonic_NoWarning::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard();
    // 基线 RentTable 已单调，无需破坏

    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::RentTableNotMonotonic);
    TestNull(TEXT("AC-35 正路径: 单调 RentTable 不应产生 RentTableNotMonotonic"), W);

    return true;
}

// =============================================================================
// TC_AC36_BuildingCostMismatchInGroup — AC-36：同组 BuildingCost 不一致 → 警告
//
// GIVEN：基线克隆，Tile[3]（Property，Red 组，TileIndex=3）BuildingCost 改为 200
//        （基线 Red 组：Tile[1].BuildingCost=150 / Tile[2].BuildingCost=150 / Tile[3].BuildingCost=200）
// WHEN：ValidateTilesWarnings
// THEN：返回含 BuildingCostMismatchInGroup{ColorGroup=Red}；ValidateTiles 仍 IsValid()==true
//
// 非 vacuous：基线 TC_Baseline_NoWarnings 证 Red 组全部=150 无警告；
//   改一格为 200 → 精确触发不一致。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC36_BuildingCostMismatchInGroup,
    "Rento.Board.ValidationWarnings.TC_AC36_BuildingCostMismatchInGroup",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC36_BuildingCostMismatchInGroup::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // 破坏：Tile[3]（Red 组）BuildingCost 改为 200（与 Tile[1/2] 的 150 不一致）
    // Red 组现在：[150, 150, 200] → 不完全相同 → 触发 BuildingCostMismatchInGroup{Red}
    Tiles[3].BuildingCost = 200;

    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::BuildingCostMismatchInGroup);
    TestNotNull(TEXT("AC-36: Red 组 BuildingCost=[150,150,200] 应产生 BuildingCostMismatchInGroup"), W);

    if (W)
    {
        TestEqual(TEXT("AC-36: BuildingCostMismatchInGroup.ColorGroup 应为 Red"),
            W->ColorGroup, EColorGroup::Red);
        TestEqual(TEXT("AC-36: BuildingCostMismatchInGroup.TileIndex 应为 -1（色组级，无单格定位）"),
            W->TileIndex, -1);
    }

    // 「加载成功」语义：同组 BuildingCost 不一致不触发拒绝类
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("AC-36: 「加载成功」语义——同组 BuildingCost 不一致不触发拒绝类，ValidateTiles 仍 IsValid()==true"),
        RejectResult.IsValid());

    return true;
}

// =============================================================================
// TC_AC24b_ColorGroupMemberCountMismatch_WithMap — AC-24b：传 ExpectedGroupSizes → 产警告
//
// GIVEN：基线有 Red 组 3 个 Property 格；
//        传 ExpectedGroupSizes={Brown:2}——声明 Brown 应有 2 个，但盘上 Brown=0
// WHEN：ValidateTilesWarnings（传入 ExpectedGroupSizes={Brown:2}）
// THEN：返回含 ColorGroupMemberCountMismatch{ColorGroup=Brown, Count=0}
//        ValidateTiles 仍 IsValid()==true
//
// 非 vacuous（Brown=0 实际 ≠ 声明 2）：
//   Count==0 精确验证「色组不存在时 actual=0」语义。
//
// 亦可作为 Red 组变体：若用 {Red:2}，盘上 Red=3 ≠ 2 → Count==3。
// 本 case 选 Brown（不存在于盘上），验证 not-found → actual=0 分支。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC24b_ColorGroupMemberCountMismatch_WithMap,
    "Rento.Board.ValidationWarnings.TC_AC24b_ColorGroupMemberCountMismatch_WithMap",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC24b_ColorGroupMemberCountMismatch_WithMap::RunTest(const FString& Parameters)
{
    const TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // 声明 Brown 组应有 2 个，但基线盘上 Brown=0（所有 Property 均为 Red 组）
    TMap<EColorGroup, int32> ExpectedSizes;
    ExpectedSizes.Add(EColorGroup::Brown, 2);

    const TArray<FBoardValidationWarning> Warnings =
        UBoardValidator::ValidateTilesWarnings(Tiles, ExpectedSizes);

    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::ColorGroupMemberCountMismatch);
    TestNotNull(TEXT("AC-24b: Brown 声明=2，实际=0 应产生 ColorGroupMemberCountMismatch"), W);

    if (W)
    {
        TestEqual(TEXT("AC-24b: ColorGroupMemberCountMismatch.ColorGroup 应为 Brown"),
            W->ColorGroup, EColorGroup::Brown);
        TestEqual(TEXT("AC-24b: ColorGroupMemberCountMismatch.Count 应为 0（盘上 Brown=0）"),
            W->Count, 0);
        TestEqual(TEXT("AC-24b: ColorGroupMemberCountMismatch.TileIndex 应为 -1（色组级，无单格定位）"),
            W->TileIndex, -1);
    }

    // 「加载成功」语义：色组成员数不符不触发拒绝类
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("AC-24b: 「加载成功」语义——色组成员数不符不触发拒绝类，ValidateTiles 仍 IsValid()==true"),
        RejectResult.IsValid());

    return true;
}

// =============================================================================
// TC_AC24b_NoExpectedGroupSizes_NoWarning — AC-24b 参数注入语义：
//   不传 ExpectedGroupSizes（空 map）→ 跳过检查，不产生 ColorGroupMemberCountMismatch
//
// GIVEN：基线棋盘（Red 组 3 个 Property）；ValidateTilesWarnings 不传 ExpectedGroupSizes
// WHEN：ValidateTilesWarnings（默认参数，即空 map）
// THEN：不产生 ColorGroupMemberCountMismatch 警告（跳过此检查）
//
// 非 vacuous：证参数注入语义——不声明 = 不检查，而非"0 声明 = 全警告"。
// 若实现忽略 ExpectedGroupSizes 为空的条件，此 case 会产生 ColorGroupMemberCountMismatch → FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC24b_NoExpectedGroupSizes_NoWarning,
    "Rento.Board.ValidationWarnings.TC_AC24b_NoExpectedGroupSizes_NoWarning",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC24b_NoExpectedGroupSizes_NoWarning::RunTest(const FString& Parameters)
{
    const TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // 不传 ExpectedGroupSizes（使用默认参数 = 空 map）→ 跳过 AC-24b 检查
    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::ColorGroupMemberCountMismatch);
    TestNull(
        TEXT("AC-24b 空 map: 不传 ExpectedGroupSizes 时不应产生 ColorGroupMemberCountMismatch"),
        W);

    return true;
}

// =============================================================================
// TC_AC24b_ColorGroupMemberCountMismatch_ActualCount3 — AC-24b 变体：
//   声明 {Red:2}，实际 Red=3 → Count==3
//
// GIVEN：基线有 Red 组 3 个 Property 格；传 ExpectedGroupSizes={Red:2}
// WHEN：ValidateTilesWarnings
// THEN：ColorGroupMemberCountMismatch{ColorGroup=Red, Count=3}（实际成员数=3）
//
// 补充覆盖：TC_AC24b_ColorGroupMemberCountMismatch_WithMap 测试 Count=0 分支；
//   本 case 测试 Count > ExpectedCount 分支（Red 组确实存在）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC24b_ColorGroupMemberCountMismatch_ActualCount3,
    "Rento.Board.ValidationWarnings.TC_AC24b_ColorGroupMemberCountMismatch_ActualCount3",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC24b_ColorGroupMemberCountMismatch_ActualCount3::RunTest(const FString& Parameters)
{
    const TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // 声明 Red 组应有 2 个，实际 Red=3 → Count=3 ≠ 2 → 产警告
    TMap<EColorGroup, int32> ExpectedSizes;
    ExpectedSizes.Add(EColorGroup::Red, 2);

    const TArray<FBoardValidationWarning> Warnings =
        UBoardValidator::ValidateTilesWarnings(Tiles, ExpectedSizes);

    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::ColorGroupMemberCountMismatch);
    TestNotNull(TEXT("AC-24b 变体: Red 声明=2，实际=3 应产生 ColorGroupMemberCountMismatch"), W);

    if (W)
    {
        TestEqual(TEXT("AC-24b 变体: ColorGroup 应为 Red"), W->ColorGroup, EColorGroup::Red);
        // 实际 Red 组成员数=3（Tile[1/2/3] 均为 Red Property）
        TestEqual(TEXT("AC-24b 变体: Count 应为 3（实际 Red 组成员数）"), W->Count, 3);
    }

    // 「加载成功」语义（code-review INFO 补齐，与其余两 AC-24b case 一致）
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("AC-24b 变体: 「加载成功」语义——色组成员数不符不触发拒绝类，ValidateTiles 仍 IsValid()==true"),
        RejectResult.IsValid());

    return true;
}

// =============================================================================
// ↓↓↓ code-review BD-006 补强测试（闭合真覆盖缺口，2026-06-07）↓↓↓
// =============================================================================

// =============================================================================
// TC_AC24d_CornerTileCountUnusual_ZeroFreeParking — AC-24d Count=0 分支（BLOCKING-2）
//
// GIVEN：构造无 FreeParking 格的盘（FreeParkingCount=0）
// WHEN：ValidateTilesWarnings
// THEN：含 CornerTileCountUnusual{TileType=FreeParking, Count=0}
//
// 非 vacuous：实现 `Count != 1` 同覆盖 0 与 ≥2；原测试仅验 =2 分支。
//   若实现误改为 `Count > 1`，Count=0 不再产警告 → 本测试 FAIL（守住 !=1 语义）。
//   构造：基线去掉 FreeParking(Tile[6]) 改成 Tax（保持 N=8 索引连续、不引入拒绝类）。
//   注：不能去掉 Jail/GoToJail（GoToJail 无 Jail 触 bd-005 NoJailTile 硬拒绝）；
//       FreeParking 缺失不触发任何拒绝类，是干净的 Count=0 warning 验证点。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC24d_CornerTileCountUnusual_ZeroFreeParking,
    "Rento.Board.ValidationWarnings.TC_AC24d_CornerTileCountUnusual_ZeroFreeParking",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC24d_CornerTileCountUnusual_ZeroFreeParking::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard(); // N=8，FreeParking 在 Tile[6]

    // 破坏：把唯一 FreeParking（Tile[6]）改成 Tax → FreeParkingCount=0
    // Tax 格无数组/无色组/无薪，不引入任何拒绝类问题；TaxAmount 设正值（Tax 格允许）
    Tiles[6].TileType  = ETileType::Tax;
    Tiles[6].TaxAmount = 100;

    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::CornerTileCountUnusual);
    TestNotNull(TEXT("AC-24d Count=0: 0 个 FreeParking 应产生 CornerTileCountUnusual"), W);

    if (W)
    {
        TestEqual(TEXT("AC-24d Count=0: TileType 应为 FreeParking"), W->TileType, ETileType::FreeParking);
        TestEqual(TEXT("AC-24d Count=0: Count 应为 0"), W->Count, 0);
    }

    // 「加载成功」语义：FreeParking 缺失不触发拒绝类
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("AC-24d Count=0: 「加载成功」语义——FreeParking 缺失不触发拒绝类"),
        RejectResult.IsValid());

    return true;
}

// =============================================================================
// TC_MultipleWarnings_CollectAll — 全收集语义（GAP-1，警告器核心设计属性）
//
// GIVEN：同一盘同时破坏 DisplayName（AC-24a）+ Red 组 BuildingCost 不一致（AC-36）
// WHEN：ValidateTilesWarnings
// THEN：返回数组同时含 EmptyDisplayName 和 BuildingCostMismatchInGroup 两条
//
// 非 vacuous：直接证「全收集非 fail-fast」——若实现误在首条警告后 return，
//   则只会返回 1 条，本测试断言「两码都在」会 FAIL。这是警告器区别于拒绝类
//   （fail-fast）的核心契约，单一破坏测试无法守住。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_MultipleWarnings_CollectAll,
    "Rento.Board.ValidationWarnings.TC_MultipleWarnings_CollectAll",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_MultipleWarnings_CollectAll::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // 破坏 1（AC-24a）：Tile[1] DisplayName 置空
    Tiles[1].DisplayName = FText::GetEmpty();
    // 破坏 2（AC-36）：Tile[3] BuildingCost 改 200，使 Red 组 [150,150,200] 不一致
    Tiles[3].BuildingCost = 200;

    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    // 全收集：两条不同警告码均应在数组中
    const FBoardValidationWarning* WEmpty = FindWarning(Warnings, EBoardValidationWarning::EmptyDisplayName);
    const FBoardValidationWarning* WCost  = FindWarning(Warnings, EBoardValidationWarning::BuildingCostMismatchInGroup);

    TestNotNull(TEXT("全收集: 应含 EmptyDisplayName（AC-24a）"), WEmpty);
    TestNotNull(TEXT("全收集: 应含 BuildingCostMismatchInGroup（AC-36）"), WCost);
    // 至少 2 条（证非 fail-fast 提前 return）
    TestTrue(TEXT("全收集: 警告数应 >= 2（证非 fail-fast 全收集）"), Warnings.Num() >= 2);

    return true;
}

// =============================================================================
// TC_ValidateBoardWarnings_NullBoard_EmptyArray — BP 入口 null 兜底（GAP-2）
//
// GIVEN：ValidateBoardWarnings(nullptr, {})
// WHEN：校验
// THEN：返回空数组（null → 无棋盘可警告，合理兜底，不崩溃）
//
// 非 vacuous：守住 ValidateBoardWarnings 的 null-guard，防重构破坏（如改成解引用崩溃）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_ValidateBoardWarnings_NullBoard_EmptyArray,
    "Rento.Board.ValidationWarnings.TC_ValidateBoardWarnings_NullBoard_EmptyArray",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_ValidateBoardWarnings_NullBoard_EmptyArray::RunTest(const FString& Parameters)
{
    const TMap<EColorGroup, int32> EmptyExpected;
    const TArray<FBoardValidationWarning> Warnings =
        UBoardValidator::ValidateBoardWarnings(nullptr, EmptyExpected);

    TestEqual(TEXT("null Board → 空警告数组（兜底，不崩溃）"), Warnings.Num(), 0);

    return true;
}

// =============================================================================
// TC_AC24c_MortgageRateHigh_Railroad — AC-24c Railroad 类型路径（SUGGESTION-1）
//
// GIVEN：基线克隆，Railroad 格（Tile[4]，PurchasePrice=200）MortgageValue 改 121（> 120=floor(200×0.6)）
// WHEN：ValidateTilesWarnings
// THEN：含 MortgageRateHigh{TileIndex=4}
//
// 非 vacuous：bIsPurchasable = Property||Railroad||Utility；原测试仅过 Property 操作数，
//   本测试过 Railroad 操作数，证类型守卫 || 中段分支。121 > 120 精确超界。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidationWarnings_TC_AC24c_MortgageRateHigh_Railroad,
    "Rento.Board.ValidationWarnings.TC_AC24c_MortgageRateHigh_Railroad",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidationWarnings_TC_AC24c_MortgageRateHigh_Railroad::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeCleanBoard();

    // Railroad 格 Tile[4]：PurchasePrice=200，阈值=floor(200×0.6)=120；MortgageValue=121 > 120 → 触发
    Tiles[4].MortgageValue = 121;

    const TArray<FBoardValidationWarning> Warnings = UBoardValidator::ValidateTilesWarnings(Tiles);

    const FBoardValidationWarning* W = FindWarning(Warnings, EBoardValidationWarning::MortgageRateHigh);
    TestNotNull(TEXT("AC-24c Railroad: (200,121) 应产生 MortgageRateHigh（Railroad 类型路径）"), W);
    if (W)
    {
        TestEqual(TEXT("AC-24c Railroad: MortgageRateHigh.TileIndex 应为 4"), W->TileIndex, 4);
    }

    // 「加载成功」语义：MortgageValue=121 < PurchasePrice=200，不触发拒绝类
    const FBoardValidationError RejectResult = UBoardValidator::ValidateTiles(Tiles);
    TestTrue(
        TEXT("AC-24c Railroad: 「加载成功」语义——121 < 200 不触发拒绝类"),
        RejectResult.IsValid());

    return true;
}
