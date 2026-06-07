// BoardValidationRejectionsTest.cpp
// =============================================================================
// 单元测试：UBoardValidator 加载期拒绝类校验 + 结构化错误码
// story-005 AC-18/19a/19b/20/21/22/22b/23/23b/23c/23d/23e/23f/23g-a/23g-b/23g-c
//          /23h/23i/23j/32 全量确定性 fixture covered。
//
// 物理路径：Source/rentoTests/Private/BoardValidationRejectionsTest.cpp
// 逻辑分类：tests/unit/board/board_validation_rejections_test.cpp
// Automation 类目：Rento.Board.ValidationRejections
//
// 测试策略（非 vacuous 要求）：
//   1. MakeValidBoard() — 构造通过全部拒绝校验的最小基线棋盘（N=6）。
//      基线 AC：TC_Baseline_ValidBoard 先断言 IsValid()==true，证基线干净。
//      此断言非 vacuous：若基线本身有校验错误，后续克隆破坏 fixture 会产生错误的测试结论。
//   2. 每条 AC：克隆基线 + 只破坏一个字段 → 断言精确 Code + 定位字段。
//      破坏策略保证单因素隔离（只改一个字段触发一个错误）。
//
// 基线棋盘布局（N=6，最小合规盘）：
//   Tile[0]：TileIndex=0, TileType=Go,          SalaryAmount=200，无数组
//   Tile[1]：TileIndex=1, TileType=Property,     ColorGroup=Brown, PurchasePrice=60,
//             MortgageValue=30, RentTable=[2,10,30,90,160,250]，无 DiceMult
//   Tile[2]：TileIndex=2, TileType=Railroad,     PurchasePrice=200,
//             MortgageValue=100, RentTable=[25,50,100,200]，无 DiceMult
//   Tile[3]：TileIndex=3, TileType=Utility,      PurchasePrice=150,
//             MortgageValue=75, DiceMultiplierTable=[4,10]，无 RentTable
//   Tile[4]：TileIndex=4, TileType=Jail,         无数组，无薪/税
//   Tile[5]：TileIndex=5, TileType=Tax,          TaxAmount=100，无数组
//
// 全部确定性，无随机/时间/文件 I/O。
// 校验函数为纯函数，无 UE_LOG 副作用，不需要 AddExpectedError。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "BoardValidator.h"

// =============================================================================
// 基线棋盘构造函数 MakeValidBoard()
//
// 返回通过全部拒绝校验的最小棋盘（N=6）：
//   - N>=4 ✓
//   - TileIndex 恰好覆盖 [0,5] 各一次 ✓
//   - index0 = Go ✓，且全盘恰好 1 个 Go ✓
//   - 含 Jail 格（无 GoToJail，故 NoJailTile 检查跳过）✓
//   - Property 有 ColorGroup、RentTable[6]、PurchasePrice>0、Mortgage<=Purchase ✓
//   - Railroad 有 RentTable[4]、PurchasePrice>0、Mortgage<=Purchase ✓
//   - Utility 有 DiceMultiplierTable[2]（元素在[1,DICE_MULT_MAX]）、PurchasePrice>0 ✓
//   - Go 格 SalaryAmount=200，其余格 SalaryAmount=0 ✓
//   - Tax 格 TaxAmount=100，其余格 TaxAmount=0 ✓
//   - 无 Chance/CommunityChest，无 EventDeck 问题 ✓
//   - 无反向污染（无色组在非Property、无数组在非可购买格） ✓
// =============================================================================
static TArray<FBoardTileData> MakeValidBoard()
{
    TArray<FBoardTileData> Tiles;
    Tiles.SetNum(6);

    // ---- Tile[0]：起点格（Go，index=0）----
    Tiles[0].TileIndex     = 0;
    Tiles[0].TileType      = ETileType::Go;
    Tiles[0].ColorGroup    = EColorGroup::None;
    Tiles[0].PurchasePrice = 0;
    Tiles[0].MortgageValue = 0;
    Tiles[0].SalaryAmount  = 200; // Go 格薪资
    Tiles[0].TaxAmount     = 0;
    Tiles[0].EventDeck     = EEventDeck::None;

    // ---- Tile[1]：地产格（Property，index=1）----
    Tiles[1].TileIndex     = 1;
    Tiles[1].TileType      = ETileType::Property;
    Tiles[1].ColorGroup    = EColorGroup::Brown; // Property 须有非 None 色组
    Tiles[1].PurchasePrice = 60;
    Tiles[1].MortgageValue = 30;                 // 30 <= 60 ✓
    Tiles[1].BuildingCost  = 50;
    Tiles[1].RentTable     = {2, 10, 30, 90, 160, 250}; // 6 项 ✓
    Tiles[1].SalaryAmount  = 0;
    Tiles[1].TaxAmount     = 0;
    Tiles[1].EventDeck     = EEventDeck::None;

    // ---- Tile[2]：铁路格（Railroad，index=2）----
    Tiles[2].TileIndex     = 2;
    Tiles[2].TileType      = ETileType::Railroad;
    Tiles[2].ColorGroup    = EColorGroup::None; // Railroad 不应有色组 ✓
    Tiles[2].PurchasePrice = 200;
    Tiles[2].MortgageValue = 100;               // 100 <= 200 ✓
    Tiles[2].RentTable     = {25, 50, 100, 200}; // 4 项 ✓
    Tiles[2].SalaryAmount  = 0;
    Tiles[2].TaxAmount     = 0;
    Tiles[2].EventDeck     = EEventDeck::None;

    // ---- Tile[3]：公用事业格（Utility，index=3）----
    Tiles[3].TileIndex              = 3;
    Tiles[3].TileType               = ETileType::Utility;
    Tiles[3].ColorGroup             = EColorGroup::None; // Utility 不应有色组 ✓
    Tiles[3].PurchasePrice          = 150;
    Tiles[3].MortgageValue          = 75;                // 75 <= 150 ✓
    Tiles[3].DiceMultiplierTable    = {4, 10};           // 2 项，元素在[1,DICE_MULT_MAX] ✓
    Tiles[3].SalaryAmount           = 0;
    Tiles[3].TaxAmount              = 0;
    Tiles[3].EventDeck              = EEventDeck::None;

    // ---- Tile[4]：监狱格（Jail，index=4）----
    Tiles[4].TileIndex     = 4;
    Tiles[4].TileType      = ETileType::Jail;
    Tiles[4].ColorGroup    = EColorGroup::None;
    Tiles[4].PurchasePrice = 0;
    Tiles[4].MortgageValue = 0;
    Tiles[4].SalaryAmount  = 0;
    Tiles[4].TaxAmount     = 0;
    Tiles[4].EventDeck     = EEventDeck::None;

    // ---- Tile[5]：税务格（Tax，index=5）----
    Tiles[5].TileIndex     = 5;
    Tiles[5].TileType      = ETileType::Tax;
    Tiles[5].ColorGroup    = EColorGroup::None;
    Tiles[5].PurchasePrice = 0;
    Tiles[5].MortgageValue = 0;
    Tiles[5].SalaryAmount  = 0;
    Tiles[5].TaxAmount     = 100; // Tax 格税额
    Tiles[5].EventDeck     = EEventDeck::None;

    return Tiles;
}

// =============================================================================
// TC_Baseline_ValidBoard — 正路径：证基线棋盘通过全部拒绝校验（非 vacuous 前提）
//
// 若此测试失败，所有后续克隆测试的结论均不可信（基线本身有缺陷）。
// 必须在所有拒绝类测试之前通过。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_Baseline_ValidBoard,
    "Rento.Board.ValidationRejections.TC_Baseline_ValidBoard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_Baseline_ValidBoard::RunTest(const FString& Parameters)
{
    const TArray<FBoardTileData> Tiles = MakeValidBoard();
    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    // 基线棋盘必须通过全部拒绝校验，否则后续 AC 测试结论不可信
    TestTrue(
        TEXT("Baseline: MakeValidBoard() 应通过全部拒绝校验（IsValid()==true）"),
        Result.IsValid());

    // 精确断言 Code==None（非仅 IsValid 的非 vacuous 验证）
    TestEqual(
        TEXT("Baseline: ValidateTiles(基线盘).Code 应为 None"),
        Result.Code, EBoardValidationError::None);

    return true;
}

// =============================================================================
// TC_AC18_BoardTooSmall — AC-18：N<4 → BoardTooSmall
//
// GIVEN：3 格棋盘（N=3，索引 0..2，index0=Go，结构内容合规）
// WHEN：校验
// THEN：Code==BoardTooSmall，TileIndex==-1（板级错误无单格定位）
//
// 非 vacuous：N<4 是第一个板级检查，基线 N=6 不触发；N=3 精确触发。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC18_BoardTooSmall,
    "Rento.Board.ValidationRejections.TC_AC18_BoardTooSmall",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC18_BoardTooSmall::RunTest(const FString& Parameters)
{
    // 构造 N=3 棋盘（内容结构不重要，BoardTooSmall 是第一关，会提前返回）
    TArray<FBoardTileData> Tiles;
    Tiles.SetNum(3);
    for (int32 i = 0; i < 3; ++i)
    {
        Tiles[i].TileIndex = i;
        Tiles[i].TileType  = (i == 0) ? ETileType::Go : ETileType::Jail;
    }

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-18: N=3 应返回 BoardTooSmall"), Result.Code, EBoardValidationError::BoardTooSmall);
    TestEqual(TEXT("AC-18: BoardTooSmall 板级错误 TileIndex 应为 -1"), Result.TileIndex, -1);

    return true;
}

// =============================================================================
// TC_AC19a_MissingTileIndex — AC-19a：索引缺号 → MissingTileIndex{缺失的值}
//
// GIVEN：N=4 基线，把某格 TileIndex 改成越界值 4（使索引 2 缺失）
//         Tiles 数组 TileIndex 序列：[0, 1, 4, 3]（2 缺失，4 越界）
// WHEN：校验
// THEN：Code==MissingTileIndex，TileIndex==2（缺失的索引值本身）
//
// 非 vacuous：
//   - Seen[4] 越界（4>=N=4）→ 跳过不标记
//   - 遍历后 Seen[2]==false → MissingTileIndex{2}
//   - 基线 N=6 全覆盖，不触发；此 fixture 精确缺 2
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC19a_MissingTileIndex,
    "Rento.Board.ValidationRejections.TC_AC19a_MissingTileIndex",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC19a_MissingTileIndex::RunTest(const FString& Parameters)
{
    // 构造 N=4 基线（简单结构，索引序列 [0,1,2,3]）
    TArray<FBoardTileData> Tiles;
    Tiles.SetNum(4);
    Tiles[0].TileIndex = 0; Tiles[0].TileType = ETileType::Go;
    Tiles[1].TileIndex = 1; Tiles[1].TileType = ETileType::Jail;
    Tiles[2].TileIndex = 2; Tiles[2].TileType = ETileType::FreeParking;
    Tiles[3].TileIndex = 3; Tiles[3].TileType = ETileType::GoToJail;

    // 破坏：把 Tiles[2].TileIndex 改成越界值 4，使索引 2 缺失
    // 新序列：[0, 1, 4, 3]；N=4，合法范围 [0,3]，4 越界跳过 → Seen[2]==false
    Tiles[2].TileIndex = 4;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-19a: 索引 2 缺失 → MissingTileIndex"), Result.Code, EBoardValidationError::MissingTileIndex);
    TestEqual(TEXT("AC-19a: TileIndex 应为缺失的索引值 2"), Result.TileIndex, 2);

    return true;
}

// =============================================================================
// TC_AC19b_DuplicateTileIndex — AC-19b：索引重号 → DuplicateTileIndex{重号值}
//
// GIVEN：N=4 基线，把两格 TileIndex 都改成 1（重号）
//         序列：[0, 1, 1, 3]
// WHEN：校验
// THEN：Code==DuplicateTileIndex，TileIndex==1（重号的索引值本身）
//
// 非 vacuous：
//   - 第一次遇到 TileIndex=1：Seen[1]=false → Seen[1]=true
//   - 第二次遇到 TileIndex=1：Seen[1]=true → DuplicateTileIndex{1}，立即返回
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC19b_DuplicateTileIndex,
    "Rento.Board.ValidationRejections.TC_AC19b_DuplicateTileIndex",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC19b_DuplicateTileIndex::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles;
    Tiles.SetNum(4);
    Tiles[0].TileIndex = 0; Tiles[0].TileType = ETileType::Go;
    Tiles[1].TileIndex = 1; Tiles[1].TileType = ETileType::Jail;
    Tiles[2].TileIndex = 1; Tiles[2].TileType = ETileType::FreeParking; // 重号！
    Tiles[3].TileIndex = 3; Tiles[3].TileType = ETileType::GoToJail;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-19b: TileIndex 重号 1 → DuplicateTileIndex"), Result.Code, EBoardValidationError::DuplicateTileIndex);
    TestEqual(TEXT("AC-19b: TileIndex 应为重号值 1"), Result.TileIndex, 1);

    return true;
}

// =============================================================================
// TC_AC20_Index0NotGo — AC-20：TileIndex==0 格 TileType!=Go → Index0NotGo
//
// GIVEN：基线克隆，把 index=0 格改为 Property（同时另一格改为 Go，保持 Go 数==1）
//         以隔离 Index0NotGo，不撞 GoTileCountInvalid
// WHEN：校验
// THEN：Code==Index0NotGo
//
// 非 vacuous：
//   - 若不加另一个 Go，GoTileCountInvalid 会先触发（板级检查 4 在 Index0NotGo 之后，
//     但 Index0NotGo 是板级检查 3，先执行）。
//   - 实际顺序：板级3（Index0NotGo）先于板级4（GoTileCountInvalid）→ 此 fixture
//     index0=Property 且 Go 数==1（另一格=Go）→ 精确触发 Index0NotGo
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC20_Index0NotGo,
    "Rento.Board.ValidationRejections.TC_AC20_Index0NotGo",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC20_Index0NotGo::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：把 index=0 格（当前是 Go）改成 Jail
    // 同时：index=4 格（当前是 Jail）改成 Go，保持 Go 数==1，不触发 GoTileCountInvalid
    // 这样板级3（Index0NotGo）先触发，板级4（GoTileCountInvalid）不会到达
    Tiles[0].TileType     = ETileType::Jail;
    Tiles[0].SalaryAmount = 0; // Jail 不应有 SalaryAmount，但 Index0NotGo 先触发，不影响测试结论

    // 另一格变成 Go（保证 Go 数==1）
    Tiles[4].TileType = ETileType::Go;
    // Jail 原来没有 SalaryAmount=0，Go 原来也没有，这里 Tiles[4] 原本是 Jail 格，
    // SalaryAmount 已为 0；但 Tiles[4].TileType 现在是 Go，SalaryAmount=0 是允许的
    // 注：此 fixture 在 Index0NotGo 触发后立即返回，后续逐格检查不会执行

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-20: index0 非 Go → Index0NotGo"), Result.Code, EBoardValidationError::Index0NotGo);
    // GAP-2（code-review）：Index0NotGo 板级语义，TileIndex 填 -1（与其余板级错误一致，锁契约防回归）
    TestEqual(TEXT("AC-20: 板级语义 TileIndex 应为 -1"), Result.TileIndex, -1);

    return true;
}

// =============================================================================
// TC_AC21_PropertyMissingColorGroup — AC-21：Property 格 ColorGroup==None → 失败+TileIndex
//
// GIVEN：基线克隆，把 Property 格（index=1）ColorGroup 改为 None
// WHEN：校验
// THEN：Code==PropertyMissingColorGroup，TileIndex==1
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC21_PropertyMissingColorGroup,
    "Rento.Board.ValidationRejections.TC_AC21_PropertyMissingColorGroup",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC21_PropertyMissingColorGroup::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Property 格（数组序 Tiles[1]，TileIndex==1）失去色组
    Tiles[1].ColorGroup = EColorGroup::None;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-21: Property ColorGroup==None → PropertyMissingColorGroup"), Result.Code, EBoardValidationError::PropertyMissingColorGroup);
    TestEqual(TEXT("AC-21: 定位 TileIndex 应为 1"), Result.TileIndex, 1);

    return true;
}

// =============================================================================
// TC_AC32_ColorGroupOnNonProperty — AC-32：非 Property 格带 ColorGroup → 失败+TileIndex
//
// GIVEN：基线克隆，把 Railroad 格（index=2）ColorGroup 改为 Brown（非 None）
// WHEN：校验
// THEN：Code==ColorGroupOnNonProperty，TileIndex==2
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC32_ColorGroupOnNonProperty,
    "Rento.Board.ValidationRejections.TC_AC32_ColorGroupOnNonProperty",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC32_ColorGroupOnNonProperty::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Railroad 格（数组序 Tiles[2]，TileIndex==2）污染色组
    Tiles[2].ColorGroup = EColorGroup::Brown;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-32: Railroad 带 ColorGroup → ColorGroupOnNonProperty"), Result.Code, EBoardValidationError::ColorGroupOnNonProperty);
    TestEqual(TEXT("AC-32: 定位 TileIndex 应为 2"), Result.TileIndex, 2);

    return true;
}

// =============================================================================
// TC_AC22_Property_RentTableLength — AC-22：Property RentTable.Num()!=6 → 失败
//
// GIVEN：基线克隆，Property 格（index=1）RentTable 改为 4 项
// WHEN：校验
// THEN：Code==RentTableLengthInvalid，ExpectedLength==6，ActualLength==4，TileIndex==1
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC22_Property_RentTableLength,
    "Rento.Board.ValidationRejections.TC_AC22_Property_RentTableLength",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC22_Property_RentTableLength::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Property 格 RentTable 改为 4 项（应为 6）
    Tiles[1].RentTable = {2, 10, 30, 90};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-22: Property RentTable.Num()==4 → RentTableLengthInvalid"), Result.Code, EBoardValidationError::RentTableLengthInvalid);
    TestEqual(TEXT("AC-22: ExpectedLength 应为 6"), Result.ExpectedLength, 6);
    TestEqual(TEXT("AC-22: ActualLength 应为 4"), Result.ActualLength, 4);
    TestEqual(TEXT("AC-22: 定位 TileIndex 应为 1"), Result.TileIndex, 1);

    return true;
}

// =============================================================================
// TC_AC22b_Railroad_RentTableLength — AC-22b：Railroad RentTable.Num()!=4 → 失败
//
// GIVEN：基线克隆，Railroad 格（index=2）RentTable 改为 6 项
// WHEN：校验
// THEN：Code==RentTableLengthInvalid，ExpectedLength==4，ActualLength==6，TileIndex==2
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC22b_Railroad_RentTableLength,
    "Rento.Board.ValidationRejections.TC_AC22b_Railroad_RentTableLength",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC22b_Railroad_RentTableLength::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Railroad 格 RentTable 改为 6 项（应为 4）
    Tiles[2].RentTable = {25, 50, 100, 200, 300, 400};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-22b: Railroad RentTable.Num()==6 → RentTableLengthInvalid"), Result.Code, EBoardValidationError::RentTableLengthInvalid);
    TestEqual(TEXT("AC-22b: ExpectedLength 应为 4"), Result.ExpectedLength, 4);
    TestEqual(TEXT("AC-22b: ActualLength 应为 6"), Result.ActualLength, 6);
    TestEqual(TEXT("AC-22b: 定位 TileIndex 应为 2"), Result.TileIndex, 2);

    return true;
}

// =============================================================================
// TC_AC23_Property_PurchasePriceInvalid — AC-23（Property 分支）
//
// GIVEN：基线克隆，Property 格（index=1）PurchasePrice=0
// WHEN：校验
// THEN：Code==PurchasePriceInvalid，TileIndex==1
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23_Property_PurchasePriceInvalid,
    "Rento.Board.ValidationRejections.TC_AC23_Property_PurchasePriceInvalid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23_Property_PurchasePriceInvalid::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Property 格 PurchasePrice=0（不合法，须为正）
    // 同时把 MortgageValue 也设 0（否则 MortgageValue > PurchasePrice 先触发）
    Tiles[1].PurchasePrice = 0;
    Tiles[1].MortgageValue = 0;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23 Property: PurchasePrice=0 → PurchasePriceInvalid"), Result.Code, EBoardValidationError::PurchasePriceInvalid);
    TestEqual(TEXT("AC-23 Property: 定位 TileIndex 应为 1"), Result.TileIndex, 1);

    return true;
}

// =============================================================================
// TC_AC23_Railroad_PurchasePriceInvalid — AC-23（Railroad 分支）
//
// GIVEN：基线克隆，Railroad 格（index=2）PurchasePrice=0
// WHEN：校验
// THEN：Code==PurchasePriceInvalid，TileIndex==2
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23_Railroad_PurchasePriceInvalid,
    "Rento.Board.ValidationRejections.TC_AC23_Railroad_PurchasePriceInvalid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23_Railroad_PurchasePriceInvalid::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Railroad 格 PurchasePrice=0
    Tiles[2].PurchasePrice = 0;
    Tiles[2].MortgageValue = 0;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23 Railroad: PurchasePrice=0 → PurchasePriceInvalid"), Result.Code, EBoardValidationError::PurchasePriceInvalid);
    TestEqual(TEXT("AC-23 Railroad: 定位 TileIndex 应为 2"), Result.TileIndex, 2);

    return true;
}

// =============================================================================
// TC_AC23_Utility_PurchasePriceInvalid — AC-23（Utility 分支）
//
// GIVEN：基线克隆，Utility 格（index=3）PurchasePrice=0
// WHEN：校验
// THEN：Code==PurchasePriceInvalid，TileIndex==3
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23_Utility_PurchasePriceInvalid,
    "Rento.Board.ValidationRejections.TC_AC23_Utility_PurchasePriceInvalid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23_Utility_PurchasePriceInvalid::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Utility 格 PurchasePrice=0
    Tiles[3].PurchasePrice = 0;
    Tiles[3].MortgageValue = 0;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23 Utility: PurchasePrice=0 → PurchasePriceInvalid"), Result.Code, EBoardValidationError::PurchasePriceInvalid);
    TestEqual(TEXT("AC-23 Utility: 定位 TileIndex 应为 3"), Result.TileIndex, 3);

    return true;
}

// =============================================================================
// TC_AC23b_EventDeckMissing — AC-23b：Chance/CommunityChest EventDeck==None → 失败
//
// GIVEN：基线追加一格 Chance，EventDeck=None
// WHEN：校验
// THEN：Code==EventDeckMissing，TileIndex==该 Chance 格的 TileIndex
//
// 策略：扩展基线为 N=7，追加 Tiles[6]（TileIndex=6，Chance，EventDeck=None）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23b_EventDeckMissing,
    "Rento.Board.ValidationRejections.TC_AC23b_EventDeckMissing",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23b_EventDeckMissing::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard(); // N=6

    // 追加一格：Chance 格，EventDeck=None（缺失）
    FBoardTileData ChanceTile;
    ChanceTile.TileIndex  = 6; // N 将变为 7，TileIndex=6 在范围 [0,6] 内
    ChanceTile.TileType   = ETileType::Chance;
    ChanceTile.EventDeck  = EEventDeck::None; // 违规！Chance 须绑定牌堆
    ChanceTile.SalaryAmount = 0;
    ChanceTile.TaxAmount    = 0;
    Tiles.Add(ChanceTile);

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23b: Chance EventDeck==None → EventDeckMissing"), Result.Code, EBoardValidationError::EventDeckMissing);
    TestEqual(TEXT("AC-23b: 定位 TileIndex 应为 6"), Result.TileIndex, 6);

    return true;
}

// =============================================================================
// TC_AC23c_NoJailTile — AC-23c：含 GoToJail 但全盘无 Jail → NoJailTile
//
// GIVEN：基线克隆，把 Jail 格（index=4）改成 GoToJail（此时盘有 GoToJail，无 Jail）
// WHEN：校验
// THEN：Code==NoJailTile，TileIndex==-1（板级错误）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23c_NoJailTile,
    "Rento.Board.ValidationRejections.TC_AC23c_NoJailTile",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23c_NoJailTile::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：把唯一 Jail 格（Tiles[4]，TileIndex==4）改成 GoToJail
    // 现在盘有 GoToJail，但无 Jail → NoJailTile
    Tiles[4].TileType = ETileType::GoToJail;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23c: GoToJail 无 Jail → NoJailTile"), Result.Code, EBoardValidationError::NoJailTile);
    TestEqual(TEXT("AC-23c: 板级错误 TileIndex 应为 -1"), Result.TileIndex, -1);

    return true;
}

// =============================================================================
// TC_AC23d_GoTileCountInvalid — AC-23d：Go 格数量!=1 → GoTileCountInvalid
//
// GIVEN：基线克隆，在 Tax 格（index=5）也改为 Go（这样有 2 个 Go，index=0 和 index=5）
//         保持 index0=Go，隔离 GoTileCountInvalid 不撞 Index0NotGo
// WHEN：校验
// THEN：Code==GoTileCountInvalid，TileIndex==-1（板级错误）
//
// 非 vacuous：
//   - 板级3（Index0NotGo）：index0 仍是 Go → 通过
//   - 板级4（GoTileCountInvalid）：Go 数==2 → 触发
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23d_GoTileCountInvalid,
    "Rento.Board.ValidationRejections.TC_AC23d_GoTileCountInvalid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23d_GoTileCountInvalid::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：把 Tax 格（Tiles[5]，TileIndex==5）改成 Go
    // 此时有 2 个 Go（index=0 和 index=5）
    Tiles[5].TileType   = ETileType::Go;
    Tiles[5].TaxAmount  = 0;         // Go 格不应有 TaxAmount，但 GoTileCountInvalid 先触发
    Tiles[5].SalaryAmount = 200;     // Go 格的薪资（不影响测试结论，GoTileCountInvalid 先触发）

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23d: 2 个 Go → GoTileCountInvalid"), Result.Code, EBoardValidationError::GoTileCountInvalid);
    TestEqual(TEXT("AC-23d: 板级错误 TileIndex 应为 -1"), Result.TileIndex, -1);

    return true;
}

// =============================================================================
// TC_AC23e_MortgageExceedsPurchase — AC-23e：MortgageValue > PurchasePrice → 失败
//
// GIVEN：基线克隆，Property 格（index=1）MortgageValue=PurchasePrice+1（边界外）
// WHEN：校验
// THEN：Code==MortgageExceedsPurchase，TileIndex==1
//
// story-005 QA 说明：MortgageValue==PurchasePrice 由 AC-23e 拒绝（边界包含）；
//   MortgageValue > PurchasePrice*0.6 但 < PurchasePrice 走警告（story-006，不在本 story）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23e_MortgageExceedsPurchase,
    "Rento.Board.ValidationRejections.TC_AC23e_MortgageExceedsPurchase",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23e_MortgageExceedsPurchase::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Property 格 MortgageValue = PurchasePrice + 1 = 61（超出上界）
    Tiles[1].MortgageValue = Tiles[1].PurchasePrice + 1; // 60 + 1 = 61

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23e: MortgageValue > PurchasePrice → MortgageExceedsPurchase"), Result.Code, EBoardValidationError::MortgageExceedsPurchase);
    TestEqual(TEXT("AC-23e: 定位 TileIndex 应为 1"), Result.TileIndex, 1);

    return true;
}

// =============================================================================
// TC_AC23f_Utility_DiceMultiplierLength — AC-23f：Utility DiceMult.Num()!=2 → 失败
//
// GIVEN：基线克隆，Utility 格（index=3）DiceMultiplierTable 改为 3 项
// WHEN：校验
// THEN：Code==DiceMultiplierLengthInvalid，ExpectedLength==2，ActualLength==3，TileIndex==3
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23f_Utility_DiceMultiplierLength,
    "Rento.Board.ValidationRejections.TC_AC23f_Utility_DiceMultiplierLength",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23f_Utility_DiceMultiplierLength::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Utility 格 DiceMultiplierTable 改为 3 项（应为 2）
    Tiles[3].DiceMultiplierTable = {4, 10, 12};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23f: Utility DiceMult.Num()==3 → DiceMultiplierLengthInvalid"), Result.Code, EBoardValidationError::DiceMultiplierLengthInvalid);
    TestEqual(TEXT("AC-23f: ExpectedLength 应为 2"), Result.ExpectedLength, 2);
    TestEqual(TEXT("AC-23f: ActualLength 应为 3"), Result.ActualLength, 3);
    TestEqual(TEXT("AC-23f: 定位 TileIndex 应为 3"), Result.TileIndex, 3);

    return true;
}

// =============================================================================
// TC_AC23ga_DiceMultiplierTableNotAllowed_Property — AC-23g-a（Property 带 DiceMult）
//
// GIVEN：基线克隆，Property 格（index=1）增加非空 DiceMultiplierTable
// WHEN：校验
// THEN：Code==DiceMultiplierTableNotAllowed，TileIndex==1
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23ga_DiceMultiplierTableNotAllowed_Property,
    "Rento.Board.ValidationRejections.TC_AC23ga_DiceMultiplierTableNotAllowed_Property",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23ga_DiceMultiplierTableNotAllowed_Property::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Property 格误填 DiceMultiplierTable（反向校验，字段污染）
    Tiles[1].DiceMultiplierTable = {4, 10};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23g-a: Property 带 DiceMult → DiceMultiplierTableNotAllowed"), Result.Code, EBoardValidationError::DiceMultiplierTableNotAllowed);
    TestEqual(TEXT("AC-23g-a: 定位 TileIndex 应为 1"), Result.TileIndex, 1);

    return true;
}

// =============================================================================
// TC_AC23ga_DiceMultiplierTableNotAllowed_Railroad — AC-23g-a（Railroad 带 DiceMult）
//
// GIVEN：基线克隆，Railroad 格（index=2）增加非空 DiceMultiplierTable
// WHEN：校验
// THEN：Code==DiceMultiplierTableNotAllowed，TileIndex==2
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23ga_DiceMultiplierTableNotAllowed_Railroad,
    "Rento.Board.ValidationRejections.TC_AC23ga_DiceMultiplierTableNotAllowed_Railroad",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23ga_DiceMultiplierTableNotAllowed_Railroad::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Railroad 格误填 DiceMultiplierTable
    Tiles[2].DiceMultiplierTable = {4, 10};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23g-a: Railroad 带 DiceMult → DiceMultiplierTableNotAllowed"), Result.Code, EBoardValidationError::DiceMultiplierTableNotAllowed);
    TestEqual(TEXT("AC-23g-a: 定位 TileIndex 应为 2"), Result.TileIndex, 2);

    return true;
}

// =============================================================================
// TC_AC23gb_RentTableNotAllowed_Utility — AC-23g-b：Utility 带 RentTable → 失败
//
// GIVEN：基线克隆，Utility 格（index=3）增加非空 RentTable
// WHEN：校验
// THEN：Code==RentTableNotAllowed，TileIndex==3
//
// 策略：Utility 格 DiceMultiplierTable 保持有效（2项）且元素合规，
//         DiceMultiplierLengthInvalid 不先触发；DiceMultiplierOutOfRange 也不先触发；
//         RentTableNotAllowed 按子序 9 触发。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23gb_RentTableNotAllowed_Utility,
    "Rento.Board.ValidationRejections.TC_AC23gb_RentTableNotAllowed_Utility",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23gb_RentTableNotAllowed_Utility::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Utility 格误填 RentTable（反向校验，字段污染）
    // DiceMultiplierTable 保持基线有效（{4,10}，2项，合规）→ 子序 6/7 通过
    Tiles[3].RentTable = {10, 20, 30, 40, 50, 60};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23g-b: Utility 带 RentTable → RentTableNotAllowed"), Result.Code, EBoardValidationError::RentTableNotAllowed);
    TestEqual(TEXT("AC-23g-b: 定位 TileIndex 应为 3"), Result.TileIndex, 3);

    return true;
}

// =============================================================================
// TC_AC23gc_RentArrayOnNonPurchasable — AC-23g-c：非可购买格带数组 → 失败
//
// GIVEN：基线克隆，Tax 格（index=5）增加非空 RentTable
// WHEN：校验
// THEN：Code==RentArrayOnNonPurchasable，TileIndex==5
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23gc_RentArrayOnNonPurchasable,
    "Rento.Board.ValidationRejections.TC_AC23gc_RentArrayOnNonPurchasable",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23gc_RentArrayOnNonPurchasable::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Tax 格（数组序 Tiles[5]，TileIndex==5）污染 RentTable
    Tiles[5].RentTable = {10, 20, 30, 40, 50, 60};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23g-c: Tax 带 RentTable → RentArrayOnNonPurchasable"), Result.Code, EBoardValidationError::RentArrayOnNonPurchasable);
    TestEqual(TEXT("AC-23g-c: 定位 TileIndex 应为 5"), Result.TileIndex, 5);

    return true;
}

// =============================================================================
// TC_AC23h_SalaryOnNonGoTile — AC-23h：非 Go 格 SalaryAmount!=0 → 失败
//
// GIVEN：基线克隆，Property 格（index=1）SalaryAmount=200（非 Go 格不应有薪资）
// WHEN：校验
// THEN：Code==SalaryOnNonGoTile，TileIndex==1
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23h_SalaryOnNonGoTile,
    "Rento.Board.ValidationRejections.TC_AC23h_SalaryOnNonGoTile",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23h_SalaryOnNonGoTile::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Property 格（非 Go）误填 SalaryAmount=200
    Tiles[1].SalaryAmount = 200;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23h: 非 Go 格 SalaryAmount!=0 → SalaryOnNonGoTile"), Result.Code, EBoardValidationError::SalaryOnNonGoTile);
    TestEqual(TEXT("AC-23h: 定位 TileIndex 应为 1"), Result.TileIndex, 1);

    return true;
}

// =============================================================================
// TC_AC23i_TaxOnNonTaxTile — AC-23i：非 Tax 格 TaxAmount!=0 → 失败
//
// GIVEN：基线克隆，Property 格（index=1）TaxAmount=100（非 Tax 格不应有税额）
// WHEN：校验
// THEN：Code==TaxOnNonTaxTile，TileIndex==1
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23i_TaxOnNonTaxTile,
    "Rento.Board.ValidationRejections.TC_AC23i_TaxOnNonTaxTile",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23i_TaxOnNonTaxTile::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Property 格（非 Tax）误填 TaxAmount=100
    Tiles[1].TaxAmount = 100;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23i: 非 Tax 格 TaxAmount!=0 → TaxOnNonTaxTile"), Result.Code, EBoardValidationError::TaxOnNonTaxTile);
    TestEqual(TEXT("AC-23i: 定位 TileIndex 应为 1"), Result.TileIndex, 1);

    return true;
}

// =============================================================================
// TC_AC23j_DiceMultiplierOutOfRange_Below — AC-23j（下界子 case）
//
// GIVEN：基线克隆，Utility 格（index=3）DiceMultiplierTable=[0,10]（含 0，<1 非法）
// WHEN：校验
// THEN：Code==DiceMultiplierOutOfRange，TileIndex==3，OffendingValue==0
//
// 非 vacuous：0<1，精确 OffendingValue==0，非模糊断言
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23j_DiceMultiplierOutOfRange_Below,
    "Rento.Board.ValidationRejections.TC_AC23j_DiceMultiplierOutOfRange_Below",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23j_DiceMultiplierOutOfRange_Below::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Utility 格 DiceMultiplierTable=[0,10]，0 < 1 → 越界
    Tiles[3].DiceMultiplierTable = {0, 10};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23j 下界: [0,10] → DiceMultiplierOutOfRange"), Result.Code, EBoardValidationError::DiceMultiplierOutOfRange);
    TestEqual(TEXT("AC-23j 下界: 定位 TileIndex 应为 3"), Result.TileIndex, 3);
    TestEqual(TEXT("AC-23j 下界: OffendingValue 应为 0（违规元素）"), Result.OffendingValue, 0);

    return true;
}

// =============================================================================
// TC_AC23j_DiceMultiplierOutOfRange_Above — AC-23j（上界子 case）
//
// GIVEN：基线克隆，Utility 格（index=3）DiceMultiplierTable=[4,200000000]（>DICE_MULT_MAX）
// WHEN：校验
// THEN：Code==DiceMultiplierOutOfRange，TileIndex==3，OffendingValue==200000000
//
// 非 vacuous：200,000,000 > 1,000,000=DICE_MULT_MAX，精确越界元素值
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23j_DiceMultiplierOutOfRange_Above,
    "Rento.Board.ValidationRejections.TC_AC23j_DiceMultiplierOutOfRange_Above",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23j_DiceMultiplierOutOfRange_Above::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Utility 格 DiceMultiplierTable=[4,200000000]，200000000 > DICE_MULT_MAX=1000000
    Tiles[3].DiceMultiplierTable = {4, 200000000};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23j 上界: [4,200000000] → DiceMultiplierOutOfRange"), Result.Code, EBoardValidationError::DiceMultiplierOutOfRange);
    TestEqual(TEXT("AC-23j 上界: 定位 TileIndex 应为 3"), Result.TileIndex, 3);
    TestEqual(TEXT("AC-23j 上界: OffendingValue 应为 200000000"), Result.OffendingValue, 200000000);

    return true;
}

// =============================================================================
// TC_AC23j_DiceMultiplierBoundary_Valid — AC-23j（边界正路径）
//
// GIVEN：基线克隆，Utility 格（index=3）DiceMultiplierTable=[1, DICE_MULT_MAX]（合法边界）
// WHEN：校验
// THEN：IsValid()==true（边界值合法，不应触发 DiceMultiplierOutOfRange）
//
// 非 vacuous：
//   1 = DICE_MULT_MAX 的下界，DICE_MULT_MAX = 上界，均合法。
//   若实现用 <1（正确）而误写为 <=1，则值=1 时触发拒绝，本测试会 FAIL。
//   若实现用 >DICE_MULT_MAX（正确）而误写为 >=DICE_MULT_MAX，则此测试会 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23j_DiceMultiplierBoundary_Valid,
    "Rento.Board.ValidationRejections.TC_AC23j_DiceMultiplierBoundary_Valid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23j_DiceMultiplierBoundary_Valid::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 合法边界：[1, DICE_MULT_MAX]，两端均在合法区间内
    Tiles[3].DiceMultiplierTable = {1, UBoardValidator::DICE_MULT_MAX};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    // 边界值合法，不应拒绝
    TestTrue(
        TEXT("AC-23j 边界正路径: [1, DICE_MULT_MAX] 应通过校验（IsValid()==true）"),
        Result.IsValid());

    return true;
}

// =============================================================================
// ↓↓↓ code-review BD-005 补强测试（闭合真覆盖缺口，2026-06-07）↓↓↓
// =============================================================================

// =============================================================================
// TC_AC18_BoardAtMinimum_N4_Passes — AC-18 下界正路径：N=4 恰好合法（SUGGESTION-2）
//
// GIVEN：N=4 最小合规盘（index0=Go 且唯一 Go，含 Jail，无反向污染）
// WHEN：校验
// THEN：IsValid()==true
//
// 非 vacuous：基线 MakeValidBoard() 是 N=6，不验下界。若实现误写 `N <= 4` 则 N=4
//   会被错拒，本测试会 FAIL；当前实现 `N < 4`，N=4 通过。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC18_BoardAtMinimum_N4_Passes,
    "Rento.Board.ValidationRejections.TC_AC18_BoardAtMinimum_N4_Passes",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC18_BoardAtMinimum_N4_Passes::RunTest(const FString& Parameters)
{
    // 构造 N=4 最小合规盘：Go(0) / Jail(1) / FreeParking(2) / GoToJail(3)
    // GoToJail 有目标（盘内有 Jail），无反向污染，Go 唯一在 index0
    TArray<FBoardTileData> Tiles;
    Tiles.SetNum(4);
    Tiles[0].TileIndex = 0; Tiles[0].TileType = ETileType::Go;          Tiles[0].SalaryAmount = 200;
    Tiles[1].TileIndex = 1; Tiles[1].TileType = ETileType::Jail;
    Tiles[2].TileIndex = 2; Tiles[2].TileType = ETileType::FreeParking;
    Tiles[3].TileIndex = 3; Tiles[3].TileType = ETileType::GoToJail;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestTrue(
        TEXT("AC-18 下界: N=4 最小合规盘应通过校验（IsValid()==true）"),
        Result.IsValid());

    return true;
}

// =============================================================================
// TC_AC23d_ZeroGo_GoTileCountInvalid — AC-23d「0 个 Go」子 case（GAP-1）
//
// GIVEN：基线克隆，把唯一 Go（index0）改成 Jail 之外的非 Go 类型，使全盘 0 个 Go
// WHEN：校验
// THEN：Code==GoTileCountInvalid，TileIndex==-1（板级错误）
//
// 关键（顺序契约验证）：实现已调换板级检查顺序，GoTileCountInvalid（板级 3）先于
//   Index0NotGo（板级 4）。若实现顺序回退（Index0NotGo 先），「0 个 Go」会被
//   Index0NotGo 抢先，本测试断言 GoTileCountInvalid 会 FAIL —— 即本测试守住该顺序契约。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23d_ZeroGo_GoTileCountInvalid,
    "Rento.Board.ValidationRejections.TC_AC23d_ZeroGo_GoTileCountInvalid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23d_ZeroGo_GoTileCountInvalid::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：把唯一 Go（Tiles[0]）改成 FreeParking → 全盘 0 个 Go
    // 同时清掉 Go 专属薪额（否则逐格子序会因「非 Go 带 SalaryAmount」拒绝，
    // 但板级 3 GoTileCountInvalid 先返回，逐格不执行；清零仅为 fixture 卫生）
    Tiles[0].TileType     = ETileType::FreeParking;
    Tiles[0].SalaryAmount = 0;

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23d 0个Go: → GoTileCountInvalid（验顺序契约：先于 Index0NotGo）"),
        Result.Code, EBoardValidationError::GoTileCountInvalid);
    TestEqual(TEXT("AC-23d 0个Go: 板级错误 TileIndex 应为 -1"), Result.TileIndex, -1);

    return true;
}

// =============================================================================
// TC_AC23b_EventDeckMissing_CommunityChest — AC-23b CommunityChest 分支（GAP-3）
//
// GIVEN：基线追加一格 CommunityChest，EventDeck=None
// WHEN：校验
// THEN：Code==EventDeckMissing，TileIndex==6
//
// 非 vacuous：实现条件 `Type==Chance || Type==CommunityChest`，原测试只覆盖 Chance 分支；
//   本测试覆盖 CommunityChest 分支（|| 右侧），证两枚举值均触发。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23b_EventDeckMissing_CommunityChest,
    "Rento.Board.ValidationRejections.TC_AC23b_EventDeckMissing_CommunityChest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23b_EventDeckMissing_CommunityChest::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard(); // N=6

    // 追加一格：CommunityChest 格，EventDeck=None（缺失）
    FBoardTileData CCTile;
    CCTile.TileIndex     = 6;
    CCTile.TileType      = ETileType::CommunityChest;
    CCTile.ColorGroup    = EColorGroup::None;   // 显式：非地产格无色组（防默认值漂移，GAP-3 卫生）
    CCTile.EventDeck     = EEventDeck::None;     // 违规！CommunityChest 须绑定牌堆
    CCTile.SalaryAmount  = 0;
    CCTile.TaxAmount     = 0;
    Tiles.Add(CCTile);

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23b CC: CommunityChest EventDeck==None → EventDeckMissing"),
        Result.Code, EBoardValidationError::EventDeckMissing);
    TestEqual(TEXT("AC-23b CC: 定位 TileIndex 应为 6"), Result.TileIndex, 6);

    return true;
}

// =============================================================================
// TC_AC23gc_RentArrayOnNonPurchasable_DiceMult — AC-23g-c DiceMult 污染分支（GAP-4）
//
// GIVEN：基线克隆，Tax 格（index=5）带非空 DiceMultiplierTable（|| 右侧分支）
// WHEN：校验
// THEN：Code==RentArrayOnNonPurchasable，TileIndex==5
//
// 非 vacuous：实现条件 `RentTable.Num()>0 || DiceMultiplierTable.Num()>0`，原测试只覆盖
//   左侧（RentTable）；本测试覆盖右侧（DiceMultiplierTable），证 || 两分支均触发。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23gc_RentArrayOnNonPurchasable_DiceMult,
    "Rento.Board.ValidationRejections.TC_AC23gc_RentArrayOnNonPurchasable_DiceMult",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23gc_RentArrayOnNonPurchasable_DiceMult::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 破坏：Tax 格（数组序 Tiles[5]，TileIndex==5）污染 DiceMultiplierTable（非 RentTable）
    Tiles[5].DiceMultiplierTable = {4, 10};

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestEqual(TEXT("AC-23g-c DiceMult: Tax 带 DiceMultiplierTable → RentArrayOnNonPurchasable"),
        Result.Code, EBoardValidationError::RentArrayOnNonPurchasable);
    TestEqual(TEXT("AC-23g-c DiceMult: 定位 TileIndex 应为 5"), Result.TileIndex, 5);

    return true;
}

// =============================================================================
// TC_AC23e_MortgageEqualsPurchase_Valid — AC-23e 边界正路径（doc-sync）
//
// GIVEN：基线克隆，Property 格（index=1）MortgageValue == PurchasePrice
// WHEN：校验
// THEN：IsValid()==true（== 是合法边界，仅 > 拒绝）
//
// 依据：GDD §Edge Cases 第 272 行「MortgageValue ≤ PurchasePrice 是硬数据约束」——
//   == 合法。story 第 79 行原 edge note「== 拒绝」与 GDD 相反，已 doc-sync 更正。
// 非 vacuous：若实现误写 `>=` 而非 `>`，则 == 时触发 MortgageExceedsPurchase，本测试 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_AC23e_MortgageEqualsPurchase_Valid,
    "Rento.Board.ValidationRejections.TC_AC23e_MortgageEqualsPurchase_Valid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_AC23e_MortgageEqualsPurchase_Valid::RunTest(const FString& Parameters)
{
    TArray<FBoardTileData> Tiles = MakeValidBoard();

    // 合法边界：MortgageValue == PurchasePrice（== 不拒绝，仅 > 拒绝）
    Tiles[1].MortgageValue = Tiles[1].PurchasePrice; // 60 == 60

    const FBoardValidationError Result = UBoardValidator::ValidateTiles(Tiles);

    TestTrue(
        TEXT("AC-23e 边界正路径: MortgageValue==PurchasePrice 应通过校验（IsValid()==true）"),
        Result.IsValid());

    return true;
}

// =============================================================================
// TC_ValidateBoard_NullBoard_ReturnsBoardTooSmall — BP 入口 null 兜底（SUGGESTION-1）
//
// GIVEN：ValidateBoard(nullptr)
// WHEN：校验
// THEN：Code==BoardTooSmall（null → 空数组 → N=0 → BoardTooSmall，合理兜底）
//
// 非 vacuous：守住 ValidateBoard 的 null-guard，防重构破坏（如改成解引用 null 崩溃）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardValidation_TC_ValidateBoard_NullBoard_ReturnsBoardTooSmall,
    "Rento.Board.ValidationRejections.TC_ValidateBoard_NullBoard_ReturnsBoardTooSmall",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardValidation_TC_ValidateBoard_NullBoard_ReturnsBoardTooSmall::RunTest(const FString& Parameters)
{
    const FBoardValidationError Result = UBoardValidator::ValidateBoard(nullptr);

    TestEqual(TEXT("null Board → BoardTooSmall（空数组兜底）"),
        Result.Code, EBoardValidationError::BoardTooSmall);

    return true;
}
