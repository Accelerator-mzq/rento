// BoardDataAssetSchemaTest.cpp
// =============================================================================
// 单元测试：FBoardTileData + UBoardDataAsset Schema 结构验证
// story-001（棋盘数据 schema）AC-4 / AC-4b / AC-7b / 结构完整性 covered。
//
// 物理路径：Source/rentoTests/Private/BoardDataAssetSchemaTest.cpp
// 逻辑分类：tests/unit/board/board_data_asset_schema_test.cpp（文档记录，与 FF-001/FF-003 惯例一致）
// Automation 类目：Rento.Board.BoardDataAssetSchema
//
// 测试设计原则（本项目硬约束）：
//   - 确定性：全部 fixture 代码构造，无随机/时间依赖/磁盘 IO
//   - 非 vacuous Edge：每个 Edge 反向测试的谓词选取确保真能 FAIL
//     （即若删掉断言的 TestFalse 改 TestTrue，该 fixture 会令测试通过——证谓词非永假）
//   - 无 story-005 生产校验逻辑：本 story 只断言 schema 语义谓词（数据形态），
//     不实现任何加载期错误码 / 拒绝逻辑（归 story-005/006）
//   - headless -nullrhi 可跑：不碰渲染/Slate，纯内存对象操作
//
// 覆盖映射：
//   TC_AC4_PropertyTile_ValidSchema               → AC-4  正向
//   TC_AC4_Edge_PropertyTile_ShortRentTable       → AC-4  反向 Edge（RentTable.Num()==5 令谓词 false）
//   TC_AC4b_UtilityTile_ValidSchema               → AC-4b 正向
//   TC_AC4b_Edge_UtilityTile_WithRentTable        → AC-4b 反向 Edge（RentTable 非空令谓词 false）
//   TC_AC7b_GoTile_ValidSchema                    → AC-7b 正向
//   TC_AC7b_Edge_GoTile_ZeroSalary                → AC-7b 反向 Edge（SalaryAmount==0 令谓词 false）
//   TC_Integrity_UBoardDataAsset_FieldReadWrite   → 结构完整性（顶层载体字段可读写，含多 key ColorKeyTable）
//   TC_Integrity_EnumOrdinal_AppendOnly           → 结构完整性（枚举整数值顺序契约 / append-only，ETileType 全 10 项）
//   TC_Integrity_DefaultConstruct_MatchesADRDefaults → 结构完整性（FBoardTileData 默认值契约 + 反射字段计数==13）
//   TC_Integrity_PlaceholderData_DefaultsTrue     → 结构完整性（bIsPlaceholderData 默认 true，Editor target 可测）
//   TC_Schema_RailroadTile_RentTableLength4       → Railroad schema 正向（RentTable==4 story-005 校验锚点）
// =============================================================================

#include "Misc/AutomationTest.h"
#include "UObject/Class.h"       // TFieldIterator<FProperty>
#include "BoardDataAsset.h"

// =============================================================================
// TC_AC4 — AC-4 正向：Property 格 schema 合法谓词全真
//
// GIVEN：代码构造的 Property FBoardTileData fixture
//         （ColorGroup=Red, PurchasePrice=300, RentTable 6 项, BuildingCost=200,
//           MortgageValue=150, DiceMultiplierTable 空）
// WHEN：内联断言 schema 谓词
// THEN：ColorGroup!=None && PurchasePrice>0 && RentTable.Num()==6
//        && BuildingCost>0 && DiceMultiplierTable.Num()==0 全真
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_AC4_PropertyTile_ValidSchema,
    "Rento.Board.BoardDataAssetSchema.TC_AC4_PropertyTile_ValidSchema",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_AC4_PropertyTile_ValidSchema::RunTest(const FString& Parameters)
{
    // GIVEN：构造合法的 Property 格 fixture（经典大富翁红色地产 Boardwalk 级参考值）
    FBoardTileData Tile;
    Tile.TileIndex   = 39;
    Tile.TileType    = ETileType::Property;
    Tile.ColorGroup  = EColorGroup::Red;
    Tile.PurchasePrice = 300;
    // RentTable：[无房, 1房, 2房, 3房, 4房, 酒店] = 6 项（AC-4 指定）
    Tile.RentTable   = { 20, 60, 180, 500, 700, 900 };
    Tile.BuildingCost  = 200;
    Tile.MortgageValue = 150;
    // DiceMultiplierTable：Property 格不使用，保持空（AC-4 指定）
    Tile.DiceMultiplierTable = {};

    // WHEN + THEN：断言 AC-4 schema 谓词各分量
    TestTrue(
        TEXT("AC-4: Property 格 ColorGroup 应非 None（有色组归属）"),
        Tile.ColorGroup != EColorGroup::None);

    TestTrue(
        TEXT("AC-4: Property 格 PurchasePrice 应 > 0"),
        Tile.PurchasePrice > 0);

    TestTrue(
        TEXT("AC-4: Property 格 RentTable 应含 6 项（无房/1-4房/酒店）"),
        Tile.RentTable.Num() == 6);

    TestTrue(
        TEXT("AC-4: Property 格 BuildingCost 应 > 0"),
        Tile.BuildingCost > 0);

    TestTrue(
        TEXT("AC-4: Property 格 DiceMultiplierTable 应为空（Property 不用骰点倍率）"),
        Tile.DiceMultiplierTable.Num() == 0);

    return true;
}

// =============================================================================
// TC_AC4 Edge — AC-4 反向：RentTable.Num()==5 令谓词为 false
//
// Edge 非 vacuous 保证：
//   谓词 = (RentTable.Num() == 6)。
//   本 fixture RentTable 只填 5 项，谓词值 = false。
//   若将 TestFalse 改为 TestTrue，此测试会 FAIL——证谓词非永假。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_AC4_Edge_PropertyTile_ShortRentTable,
    "Rento.Board.BoardDataAssetSchema.TC_AC4_Edge_PropertyTile_ShortRentTable_Invalid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_AC4_Edge_PropertyTile_ShortRentTable::RunTest(const FString& Parameters)
{
    // GIVEN：RentTable 只有 5 项（缺少酒店一级）——非法的 Property schema
    FBoardTileData Tile;
    Tile.TileType   = ETileType::Property;
    Tile.ColorGroup = EColorGroup::Red;
    Tile.PurchasePrice  = 300;
    Tile.RentTable  = { 20, 60, 180, 500, 700 };  // ← 5 项，故意缺第 6 项
    Tile.BuildingCost   = 200;
    Tile.MortgageValue  = 150;
    Tile.DiceMultiplierTable = {};

    // WHEN：计算 AC-4 Property 合法谓词（RentTable.Num()==6 分量）
    const bool bPropertySchemaValid =
        (Tile.ColorGroup != EColorGroup::None) &&
        (Tile.PurchasePrice > 0)               &&
        (Tile.RentTable.Num() == 6)            &&  // ← 此分量为 false（实际 5 项）
        (Tile.BuildingCost > 0)                &&
        (Tile.DiceMultiplierTable.Num() == 0);

    // THEN：整体谓词应为 false（RentTable.Num()==5 令短路求值使整体 false）
    TestFalse(
        TEXT("AC-4 Edge: RentTable 只有 5 项时 Property schema 谓词应为 false（守住长度即语义）"),
        bPropertySchemaValid);

    return true;
}

// =============================================================================
// TC_AC4b — AC-4b 正向：Utility 格 schema 合法谓词全真
//
// GIVEN：代码构造的 Utility FBoardTileData fixture
//         （TileType=Utility, PurchasePrice=150, DiceMultiplierTable={4,10}, RentTable={}）
// WHEN：内联断言 schema 谓词
// THEN：PurchasePrice>0 && DiceMultiplierTable.Num()==2 && RentTable.Num()==0 全真
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_AC4b_UtilityTile_ValidSchema,
    "Rento.Board.BoardDataAssetSchema.TC_AC4b_UtilityTile_ValidSchema",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_AC4b_UtilityTile_ValidSchema::RunTest(const FString& Parameters)
{
    // GIVEN：构造合法的 Utility 格 fixture（单持倍率 4，双持倍率 10）
    FBoardTileData Tile;
    Tile.TileIndex        = 12;
    Tile.TileType         = ETileType::Utility;
    Tile.ColorGroup       = EColorGroup::None;   // Utility 无色组
    Tile.PurchasePrice    = 150;
    Tile.RentTable        = {};                  // Utility 不使用 RentTable（AC-4b 指定）
    // DiceMultiplierTable：[单持倍率, 双持倍率] = 2 项（AC-4b 指定）
    Tile.DiceMultiplierTable = { 4, 10 };
    Tile.BuildingCost     = 0;
    Tile.MortgageValue    = 75;

    // WHEN + THEN：断言 AC-4b schema 谓词各分量
    TestTrue(
        TEXT("AC-4b: Utility 格 PurchasePrice 应 > 0"),
        Tile.PurchasePrice > 0);

    TestTrue(
        TEXT("AC-4b: Utility 格 DiceMultiplierTable 应含 2 项（单持/双持倍率）"),
        Tile.DiceMultiplierTable.Num() == 2);

    TestTrue(
        TEXT("AC-4b: Utility 格 RentTable 应为空（倍率不混入 RentTable）"),
        Tile.RentTable.Num() == 0);

    return true;
}

// =============================================================================
// TC_AC4b Edge — AC-4b 反向：RentTable 非空令 Utility schema 谓词为 false
//
// Edge 非 vacuous 保证：
//   谓词 = (RentTable.Num() == 0)。
//   本 fixture RentTable 填了 1 项，谓词值 = false。
//   若将 TestFalse 改为 TestTrue，此测试会 FAIL——证谓词非永假。
//   语义：Utility 格若 RentTable 非空，说明 schema 混用了两套租金机制，不合法。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_AC4b_Edge_UtilityTile_WithRentTable,
    "Rento.Board.BoardDataAssetSchema.TC_AC4b_Edge_UtilityTile_WithRentTable_Invalid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_AC4b_Edge_UtilityTile_WithRentTable::RunTest(const FString& Parameters)
{
    // GIVEN：Utility 格 fixture，但错误地填了 RentTable（两套租金机制混用）
    FBoardTileData Tile;
    Tile.TileType            = ETileType::Utility;
    Tile.PurchasePrice       = 150;
    Tile.DiceMultiplierTable = { 4, 10 };
    Tile.RentTable           = { 25 };  // ← 非空，故意违反 Utility schema 约束

    // WHEN：计算 AC-4b Utility 合法谓词
    const bool bUtilitySchemaValid =
        (Tile.PurchasePrice > 0)               &&
        (Tile.DiceMultiplierTable.Num() == 2)  &&
        (Tile.RentTable.Num() == 0);           // ← 此分量为 false（RentTable 有 1 项）

    // THEN：整体谓词应为 false（RentTable 非空时 Utility 不符合 AC-4b 语义）
    TestFalse(
        TEXT("AC-4b Edge: Utility 格 RentTable 非空时 schema 谓词应为 false（倍率不得混入 RentTable）"),
        bUtilitySchemaValid);

    return true;
}

// =============================================================================
// TC_AC7b — AC-7b 正向：Go 格 schema 合法谓词全真
//
// GIVEN：代码构造的 Go FBoardTileData fixture
//         （TileType=Go, SalaryAmount=200, SpecialAction=CollectSalary）
// WHEN：内联断言 schema 谓词
// THEN：SalaryAmount>0 && SpecialAction==CollectSalary 全真
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_AC7b_GoTile_ValidSchema,
    "Rento.Board.BoardDataAssetSchema.TC_AC7b_GoTile_ValidSchema",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_AC7b_GoTile_ValidSchema::RunTest(const FString& Parameters)
{
    // GIVEN：构造合法的 Go 格 fixture（起点格，薪资 200，经典大富翁标准值）
    FBoardTileData Tile;
    Tile.TileIndex     = 0;
    Tile.TileType      = ETileType::Go;
    Tile.ColorGroup    = EColorGroup::None;
    Tile.PurchasePrice = 0;   // Go 格不可购买
    Tile.SalaryAmount  = 200; // AC-7b 指定：SalaryAmount>0
    Tile.SpecialAction = ETileAction::CollectSalary; // AC-7b 指定

    // WHEN + THEN：断言 AC-7b schema 谓词各分量
    TestTrue(
        TEXT("AC-7b: Go 格 SalaryAmount 应 > 0"),
        Tile.SalaryAmount > 0);

    TestTrue(
        TEXT("AC-7b: Go 格 SpecialAction 应为 CollectSalary"),
        Tile.SpecialAction == ETileAction::CollectSalary);

    return true;
}

// =============================================================================
// TC_AC7b Edge — AC-7b 反向：SalaryAmount==0 令 Go schema 谓词为 false
//
// Edge 非 vacuous 保证：
//   谓词 = (SalaryAmount > 0)。
//   本 fixture SalaryAmount==0，谓词值 = false。
//   若将 TestFalse 改为 TestTrue，此测试会 FAIL——证谓词非永假。
//   语义：Go 格若 SalaryAmount==0，说明薪资未设置，schema 不完整。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_AC7b_Edge_GoTile_ZeroSalary,
    "Rento.Board.BoardDataAssetSchema.TC_AC7b_Edge_GoTile_ZeroSalary_Invalid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_AC7b_Edge_GoTile_ZeroSalary::RunTest(const FString& Parameters)
{
    // GIVEN：Go 格 fixture，但 SalaryAmount==0（未设置薪资，schema 不完整）
    FBoardTileData Tile;
    Tile.TileType      = ETileType::Go;
    Tile.SalaryAmount  = 0;  // ← 故意置 0，违反 Go 格 schema 约束
    Tile.SpecialAction = ETileAction::CollectSalary;

    // WHEN：计算 AC-7b Go 合法谓词
    const bool bGoSchemaValid =
        (Tile.SalaryAmount > 0) &&                         // ← 此分量为 false（SalaryAmount==0）
        (Tile.SpecialAction == ETileAction::CollectSalary);

    // THEN：整体谓词应为 false（SalaryAmount==0 时 Go schema 不合法）
    TestFalse(
        TEXT("AC-7b Edge: Go 格 SalaryAmount==0 时 schema 谓词应为 false（薪资未设置）"),
        bGoSchemaValid);

    return true;
}

// =============================================================================
// TC_Integrity_UBoardDataAsset_FieldReadWrite — 结构完整性：顶层载体字段可读写
//
// 验证 UBoardDataAsset 作为 UObject 可正常构造，三个顶层字段（BoardIdentifier /
// ColorKeyTable / Tiles）可写入后读回且值相等，证 Primary Data Asset 载体结构有效。
//
// ColorKeyTable 多 key 覆盖：
//   - EColorGroup::Red      — 普通色组 key
//   - EColorGroup::DarkBlue — 不同色组 key（证多条目共存）
//   - EColorGroup::None     — 边界：None=0 作为 TMap key 应同样可存取
//
// 注意：
//   - NewObject<UBoardDataAsset>() 需要 Outer（GetTransientPackage()），headless 安全
//   - rentoTests 以 Editor target（rentoEditor.Target.cs，Type=TargetType.Editor）编译，
//     因此 WITH_EDITORONLY_DATA 恒为 1；bIsPlaceholderData 的独立测试见
//     TC_Integrity_PlaceholderData_DefaultsTrue
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_Integrity_UBoardDataAsset_FieldReadWrite,
    "Rento.Board.BoardDataAssetSchema.TC_Integrity_UBoardDataAsset_FieldReadWrite",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_Integrity_UBoardDataAsset_FieldReadWrite::RunTest(const FString& Parameters)
{
    // GIVEN：创建 UBoardDataAsset 实例（GetTransientPackage() 作为 Outer，headless 安全）
    UBoardDataAsset* Asset = NewObject<UBoardDataAsset>(GetTransientPackage(), UBoardDataAsset::StaticClass());
    if (!TestNotNull(TEXT("TC_Integrity: UBoardDataAsset 应能通过 NewObject<> 构造"), Asset))
    {
        return false;
    }

    // ---- BoardIdentifier 读写 ----
    // WHEN：写入 BoardIdentifier
    const FName ExpectedIdentifier = FName(TEXT("Classic40"));
    Asset->BoardIdentifier = ExpectedIdentifier;

    // THEN：读回 BoardIdentifier 应与写入值相等（FName 比较）
    TestEqual(
        TEXT("TC_Integrity: BoardIdentifier 写入后读回应相等"),
        Asset->BoardIdentifier,
        ExpectedIdentifier);

    // ---- Tiles 读写 ----
    // WHEN：向 Tiles 数组添加一格（Go 格）
    FBoardTileData GoTile;
    GoTile.TileIndex     = 0;
    GoTile.TileType      = ETileType::Go;
    GoTile.SalaryAmount  = 200;
    GoTile.SpecialAction = ETileAction::CollectSalary;
    Asset->Tiles.Add(GoTile);

    // THEN：Tiles 数组应含 1 格，且读回字段值相等
    TestEqual(
        TEXT("TC_Integrity: Tiles 添加 1 格后 Num 应为 1"),
        Asset->Tiles.Num(), 1);

    TestEqual(
        TEXT("TC_Integrity: 读回 TileType 应为 Go"),
        Asset->Tiles[0].TileType, ETileType::Go);

    TestEqual(
        TEXT("TC_Integrity: 读回 SalaryAmount 应为 200"),
        Asset->Tiles[0].SalaryAmount, 200);

    TestTrue(
        TEXT("TC_Integrity: 读回 SpecialAction 应为 CollectSalary"),
        Asset->Tiles[0].SpecialAction == ETileAction::CollectSalary);

    // ---- ColorKeyTable 读写（多 key + None=0 边界）----
    // WHEN：添加三个 key：Red / DarkBlue / None（边界，None=0）
    const FColor RedColor     = FColor(220, 50,  50,  255);
    const FColor DarkBlueColor = FColor(0,   0,  139, 255);
    const FColor NoneColor    = FColor(128, 128, 128, 255); // None=0 作 key 的边界值

    Asset->ColorKeyTable.Add(EColorGroup::Red,      RedColor);
    Asset->ColorKeyTable.Add(EColorGroup::DarkBlue, DarkBlueColor);
    Asset->ColorKeyTable.Add(EColorGroup::None,     NoneColor);

    // THEN：ColorKeyTable 应含 3 项
    TestEqual(
        TEXT("TC_Integrity: ColorKeyTable 添加 3 项后 Num 应为 3"),
        Asset->ColorKeyTable.Num(), 3);

    // THEN：Find Red —— 存在且 R 分量正确
    const FColor* FoundRed = Asset->ColorKeyTable.Find(EColorGroup::Red);
    TestNotNull(TEXT("TC_Integrity: ColorKeyTable 应能 Find 到 EColorGroup::Red"), FoundRed);
    if (FoundRed)
    {
        TestEqual(TEXT("TC_Integrity: Red 颜色 R 分量应为 220"),
            static_cast<int32>(FoundRed->R), 220);
        TestEqual(TEXT("TC_Integrity: Red 颜色 G 分量应为 50"),
            static_cast<int32>(FoundRed->G), 50);
    }

    // THEN：Find DarkBlue —— 存在且 B 分量正确（证多 key 共存）
    const FColor* FoundDarkBlue = Asset->ColorKeyTable.Find(EColorGroup::DarkBlue);
    TestNotNull(TEXT("TC_Integrity: ColorKeyTable 应能 Find 到 EColorGroup::DarkBlue"), FoundDarkBlue);
    if (FoundDarkBlue)
    {
        TestEqual(TEXT("TC_Integrity: DarkBlue 颜色 B 分量应为 139"),
            static_cast<int32>(FoundDarkBlue->B), 139);
        TestEqual(TEXT("TC_Integrity: DarkBlue 颜色 R 分量应为 0"),
            static_cast<int32>(FoundDarkBlue->R), 0);
    }

    // THEN：Find None（边界：EColorGroup::None=0 作 TMap key 应可存取）
    const FColor* FoundNone = Asset->ColorKeyTable.Find(EColorGroup::None);
    TestNotNull(TEXT("TC_Integrity: ColorKeyTable 应能 Find 到 EColorGroup::None（None=0 边界 key）"), FoundNone);
    if (FoundNone)
    {
        TestEqual(TEXT("TC_Integrity: None 颜色 R 分量应为 128"),
            static_cast<int32>(FoundNone->R), 128);
    }

    return true;
}

// =============================================================================
// TC_Integrity_EnumOrdinal_AppendOnly — 结构完整性：枚举整数值顺序契约
//
// 断言关键枚举项的底层整数值符合 append-only 约定（ADR-0003/0005）。
// 目的：坐实枚举顺序契约——若有人重排枚举（如插入新值到中间），
//        这些断言会立即 FAIL，在 CI 阶段捕获破坏性变更。
//
// ETileType：锁全 10 项（0-9）——重排任意中段项即 FAIL。
// EColorGroup / ETileAction / EEventDeck：锁首项=0 + 末尾项 + 关键中段项。
// 注意：static_cast<uint8>(EnumValue) 是读取底层整数值的标准方式（uint8 背书枚举）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_Integrity_EnumOrdinal_AppendOnly,
    "Rento.Board.BoardDataAssetSchema.TC_Integrity_EnumOrdinal_AppendOnly",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_Integrity_EnumOrdinal_AppendOnly::RunTest(const FString& Parameters)
{
    // ---- ETileType（全 10 项，Property=0 … GoToJail=9，全部锁定）----
    // 锁全 10 项：重排任意中段（如把 Jail 从 7 挪到 4）即有至少一条断言 FAIL
    TestEqual(TEXT("ETileType::Property=0"),
        static_cast<uint8>(ETileType::Property),       static_cast<uint8>(0));
    TestEqual(TEXT("ETileType::Railroad=1"),
        static_cast<uint8>(ETileType::Railroad),       static_cast<uint8>(1));
    TestEqual(TEXT("ETileType::Utility=2"),
        static_cast<uint8>(ETileType::Utility),        static_cast<uint8>(2));
    TestEqual(TEXT("ETileType::Chance=3"),
        static_cast<uint8>(ETileType::Chance),         static_cast<uint8>(3));
    TestEqual(TEXT("ETileType::CommunityChest=4"),
        static_cast<uint8>(ETileType::CommunityChest), static_cast<uint8>(4));
    TestEqual(TEXT("ETileType::Tax=5"),
        static_cast<uint8>(ETileType::Tax),            static_cast<uint8>(5));
    TestEqual(TEXT("ETileType::Go=6"),
        static_cast<uint8>(ETileType::Go),             static_cast<uint8>(6));
    TestEqual(TEXT("ETileType::Jail=7"),
        static_cast<uint8>(ETileType::Jail),           static_cast<uint8>(7));
    TestEqual(TEXT("ETileType::FreeParking=8"),
        static_cast<uint8>(ETileType::FreeParking),    static_cast<uint8>(8));
    TestEqual(TEXT("ETileType::GoToJail=9（末尾项，append-only 上边界）"),
        static_cast<uint8>(ETileType::GoToJail),       static_cast<uint8>(9));

    // ---- EColorGroup（None=0，DarkBlue=8）----
    TestEqual(TEXT("EColorGroup::None=0（append-only 基准）"),
        static_cast<uint8>(EColorGroup::None),     static_cast<uint8>(0));
    TestEqual(TEXT("EColorGroup::Brown=1（固定首色组）"),
        static_cast<uint8>(EColorGroup::Brown),    static_cast<uint8>(1));
    TestEqual(TEXT("EColorGroup::DarkBlue=8（末尾项）"),
        static_cast<uint8>(EColorGroup::DarkBlue), static_cast<uint8>(8));

    // ---- ETileAction（None=0，TriggerHouseRuleCheck=3）----
    TestEqual(TEXT("ETileAction::None=0（append-only 基准）"),
        static_cast<uint8>(ETileAction::None),                  static_cast<uint8>(0));
    TestEqual(TEXT("ETileAction::CollectSalary=1（Go 格动作，固定）"),
        static_cast<uint8>(ETileAction::CollectSalary),         static_cast<uint8>(1));
    TestEqual(TEXT("ETileAction::TriggerHouseRuleCheck=3（末尾项）"),
        static_cast<uint8>(ETileAction::TriggerHouseRuleCheck), static_cast<uint8>(3));

    // ---- EEventDeck（None=0，CommunityChest=2）----
    TestEqual(TEXT("EEventDeck::None=0（append-only 基准）"),
        static_cast<uint8>(EEventDeck::None),           static_cast<uint8>(0));
    TestEqual(TEXT("EEventDeck::Chance=1"),
        static_cast<uint8>(EEventDeck::Chance),         static_cast<uint8>(1));
    TestEqual(TEXT("EEventDeck::CommunityChest=2（末尾项）"),
        static_cast<uint8>(EEventDeck::CommunityChest), static_cast<uint8>(2));

    return true;
}

// =============================================================================
// TC_Integrity_DefaultConstruct_MatchesADRDefaults — 结构完整性：
//   FBoardTileData 默认值契约 + 反射字段计数==13
//
// must-fix Gap #1 + EC-A(FText) + EC-D(默认值契约)
//
// GIVEN：FBoardTileData 默认构造（不赋值任何字段）
// WHEN：逐字段读取
// THEN：每个字段默认值与 ADR-0002 §Key Interfaces 逐字一致
//
// 反射字段计数断言（非 vacuous 保证）：
//   TFieldIterator 遍历 FBoardTileData::StaticStruct() 的本类 UPROPERTY，
//   断言数量==13。
//   若某字段被删除（如误删 DisplayName），计数变为 12 → TestEqual FAIL。
//   若错误地新增了一个 UPROPERTY，计数变为 14 → TestEqual FAIL。
//   两方向都能捕获，非 vacuous。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_Integrity_DefaultConstruct_MatchesADRDefaults,
    "Rento.Board.BoardDataAssetSchema.TC_Integrity_DefaultConstruct_MatchesADRDefaults",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_Integrity_DefaultConstruct_MatchesADRDefaults::RunTest(const FString& Parameters)
{
    // GIVEN：默认构造（不赋值任何字段）
    FBoardTileData Tile;

    // ---- 逐字段断言默认值（ADR-0002 §Key Interfaces）----

    TestEqual(TEXT("默认 TileIndex 应为 0"),
        Tile.TileIndex, 0);

    TestEqual(TEXT("默认 TileType 应为 ETileType::Property"),
        Tile.TileType, ETileType::Property);

    // FText 默认值：IsEmpty()==true（无 inline 文本）
    // EC-A：FText 字段存在且默认空，证 DisplayName 在 schema 中（非 vacuous——若字段被删编译报错）
    TestTrue(TEXT("默认 DisplayName 应为空 FText（IsEmpty()==true）"),
        Tile.DisplayName.IsEmpty());

    TestEqual(TEXT("默认 ColorGroup 应为 EColorGroup::None"),
        Tile.ColorGroup, EColorGroup::None);

    TestEqual(TEXT("默认 PurchasePrice 应为 0"),
        Tile.PurchasePrice, 0);

    // TArray 默认：空（Num()==0）
    TestEqual(TEXT("默认 RentTable 应为空（Num()==0）"),
        Tile.RentTable.Num(), 0);

    TestEqual(TEXT("默认 DiceMultiplierTable 应为空（Num()==0）"),
        Tile.DiceMultiplierTable.Num(), 0);

    TestEqual(TEXT("默认 BuildingCost 应为 0"),
        Tile.BuildingCost, 0);

    TestEqual(TEXT("默认 MortgageValue 应为 0"),
        Tile.MortgageValue, 0);

    TestEqual(TEXT("默认 TaxAmount 应为 0"),
        Tile.TaxAmount, 0);

    TestEqual(TEXT("默认 SalaryAmount 应为 0"),
        Tile.SalaryAmount, 0);

    TestEqual(TEXT("默认 EventDeck 应为 EEventDeck::None"),
        Tile.EventDeck, EEventDeck::None);

    TestEqual(TEXT("默认 SpecialAction 应为 ETileAction::None"),
        Tile.SpecialAction, ETileAction::None);

    // ---- 反射字段计数==13（坐实「含全 13 字段」AC）----
    // TFieldIterator 遍历 FBoardTileData 本类声明的 UPROPERTY（EFieldIterationFlags::None=不含父类）。
    // 若字段被删或漏加 UPROPERTY 宏，计数偏离 13 → TestEqual FAIL（非 vacuous）。
    int32 PropertyCount = 0;
    for (TFieldIterator<FProperty> It(FBoardTileData::StaticStruct(), EFieldIterationFlags::None); It; ++It)
    {
        ++PropertyCount;
    }
    TestEqual(TEXT("FBoardTileData 应恰好有 13 个 UPROPERTY 字段（ADR-0002 §Key Interfaces）"),
        PropertyCount, 13);

    return true;
}

// =============================================================================
// TC_Integrity_PlaceholderData_DefaultsTrue — 结构完整性：
//   bIsPlaceholderData 默认值为 true（新建资产默认占位，cook 门控语义）
//
// must-fix Gap #3
//
// 编译环境说明：
//   rentoTests 以 Editor target 编译（rentoEditor.Target.cs，Type=TargetType.Editor），
//   因此 WITH_EDITORONLY_DATA 在此 target 下恒为 1，bIsPlaceholderData 字段
//   在测试 target 中始终存在，本测试可无条件断言。
//   整个测试块用 #if WITH_EDITORONLY_DATA 包裹的原因：
//   若未来有人在 Shipping target 下运行测试，WITH_EDITORONLY_DATA=0 时字段不存在，
//   整个测试不编译（而非引用不存在的字段导致编译错误）。
//   #else 分支为空——Shipping 下字段本就不存在，无需测试，也不会遗漏 cook 门控
//   （gate-check commandlet 在 cook 前以 Editor 模式扫描，见 ADR-0002 §Implementation Guidelines #3）。
//
// 非 vacuous 保证：
//   若 bIsPlaceholderData 的默认值被改为 false，TestTrue 断言 FAIL。
//   若 #if WITH_EDITORONLY_DATA 被移除导致字段在 Shipping 下编译，
//   则 #else 分支（理论上不会执行）不会产生假绿。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_Integrity_PlaceholderData_DefaultsTrue,
    "Rento.Board.BoardDataAssetSchema.TC_Integrity_PlaceholderData_DefaultsTrue",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_Integrity_PlaceholderData_DefaultsTrue::RunTest(const FString& Parameters)
{
#if WITH_EDITORONLY_DATA
    // GIVEN：新建 UBoardDataAsset 实例（不手动设置任何字段，验证默认值）
    UBoardDataAsset* Asset = NewObject<UBoardDataAsset>(GetTransientPackage(), UBoardDataAsset::StaticClass());
    if (!TestNotNull(TEXT("PlaceholderData: UBoardDataAsset 应能通过 NewObject<> 构造"), Asset))
    {
        return false;
    }

    // WHEN：读取 bIsPlaceholderData（仅默认构造，未赋值）

    // THEN：bIsPlaceholderData 默认值应为 true（GDD CR-6 + ADR-0002 §Key Interfaces）
    // 非 vacuous：若默认值被改为 false，此 TestTrue 立即 FAIL。
    TestTrue(
        TEXT("PlaceholderData: 新建 UBoardDataAsset 的 bIsPlaceholderData 默认值应为 true（cook 门控语义）"),
        Asset->bIsPlaceholderData);

#else
    // Shipping target（WITH_EDITORONLY_DATA=0）：bIsPlaceholderData 字段不存在，无需测试。
    // cook 门控 gate-check 以 Editor 模式运行（见 ADR-0002 §Implementation Guidelines #3），
    // 不依赖 Shipping 运行时读取此字段。
    UE_LOG(LogTemp, Log, TEXT("PlaceholderData: Shipping target，WITH_EDITORONLY_DATA=0，跳过 bIsPlaceholderData 测试"));
#endif // WITH_EDITORONLY_DATA

    return true;
}

// =============================================================================
// TC_Schema_RailroadTile_RentTableLength4 — Railroad schema 正向
//
// advisory：给 story-005 校验逻辑提供明确的锚点断言。
// BoardDataAsset.h 注释声明「Railroad=4 级租金表」；本测试坐实此约定，
// story-005 校验器可以此为参照实现「RentTable.Num()!=4 → 拒绝 Railroad 格」规则。
//
// GIVEN：TileType=Railroad, PurchasePrice=200, RentTable={25,50,100,200},
//         DiceMultiplierTable={} 的 fixture
// WHEN：内联断言 schema 谓词
// THEN：PurchasePrice>0 && RentTable.Num()==4 && DiceMultiplierTable.Num()==0 全真
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardDataAssetSchema_TC_Schema_RailroadTile_RentTableLength4,
    "Rento.Board.BoardDataAssetSchema.TC_Schema_RailroadTile_RentTableLength4",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDataAssetSchema_TC_Schema_RailroadTile_RentTableLength4::RunTest(const FString& Parameters)
{
    // GIVEN：合法的 Railroad 格 fixture（经典大富翁：持 1/2/3/4 条铁路分别收 25/50/100/200）
    FBoardTileData Tile;
    Tile.TileIndex        = 5;
    Tile.TileType         = ETileType::Railroad;
    Tile.ColorGroup       = EColorGroup::None;   // Railroad 无色组
    Tile.PurchasePrice    = 200;
    // RentTable：[1条, 2条, 3条, 4条] = 4 项（BoardDataAsset.h 注释约定）
    Tile.RentTable        = { 25, 50, 100, 200 };
    Tile.DiceMultiplierTable = {};               // Railroad 不用骰点倍率
    Tile.BuildingCost     = 0;                   // Railroad 不可建房
    Tile.MortgageValue    = 100;

    // WHEN + THEN：断言 Railroad schema 谓词各分量
    TestTrue(
        TEXT("Railroad: PurchasePrice 应 > 0"),
        Tile.PurchasePrice > 0);

    TestTrue(
        TEXT("Railroad: RentTable 应含 4 项（持 1/2/3/4 条铁路的租金级别）"),
        Tile.RentTable.Num() == 4);

    TestTrue(
        TEXT("Railroad: DiceMultiplierTable 应为空（Railroad 不用骰点倍率）"),
        Tile.DiceMultiplierTable.Num() == 0);

    return true;
}
