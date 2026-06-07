// GenerateClassic40Commandlet.cpp
// =============================================================================
// Commandlet 实现：程序化生成 DA_Board_Classic40 棋盘资产
//
// story-007（board-data），ADR-0002 §Migration Plan Step 4
// 调用：UnrealEditor-Cmd rento.uproject -run=GenerateClassic40
//
// 生成规则（GDD CR-6 + 公开参考值）：
//   - 40 格布局：0=Go, 10=Jail, 20=FreeParking, 30=GoToJail
//   - Railroad: 5/15/25/35；Utility: 12/28
//   - Tax: 4(所得税 200)/38(奢侈税 100)；Chance: 7/22/36；CC: 2/17/33
//   - Property 色组：棕(1,3)/浅蓝(6,8,9)/粉(11,13,14)/橙(16,18,19)
//                    红(21,23,24)/黄(26,27,29)/绿(31,32,34)/深蓝(37,39)
//   - bIsPlaceholderData=true（temp-fill 占位，alpha gate 阻断）
//   - BoardIdentifier = FName("Classic40")
//   - ColorKeyTable：8 色组占位颜色（art bible §6 真值待定）
//
// ⚠ DisplayName 使用 IP 安全占位名（"Brown 1" / "Railroad A" 等），
//    绝不使用 "Mediterranean Avenue" 等大富翁商标地名（OQ-3 IP 安全待定）。
// =============================================================================

// 仅在 Editor 构建中编译（Shipping 构建剥离）
#if WITH_EDITOR

#include "Commandlets/GenerateClassic40Commandlet.h"

// UE 引擎头文件
#include "AssetRegistry/AssetRegistryModule.h"  // FAssetRegistryModule::AssetCreated
#include "HAL/FileManager.h"                    // IFileManager（目录创建）
#include "Misc/PackageName.h"                   // FPackageName::LongPackageNameToFilename
#include "UObject/Package.h"                    // CreatePackage / UPackage
#include "UObject/SavePackage.h"                // FSavePackageArgs

// 本项目头文件
#include "BoardDataAsset.h"

// =============================================================================
// Main — Commandlet 入口
// =============================================================================
int32 UGenerateClassic40Commandlet::Main(const FString& Params)
{
    // --------------------------------------------------------------------------
    // Step 1：确定目标 Package 路径和磁盘路径
    // --------------------------------------------------------------------------
    // 逻辑路径（Content Browser 路径）
    const FString PackagePath = TEXT("/Game/Boards/DA_Board_Classic40");
    // 磁盘路径（.uasset 文件的绝对路径）
    FString FilePath;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, FilePath, TEXT(".uasset")))
    {
        UE_LOG(LogTemp, Error,
            TEXT("GenerateClassic40: 无法将包路径转换为文件路径: %s"), *PackagePath);
        return 1;
    }

    UE_LOG(LogTemp, Display,
        TEXT("GenerateClassic40: 目标文件路径 = %s"), *FilePath);

    // --------------------------------------------------------------------------
    // Step 2：确保目录存在（Content/Boards/）
    // --------------------------------------------------------------------------
    const FString DirPath = FPaths::GetPath(FilePath);
    if (!IFileManager::Get().DirectoryExists(*DirPath))
    {
        IFileManager::Get().MakeDirectory(*DirPath, /*bTree=*/true);
        UE_LOG(LogTemp, Display, TEXT("GenerateClassic40: 已创建目录 %s"), *DirPath);
    }

    // --------------------------------------------------------------------------
    // Step 3：创建或获取 Package（幂等覆盖）
    // --------------------------------------------------------------------------
    // CreatePackage：若磁盘已存在该 package，编辑器启动 AssetRegistry 扫描会为其建
    // Linker 但仅"部分加载"。CreatePackage 返回这个部分加载的 package。
    // ⚠ 必须 FullyLoad()——否则 SavePackage 会因"资产只加载了一部分"appError 崩溃
    //   （第二次运行覆盖已有资产时的真实地雷，实测坐实）。FullyLoad 把已存在内容
    //   完整载入内存，使覆盖保存安全。首次运行（package 不存在）FullyLoad 为空操作。
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error,
            TEXT("GenerateClassic40: 创建 Package 失败: %s"), *PackagePath);
        return 1;
    }
    Package->FullyLoad();

    // --------------------------------------------------------------------------
    // Step 4：复用或新建 UBoardDataAsset 实例（幂等）
    // --------------------------------------------------------------------------
    // 若 package 已含同名资产（覆盖场景），复用并清空旧数据后重填——避免 NewObject
    // 同名冲突把旧对象 rename 成游离对象污染包。否则新建。
    UBoardDataAsset* Asset = FindObject<UBoardDataAsset>(Package, TEXT("DA_Board_Classic40"));
    if (Asset)
    {
        Asset->Tiles.Empty();
        Asset->ColorKeyTable.Empty();
        UE_LOG(LogTemp, Display,
            TEXT("GenerateClassic40: 复用已存在资产并清空旧数据（幂等覆盖）"));
    }
    else
    {
        // 使用 NewObject<> 创建资产（遵守 UE 对象生命周期纪律，禁用 new/delete）
        Asset = NewObject<UBoardDataAsset>(
            Package,
            UBoardDataAsset::StaticClass(),
            TEXT("DA_Board_Classic40"),
            RF_Public | RF_Standalone);
    }

    if (!Asset)
    {
        UE_LOG(LogTemp, Error,
            TEXT("GenerateClassic40: 创建 UBoardDataAsset 实例失败"));
        return 1;
    }

    // 标记 Package 为已修改（确保保存时不被跳过）。
    // 用 MarkPackageDirty()（非底层 SetDirtyFlag）：广播 OnPackageDirtyStateUpdated，
    // 与 UE package 生命周期约定一致（code-review F-2）。commandlet 下监听者为零，
    // 但若未来迁编辑器工具/Python 环境，编辑器 UI 能正确感知 package 变脏。
    Package->MarkPackageDirty();

    // ---- 顶层元数据 ----
    // 棋盘逻辑 ID（与资产路径/PrimaryAssetId 解耦，ADR-0002 R3）
    Asset->BoardIdentifier = FName(TEXT("Classic40"));

    // bIsPlaceholderData = true（temp-fill 占位，story-007 ADR-0002 §Migration Plan Step 4）
    // Alpha gate commandlet（BoardCookGate）会检测此字段并阻断构建
#if WITH_EDITORONLY_DATA
    Asset->bIsPlaceholderData = true;
#endif

    // ---- ColorKeyTable：8 色组占位色值（art bible §6 真值待 OQ-3/OQ-5 确认）----
    // 占位色值使用合理的直觉色，后续由美术替换
    Asset->ColorKeyTable.Add(EColorGroup::Brown,     FColor(139,  69,  19, 255));  // 棕色
    Asset->ColorKeyTable.Add(EColorGroup::LightBlue, FColor(173, 216, 230, 255));  // 淡蓝色
    Asset->ColorKeyTable.Add(EColorGroup::Magenta,   FColor(255,   0, 255, 255));  // 品红/粉色
    Asset->ColorKeyTable.Add(EColorGroup::Orange,    FColor(255, 165,   0, 255));  // 橙色
    Asset->ColorKeyTable.Add(EColorGroup::Red,       FColor(220,  20,  60, 255));  // 红色
    Asset->ColorKeyTable.Add(EColorGroup::Yellow,    FColor(255, 255,   0, 255));  // 黄色
    Asset->ColorKeyTable.Add(EColorGroup::Green,     FColor( 34, 139,  34, 255));  // 绿色
    Asset->ColorKeyTable.Add(EColorGroup::DarkBlue,  FColor(  0,   0, 139, 255));  // 深蓝色

    // --------------------------------------------------------------------------
    // Step 5：按 GDD CR-6 顺序构造 40 格数据
    //
    // 布局规则：
    //   角格：0=Go / 10=Jail / 20=FreeParking / 30=GoToJail
    //   Railroad：5/15/25/35
    //   Utility：12/28
    //   Tax：4(所得税200) / 38(奢侈税100)
    //   Chance：7/22/36
    //   CommunityChest：2/17/33
    //   Property 按色组+索引如下分布（棕→深蓝，价格递增）：
    //     棕(Brown)=1,3 / 浅蓝(LightBlue)=6,8,9 / 粉(Magenta)=11,13,14
    //     橙(Orange)=16,18,19 / 红(Red)=21,23,24 / 黄(Yellow)=26,27,29
    //     绿(Green)=31,32,34 / 深蓝(DarkBlue)=37,39
    //
    // ⚠ 所有 DisplayName 使用 IP 安全占位名，绝不使用商标地名
    // --------------------------------------------------------------------------
    Asset->Tiles.Reserve(40);

    // --- 索引 0：Go（起点格） ---
    Asset->Tiles.Add(MakeSpecialTile(0, ETileType::Go,
        NSLOCTEXT("Board", "Tile_Go", "起点"),
        /*TaxAmt=*/0, /*SalaryAmt=*/200,
        ETileAction::CollectSalary, EEventDeck::None));

    // --- 索引 1：Property（棕色1） ---
    Asset->Tiles.Add(MakePropertyTile(1,
        NSLOCTEXT("Board", "Tile_P01", "Brown 1"),
        EColorGroup::Brown, 60, {2,10,30,90,160,250}, 50, 30));

    // --- 索引 2：CommunityChest ---
    Asset->Tiles.Add(MakeSpecialTile(2, ETileType::CommunityChest,
        NSLOCTEXT("Board", "Tile_CC1", "公共基金 1"),
        0, 0, ETileAction::None, EEventDeck::CommunityChest));

    // --- 索引 3：Property（棕色2） ---
    Asset->Tiles.Add(MakePropertyTile(3,
        NSLOCTEXT("Board", "Tile_P03", "Brown 2"),
        EColorGroup::Brown, 60, {4,20,60,180,320,450}, 50, 30));

    // --- 索引 4：Tax（所得税） ---
    Asset->Tiles.Add(MakeSpecialTile(4, ETileType::Tax,
        NSLOCTEXT("Board", "Tile_Tax1", "所得税"),
        /*TaxAmt=*/200));

    // --- 索引 5：Railroad（A线） ---
    Asset->Tiles.Add(MakeRailroadTile(5,
        NSLOCTEXT("Board", "Tile_RR1", "Railroad A")));

    // --- 索引 6：Property（浅蓝1） ---
    Asset->Tiles.Add(MakePropertyTile(6,
        NSLOCTEXT("Board", "Tile_P06", "Light Blue 1"),
        EColorGroup::LightBlue, 100, {6,30,90,270,400,550}, 50, 50));

    // --- 索引 7：Chance ---
    Asset->Tiles.Add(MakeSpecialTile(7, ETileType::Chance,
        NSLOCTEXT("Board", "Tile_Ch1", "机会 1"),
        0, 0, ETileAction::None, EEventDeck::Chance));

    // --- 索引 8：Property（浅蓝2） ---
    Asset->Tiles.Add(MakePropertyTile(8,
        NSLOCTEXT("Board", "Tile_P08", "Light Blue 2"),
        EColorGroup::LightBlue, 100, {6,30,90,270,400,550}, 50, 50));

    // --- 索引 9：Property（浅蓝3） ---
    Asset->Tiles.Add(MakePropertyTile(9,
        NSLOCTEXT("Board", "Tile_P09", "Light Blue 3"),
        EColorGroup::LightBlue, 120, {8,40,100,300,450,600}, 50, 60));

    // --- 索引 10：Jail（监狱角格） ---
    Asset->Tiles.Add(MakeSpecialTile(10, ETileType::Jail,
        NSLOCTEXT("Board", "Tile_Jail", "监狱")));

    // --- 索引 11：Property（粉色1） ---
    Asset->Tiles.Add(MakePropertyTile(11,
        NSLOCTEXT("Board", "Tile_P11", "Magenta 1"),
        EColorGroup::Magenta, 140, {10,50,150,450,625,750}, 100, 70));

    // --- 索引 12：Utility（公用事业1） ---
    Asset->Tiles.Add(MakeUtilityTile(12,
        NSLOCTEXT("Board", "Tile_U1", "Utility A")));

    // --- 索引 13：Property（粉色2） ---
    Asset->Tiles.Add(MakePropertyTile(13,
        NSLOCTEXT("Board", "Tile_P13", "Magenta 2"),
        EColorGroup::Magenta, 140, {10,50,150,450,625,750}, 100, 70));

    // --- 索引 14：Property（粉色3） ---
    Asset->Tiles.Add(MakePropertyTile(14,
        NSLOCTEXT("Board", "Tile_P14", "Magenta 3"),
        EColorGroup::Magenta, 160, {12,60,180,500,700,900}, 100, 80));

    // --- 索引 15：Railroad（B线） ---
    Asset->Tiles.Add(MakeRailroadTile(15,
        NSLOCTEXT("Board", "Tile_RR2", "Railroad B")));

    // --- 索引 16：Property（橙色1） ---
    Asset->Tiles.Add(MakePropertyTile(16,
        NSLOCTEXT("Board", "Tile_P16", "Orange 1"),
        EColorGroup::Orange, 180, {14,70,200,550,750,950}, 100, 90));

    // --- 索引 17：CommunityChest ---
    Asset->Tiles.Add(MakeSpecialTile(17, ETileType::CommunityChest,
        NSLOCTEXT("Board", "Tile_CC2", "公共基金 2"),
        0, 0, ETileAction::None, EEventDeck::CommunityChest));

    // --- 索引 18：Property（橙色2） ---
    Asset->Tiles.Add(MakePropertyTile(18,
        NSLOCTEXT("Board", "Tile_P18", "Orange 2"),
        EColorGroup::Orange, 180, {14,70,200,550,750,950}, 100, 90));

    // --- 索引 19：Property（橙色3） ---
    Asset->Tiles.Add(MakePropertyTile(19,
        NSLOCTEXT("Board", "Tile_P19", "Orange 3"),
        EColorGroup::Orange, 200, {16,80,220,600,800,1000}, 100, 100));

    // --- 索引 20：FreeParking（免费停车） ---
    Asset->Tiles.Add(MakeSpecialTile(20, ETileType::FreeParking,
        NSLOCTEXT("Board", "Tile_FP", "免费停车")));

    // --- 索引 21：Property（红色1） ---
    Asset->Tiles.Add(MakePropertyTile(21,
        NSLOCTEXT("Board", "Tile_P21", "Red 1"),
        EColorGroup::Red, 220, {18,90,250,700,875,1050}, 150, 110));

    // --- 索引 22：Chance ---
    Asset->Tiles.Add(MakeSpecialTile(22, ETileType::Chance,
        NSLOCTEXT("Board", "Tile_Ch2", "机会 2"),
        0, 0, ETileAction::None, EEventDeck::Chance));

    // --- 索引 23：Property（红色2） ---
    Asset->Tiles.Add(MakePropertyTile(23,
        NSLOCTEXT("Board", "Tile_P23", "Red 2"),
        EColorGroup::Red, 220, {18,90,250,700,875,1050}, 150, 110));

    // --- 索引 24：Property（红色3） ---
    Asset->Tiles.Add(MakePropertyTile(24,
        NSLOCTEXT("Board", "Tile_P24", "Red 3"),
        EColorGroup::Red, 240, {20,100,300,750,925,1100}, 150, 120));

    // --- 索引 25：Railroad（C线） ---
    Asset->Tiles.Add(MakeRailroadTile(25,
        NSLOCTEXT("Board", "Tile_RR3", "Railroad C")));

    // --- 索引 26：Property（黄色1） ---
    Asset->Tiles.Add(MakePropertyTile(26,
        NSLOCTEXT("Board", "Tile_P26", "Yellow 1"),
        EColorGroup::Yellow, 260, {22,110,330,800,975,1150}, 150, 130));

    // --- 索引 27：Property（黄色2） ---
    Asset->Tiles.Add(MakePropertyTile(27,
        NSLOCTEXT("Board", "Tile_P27", "Yellow 2"),
        EColorGroup::Yellow, 260, {22,110,330,800,975,1150}, 150, 130));

    // --- 索引 28：Utility（公用事业2） ---
    Asset->Tiles.Add(MakeUtilityTile(28,
        NSLOCTEXT("Board", "Tile_U2", "Utility B")));

    // --- 索引 29：Property（黄色3） ---
    Asset->Tiles.Add(MakePropertyTile(29,
        NSLOCTEXT("Board", "Tile_P29", "Yellow 3"),
        EColorGroup::Yellow, 280, {24,120,360,850,1025,1200}, 150, 140));

    // --- 索引 30：GoToJail（前往监狱） ---
    Asset->Tiles.Add(MakeSpecialTile(30, ETileType::GoToJail,
        NSLOCTEXT("Board", "Tile_GTJ", "前往监狱"),
        0, 0, ETileAction::GoToJail));

    // --- 索引 31：Property（绿色1） ---
    Asset->Tiles.Add(MakePropertyTile(31,
        NSLOCTEXT("Board", "Tile_P31", "Green 1"),
        EColorGroup::Green, 300, {26,130,390,900,1100,1275}, 200, 150));

    // --- 索引 32：Property（绿色2） ---
    Asset->Tiles.Add(MakePropertyTile(32,
        NSLOCTEXT("Board", "Tile_P32", "Green 2"),
        EColorGroup::Green, 300, {26,130,390,900,1100,1275}, 200, 150));

    // --- 索引 33：CommunityChest ---
    Asset->Tiles.Add(MakeSpecialTile(33, ETileType::CommunityChest,
        NSLOCTEXT("Board", "Tile_CC3", "公共基金 3"),
        0, 0, ETileAction::None, EEventDeck::CommunityChest));

    // --- 索引 34：Property（绿色3） ---
    Asset->Tiles.Add(MakePropertyTile(34,
        NSLOCTEXT("Board", "Tile_P34", "Green 3"),
        EColorGroup::Green, 320, {28,150,450,1000,1200,1400}, 200, 160));

    // --- 索引 35：Railroad（D线） ---
    Asset->Tiles.Add(MakeRailroadTile(35,
        NSLOCTEXT("Board", "Tile_RR4", "Railroad D")));

    // --- 索引 36：Chance ---
    Asset->Tiles.Add(MakeSpecialTile(36, ETileType::Chance,
        NSLOCTEXT("Board", "Tile_Ch3", "机会 3"),
        0, 0, ETileAction::None, EEventDeck::Chance));

    // --- 索引 37：Property（深蓝1） ---
    Asset->Tiles.Add(MakePropertyTile(37,
        NSLOCTEXT("Board", "Tile_P37", "Dark Blue 1"),
        EColorGroup::DarkBlue, 350, {35,175,500,1100,1300,1500}, 200, 175));

    // --- 索引 38：Tax（奢侈税） ---
    Asset->Tiles.Add(MakeSpecialTile(38, ETileType::Tax,
        NSLOCTEXT("Board", "Tile_Tax2", "奢侈税"),
        /*TaxAmt=*/100));

    // --- 索引 39：Property（深蓝2） ---
    Asset->Tiles.Add(MakePropertyTile(39,
        NSLOCTEXT("Board", "Tile_P39", "Dark Blue 2"),
        EColorGroup::DarkBlue, 400, {50,200,600,1400,1700,2000}, 200, 200));

    // --------------------------------------------------------------------------
    // Step 6：校验格子总数
    // --------------------------------------------------------------------------
    const int32 TileCount = Asset->Tiles.Num();
    if (TileCount != 40)
    {
        UE_LOG(LogTemp, Error,
            TEXT("GenerateClassic40: 格子总数异常（期望 40，实际 %d）"), TileCount);
        return 1;
    }

    // --------------------------------------------------------------------------
    // Step 7：通知 AssetRegistry 新资产创建（编辑器内 ContentBrowser 刷新）
    // --------------------------------------------------------------------------
    // FAssetRegistryModule::AssetCreated 是 5.7 下通知 AssetRegistry 的标准方法
    FAssetRegistryModule::AssetCreated(Asset);

    // --------------------------------------------------------------------------
    // Step 8：保存 Package（使用 UE5.7 的 FSavePackageArgs 新签名）
    //
    // ⚠ UE5.6+ 弃用了旧的多参数 FSavePackageArgs 构造函数（已标 UE_DEPRECATED(5.6)）。
    //   此处使用默认构造 + 逐字段赋值（5.6+ 推荐写法）。
    //   TopLevelFlags = RF_Public | RF_Standalone 确保顶层对象被保存入包。
    // --------------------------------------------------------------------------
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags     = SAVE_NoError;           // 失败转返回值（code 检查）而非 appError 崩 CI
    SaveArgs.bSlowTask     = false;                  // 无进度条（commandlet 环境）

    // 调用 UPackage::Save 保存资产到磁盘
    const FSavePackageResultStruct SaveResult = UPackage::Save(
        Package,
        Asset,
        *FilePath,
        SaveArgs);

    if (SaveResult.Result != ESavePackageResult::Success)
    {
        UE_LOG(LogTemp, Error,
            TEXT("GenerateClassic40: 保存 Package 失败（结果码=%d），路径=%s"),
            static_cast<int32>(SaveResult.Result), *FilePath);
        return 1;
    }

    // --------------------------------------------------------------------------
    // Step 9：成功报告
    // --------------------------------------------------------------------------
    UE_LOG(LogTemp, Display,
        TEXT("GenerateClassic40: 已生成 %d 格，保存到 %s，bIsPlaceholderData=true（temp-fill）"),
        TileCount, *FilePath);

    return 0;
}

// =============================================================================
// 辅助函数：构造 Property 格
// =============================================================================
FBoardTileData UGenerateClassic40Commandlet::MakePropertyTile(
    int32 Index,
    const FText& Name,
    EColorGroup Group,
    int32 Price,
    TArray<int32> RentValues,
    int32 BuildCost,
    int32 Mortgage)
{
    FBoardTileData Tile;
    Tile.TileIndex    = Index;
    Tile.TileType     = ETileType::Property;
    Tile.DisplayName  = Name;
    Tile.ColorGroup   = Group;
    Tile.PurchasePrice = Price;
    Tile.RentTable    = MoveTemp(RentValues);   // 避免数组拷贝
    Tile.BuildingCost = BuildCost;
    Tile.MortgageValue = Mortgage;
    // DiceMultiplierTable 保持空（Property 不用骰点倍率）
    // TaxAmount / SalaryAmount / SpecialAction / EventDeck 保持默认 0/None
    return Tile;
}

// =============================================================================
// 辅助函数：构造 Railroad 格
// =============================================================================
FBoardTileData UGenerateClassic40Commandlet::MakeRailroadTile(int32 Index, const FText& Name)
{
    FBoardTileData Tile;
    Tile.TileIndex    = Index;
    Tile.TileType     = ETileType::Railroad;
    Tile.DisplayName  = Name;
    Tile.ColorGroup   = EColorGroup::None;   // Railroad 无色组
    Tile.PurchasePrice = 200;
    // RentTable：[持1条, 持2条, 持3条, 持4条]（AC-5 Railroad=4 项）
    Tile.RentTable    = {25, 50, 100, 200};
    Tile.BuildingCost = 0;      // Railroad 不可建房
    Tile.MortgageValue = 100;
    // DiceMultiplierTable 保持空（Railroad 不用骰点倍率）
    return Tile;
}

// =============================================================================
// 辅助函数：构造 Utility 格
// =============================================================================
FBoardTileData UGenerateClassic40Commandlet::MakeUtilityTile(int32 Index, const FText& Name)
{
    FBoardTileData Tile;
    Tile.TileIndex    = Index;
    Tile.TileType     = ETileType::Utility;
    Tile.DisplayName  = Name;
    Tile.ColorGroup   = EColorGroup::None;   // Utility 无色组
    Tile.PurchasePrice = 150;
    // DiceMultiplierTable：[单持倍率=4, 双持倍率=10]（AC-4b/AC-5 Utility=2 项）
    Tile.DiceMultiplierTable = {4, 10};
    // RentTable 保持空（Utility 不用 RentTable，AC-4b/AC-5）
    Tile.BuildingCost = 0;      // Utility 不可建房
    Tile.MortgageValue = 75;
    return Tile;
}

// =============================================================================
// 辅助函数：构造特殊格（角格/事件格/税格/Go格）
// =============================================================================
FBoardTileData UGenerateClassic40Commandlet::MakeSpecialTile(
    int32 Index,
    ETileType Type,
    const FText& Name,
    int32 TaxAmt,
    int32 SalaryAmt,
    ETileAction Action,
    EEventDeck Deck)
{
    FBoardTileData Tile;
    Tile.TileIndex     = Index;
    Tile.TileType      = Type;
    Tile.DisplayName   = Name;
    Tile.ColorGroup    = EColorGroup::None;   // 非地产格无色组
    Tile.TaxAmount     = TaxAmt;
    Tile.SalaryAmount  = SalaryAmt;
    Tile.SpecialAction = Action;
    Tile.EventDeck     = Deck;
    // PurchasePrice / RentTable / DiceMultiplierTable / BuildingCost / MortgageValue 保持默认 0/空
    return Tile;
}

#endif // WITH_EDITOR
