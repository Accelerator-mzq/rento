// Classic40AssetValidationTest.cpp
// =============================================================================
// [Config] 资产校验 harness：DA_Board_Classic40 真磁盘资产验证
// story-007（board-data）AC-3 / 4-asset / 4b / 4c / 5 / 6 / 7 / 7b。
//
// 物理路径：Source/rentoTests/Private/Classic40AssetValidationTest.cpp
// 逻辑分类：tests/asset-validation/board-data/classic40_asset_validation_test.cpp
//           （文档记录，与 FF-001/FF-003/BD-001 .cpp 落 Private、逻辑路径分离的惯例一致）
// Automation 类目：Rento.Board.Classic40AssetValidation
//
// 测试性质（与 [Logic] schema 测试 BD-001 的关键区别）：
//   - 本测试加载**真磁盘资产** /Game/Boards/DA_Board_Classic40（须先跑 GenerateClassic40
//     commandlet 生成），而非代码构造 fixture——依赖资产 temp-fill 状态（nightly/staging gate）。
//   - rentoTests 是 Editor target 模块，UnrealEditor-Cmd 运行环境 WITH_EDITOR=1、
//     WITH_EDITORONLY_DATA=1，可读 bIsPlaceholderData、可加载磁盘资产（-nullrhi 不妨碍：
//     -nullrhi 仅关 RHI，不改 editor 二进制能力）。
//
// ⚠ 校验 oracle 独立硬编码自 GDD CR-6 规格（见 GClassic40Oracle），**不** import/复用
//    GenerateClassic40Commandlet 的数值表，故即便由生成器产出，本测试仍能抓出手改漂移、
//    保存失败、布局错误（非同义反复）。AC-4/4b/5/7b 是 shape/存在性断言（>0、Num()==N）天然非 vacuous。
//
// ⚠ 资产未找到时（生成器未跑）：AddError 明确报错，**绝不静默 pass**（防 vacuous false-green）。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "BoardDataAsset.h"
#include "BoardDataAssetHost.h"   // TC_AC6b 经 GetTilesInGroup 接口验真资产（code-review F-3）

// =============================================================================
// 资产路径常量 + 独立 CR-6 oracle
// =============================================================================

/** DA_Board_Classic40 的对象路径（Package.Object 形式）。生成器保存于 /Game/Boards/ */
static const TCHAR* GClassic40ObjectPath =
    TEXT("/Game/Boards/DA_Board_Classic40.DA_Board_Classic40");

namespace
{
    /** CR-6 oracle 单格期望（索引 → 期望类型 + 期望色组）。独立取自 GDD CR-6，非生成器表。 */
    struct FExpectedTile
    {
        ETileType   Type;
        EColorGroup Group;  // 仅 Property 格非 None；其余 None
    };

    /**
     * GDD CR-6 经典 40 格布局 oracle（索引 0..39）。
     * 角 0/10/20/30；Railroad 5/15/25/35；Utility 12/28；Tax 4/38；
     * Chance 7/22/36；CommunityChest 2/17/33；
     * Property 色组：棕 1,3 / 浅蓝 6,8,9 / 粉 11,13,14 / 橙 16,18,19 /
     *               红 21,23,24 / 黄 26,27,29 / 绿 31,32,34 / 深蓝 37,39。
     */
    static const FExpectedTile GClassic40Oracle[40] =
    {
        /*0 */ { ETileType::Go,             EColorGroup::None      },
        /*1 */ { ETileType::Property,       EColorGroup::Brown     },
        /*2 */ { ETileType::CommunityChest, EColorGroup::None      },
        /*3 */ { ETileType::Property,       EColorGroup::Brown     },
        /*4 */ { ETileType::Tax,            EColorGroup::None      },
        /*5 */ { ETileType::Railroad,       EColorGroup::None      },
        /*6 */ { ETileType::Property,       EColorGroup::LightBlue },
        /*7 */ { ETileType::Chance,         EColorGroup::None      },
        /*8 */ { ETileType::Property,       EColorGroup::LightBlue },
        /*9 */ { ETileType::Property,       EColorGroup::LightBlue },
        /*10*/ { ETileType::Jail,           EColorGroup::None      },
        /*11*/ { ETileType::Property,       EColorGroup::Magenta   },
        /*12*/ { ETileType::Utility,        EColorGroup::None      },
        /*13*/ { ETileType::Property,       EColorGroup::Magenta   },
        /*14*/ { ETileType::Property,       EColorGroup::Magenta   },
        /*15*/ { ETileType::Railroad,       EColorGroup::None      },
        /*16*/ { ETileType::Property,       EColorGroup::Orange    },
        /*17*/ { ETileType::CommunityChest, EColorGroup::None      },
        /*18*/ { ETileType::Property,       EColorGroup::Orange    },
        /*19*/ { ETileType::Property,       EColorGroup::Orange    },
        /*20*/ { ETileType::FreeParking,    EColorGroup::None      },
        /*21*/ { ETileType::Property,       EColorGroup::Red       },
        /*22*/ { ETileType::Chance,         EColorGroup::None      },
        /*23*/ { ETileType::Property,       EColorGroup::Red       },
        /*24*/ { ETileType::Property,       EColorGroup::Red       },
        /*25*/ { ETileType::Railroad,       EColorGroup::None      },
        /*26*/ { ETileType::Property,       EColorGroup::Yellow    },
        /*27*/ { ETileType::Property,       EColorGroup::Yellow    },
        /*28*/ { ETileType::Utility,        EColorGroup::None      },
        /*29*/ { ETileType::Property,       EColorGroup::Yellow    },
        /*30*/ { ETileType::GoToJail,       EColorGroup::None      },
        /*31*/ { ETileType::Property,       EColorGroup::Green     },
        /*32*/ { ETileType::Property,       EColorGroup::Green     },
        /*33*/ { ETileType::CommunityChest, EColorGroup::None      },
        /*34*/ { ETileType::Property,       EColorGroup::Green     },
        /*35*/ { ETileType::Railroad,       EColorGroup::None      },
        /*36*/ { ETileType::Chance,         EColorGroup::None      },
        /*37*/ { ETileType::Property,       EColorGroup::DarkBlue  },
        /*38*/ { ETileType::Tax,            EColorGroup::None      },
        /*39*/ { ETileType::Property,       EColorGroup::DarkBlue  },
    };

    /**
     * 加载 DA_Board_Classic40 真磁盘资产。
     * 失败（生成器未跑/资产损坏）时 AddError 并返回 nullptr——调用方须早退，绝不静默 pass。
     */
    static UBoardDataAsset* LoadClassic40(FAutomationTestBase& Test)
    {
        UBoardDataAsset* Asset = LoadObject<UBoardDataAsset>(nullptr, GClassic40ObjectPath);
        if (!Asset)
        {
            Test.AddError(FString::Printf(
                TEXT("未找到资产 %s——请先运行 GenerateClassic40 commandlet 生成 temp-fill 资产。"),
                GClassic40ObjectPath));
            return nullptr;
        }
        // 基本结构前提：必须恰 40 格（否则后续逐格断言越界/无意义）
        if (Asset->Tiles.Num() != 40)
        {
            Test.AddError(FString::Printf(
                TEXT("资产 %s 格子数==%d，期望 40（资产损坏或生成不完整）。"),
                GClassic40ObjectPath, Asset->Tiles.Num()));
            return nullptr;
        }
        return Asset;
    }
}  // namespace

// =============================================================================
// TC-AC3 — 类型精确计数（CR-2 类型完整）
//
// 遍历 40 格 TileType，断言各枚举出现次数精确：
//   Go=1 / Jail=1 / FreeParking=1 / GoToJail=1 / Tax=2 / Chance=3 /
//   CommunityChest=3 / Railroad=4 / Utility=2 / Property=22
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClassic40_TC_AC3_TileTypeCounts,
    "Rento.Board.Classic40AssetValidation.TC_AC3_TileTypeCounts",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClassic40_TC_AC3_TileTypeCounts::RunTest(const FString& Parameters)
{
    UBoardDataAsset* Asset = LoadClassic40(*this);
    if (!Asset) { return false; }

    // 统计每种类型出现次数
    TMap<ETileType, int32> Counts;
    for (const FBoardTileData& Tile : Asset->Tiles)
    {
        Counts.FindOrAdd(Tile.TileType)++;
    }

    auto CountOf = [&Counts](ETileType T) { return Counts.FindRef(T); };

    TestEqual(TEXT("Go 计数"),             CountOf(ETileType::Go),             1);
    TestEqual(TEXT("Jail 计数"),           CountOf(ETileType::Jail),           1);
    TestEqual(TEXT("FreeParking 计数"),    CountOf(ETileType::FreeParking),    1);
    TestEqual(TEXT("GoToJail 计数"),       CountOf(ETileType::GoToJail),       1);
    TestEqual(TEXT("Tax 计数"),            CountOf(ETileType::Tax),            2);
    TestEqual(TEXT("Chance 计数"),         CountOf(ETileType::Chance),         3);
    TestEqual(TEXT("CommunityChest 计数"), CountOf(ETileType::CommunityChest), 3);
    TestEqual(TEXT("Railroad 计数"),       CountOf(ETileType::Railroad),       4);
    TestEqual(TEXT("Utility 计数"),        CountOf(ETileType::Utility),        2);
    TestEqual(TEXT("Property 计数"),       CountOf(ETileType::Property),       22);

    return true;
}

// =============================================================================
// TC-AC4asset — temp-fill 已写入（CR-6）
//
// 每个 Property 格 PurchasePrice>0 且 BuildingCost>0（证 temp-fill 已实际写入资产，非默认 0）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClassic40_TC_AC4asset_PropertyMonetaryNonZero,
    "Rento.Board.Classic40AssetValidation.TC_AC4asset_PropertyMonetaryNonZero",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClassic40_TC_AC4asset_PropertyMonetaryNonZero::RunTest(const FString& Parameters)
{
    UBoardDataAsset* Asset = LoadClassic40(*this);
    if (!Asset) { return false; }

    int32 PropertyCount = 0;
    for (const FBoardTileData& Tile : Asset->Tiles)
    {
        if (Tile.TileType == ETileType::Property)
        {
            ++PropertyCount;
            TestTrue(FString::Printf(TEXT("格 %d PurchasePrice>0"), Tile.TileIndex),
                Tile.PurchasePrice > 0);
            TestTrue(FString::Printf(TEXT("格 %d BuildingCost>0"), Tile.TileIndex),
                Tile.BuildingCost > 0);
        }
    }
    // 自证遍历到了全部 Property（防"零 Property 时 vacuous 通过"）
    TestEqual(TEXT("遍历到的 Property 格数"), PropertyCount, 22);

    return true;
}

// =============================================================================
// TC-AC4b — Utility schema（CR-3）
//
// 每个 Utility 格：PurchasePrice>0 且 DiceMultiplierTable.Num()==2 且 RentTable.Num()==0。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClassic40_TC_AC4b_UtilitySchema,
    "Rento.Board.Classic40AssetValidation.TC_AC4b_UtilitySchema",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClassic40_TC_AC4b_UtilitySchema::RunTest(const FString& Parameters)
{
    UBoardDataAsset* Asset = LoadClassic40(*this);
    if (!Asset) { return false; }

    int32 UtilityCount = 0;
    for (const FBoardTileData& Tile : Asset->Tiles)
    {
        if (Tile.TileType == ETileType::Utility)
        {
            ++UtilityCount;
            TestTrue(FString::Printf(TEXT("Utility 格 %d PurchasePrice>0"), Tile.TileIndex),
                Tile.PurchasePrice > 0);
            TestEqual(FString::Printf(TEXT("Utility 格 %d DiceMultiplierTable.Num()"), Tile.TileIndex),
                Tile.DiceMultiplierTable.Num(), 2);
            TestEqual(FString::Printf(TEXT("Utility 格 %d RentTable.Num()"), Tile.TileIndex),
                Tile.RentTable.Num(), 0);
        }
    }
    TestEqual(TEXT("遍历到的 Utility 格数"), UtilityCount, 2);

    return true;
}

// =============================================================================
// TC-AC4c — temp-fill 门控字段可读（CR-6 cook 门控）
//
// 读顶层 bIsPlaceholderData（WITH_EDITORONLY_DATA，Editor target 恒可读）。
// MVP/nightly 阶段语义：temp-fill 资产应为 true（generator 置 true）。
// 本测试只断言"字段可读 + 当前 MVP temp-fill 态==true"，**不**因 true 失败——
// 硬 Alpha gate（要求 false）由 BoardCookGate commandlet 退出码承载（职责切分，story AC-4c）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClassic40_TC_AC4c_PlaceholderFlagReadable,
    "Rento.Board.Classic40AssetValidation.TC_AC4c_PlaceholderFlagReadable",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClassic40_TC_AC4c_PlaceholderFlagReadable::RunTest(const FString& Parameters)
{
    UBoardDataAsset* Asset = LoadClassic40(*this);
    if (!Asset) { return false; }

#if WITH_EDITORONLY_DATA
    // MVP temp-fill 态：generator 置 true。此期望随经济(5)覆盖真值后翻转为 false（fixture 期望同步更新）。
    TestTrue(TEXT("MVP 阶段 bIsPlaceholderData==true（temp-fill 占位，Alpha gate 由 cook-gate commandlet 守）"),
        Asset->bIsPlaceholderData);
#else
    AddError(TEXT("WITH_EDITORONLY_DATA==0：本测试须在 Editor target 运行（rentoTests 即 Editor 模块）。"));
    return false;
#endif

    return true;
}

// =============================================================================
// TC-AC5 — 数组语义长度（CR-4）
//
// Property RentTable==6 / Railroad RentTable==4 / Utility DiceMultiplierTable==2 且 RentTable==0 /
// 非购买格（非 Property/Railroad/Utility）RentTable==0。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClassic40_TC_AC5_ArrayLengthsByType,
    "Rento.Board.Classic40AssetValidation.TC_AC5_ArrayLengthsByType",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClassic40_TC_AC5_ArrayLengthsByType::RunTest(const FString& Parameters)
{
    UBoardDataAsset* Asset = LoadClassic40(*this);
    if (!Asset) { return false; }

    for (const FBoardTileData& Tile : Asset->Tiles)
    {
        switch (Tile.TileType)
        {
        case ETileType::Property:
            TestEqual(FString::Printf(TEXT("Property 格 %d RentTable.Num()"), Tile.TileIndex),
                Tile.RentTable.Num(), 6);
            break;
        case ETileType::Railroad:
            TestEqual(FString::Printf(TEXT("Railroad 格 %d RentTable.Num()"), Tile.TileIndex),
                Tile.RentTable.Num(), 4);
            break;
        case ETileType::Utility:
            TestEqual(FString::Printf(TEXT("Utility 格 %d DiceMultiplierTable.Num()"), Tile.TileIndex),
                Tile.DiceMultiplierTable.Num(), 2);
            TestEqual(FString::Printf(TEXT("Utility 格 %d RentTable.Num()"), Tile.TileIndex),
                Tile.RentTable.Num(), 0);
            break;
        default:
            // 非购买格（角/税/事件）RentTable 必为空
            TestEqual(FString::Printf(TEXT("非购买格 %d RentTable.Num()"), Tile.TileIndex),
                Tile.RentTable.Num(), 0);
            break;
        }
    }

    return true;
}

// =============================================================================
// TC-AC6 — 色组成员（CR-5）
//
// 按 ColorGroup 统计 Property 成员：棕2/浅蓝3/粉3/橙3/红3/黄3/绿3/深蓝2，合计 22。
// 直接遍历 Tiles 按 ColorGroup 计数（不起 host subsystem，Config 测试无需 World 生命周期）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClassic40_TC_AC6_ColorGroupCounts,
    "Rento.Board.Classic40AssetValidation.TC_AC6_ColorGroupCounts",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClassic40_TC_AC6_ColorGroupCounts::RunTest(const FString& Parameters)
{
    UBoardDataAsset* Asset = LoadClassic40(*this);
    if (!Asset) { return false; }

    TMap<EColorGroup, int32> GroupCounts;
    for (const FBoardTileData& Tile : Asset->Tiles)
    {
        // 仅统计真正归属色组的格（Property 格 ColorGroup != None）
        if (Tile.ColorGroup != EColorGroup::None)
        {
            GroupCounts.FindOrAdd(Tile.ColorGroup)++;
        }
    }

    auto GroupOf = [&GroupCounts](EColorGroup G) { return GroupCounts.FindRef(G); };

    TestEqual(TEXT("棕色组成员数"),   GroupOf(EColorGroup::Brown),     2);
    TestEqual(TEXT("浅蓝色组成员数"), GroupOf(EColorGroup::LightBlue), 3);
    TestEqual(TEXT("粉色组成员数"),   GroupOf(EColorGroup::Magenta),   3);
    TestEqual(TEXT("橙色组成员数"),   GroupOf(EColorGroup::Orange),    3);
    TestEqual(TEXT("红色组成员数"),   GroupOf(EColorGroup::Red),       3);
    TestEqual(TEXT("黄色组成员数"),   GroupOf(EColorGroup::Yellow),    3);
    TestEqual(TEXT("绿色组成员数"),   GroupOf(EColorGroup::Green),     3);
    TestEqual(TEXT("深蓝色组成员数"), GroupOf(EColorGroup::DarkBlue),  2);

    // 合计自证
    int32 Total = 0;
    for (const TPair<EColorGroup, int32>& Pair : GroupCounts) { Total += Pair.Value; }
    TestEqual(TEXT("色组成员合计"), Total, 22);

    return true;
}

// =============================================================================
// TC-AC6b — 色组成员经 GetTilesInGroup 接口验真资产（CR-5，code-review F-3）
//
// story AC-6 + Out of Scope line 59 明确"本 story 调用 GetTilesInGroup 做 AC-6 资产校验"。
// TC_AC6 直接遍历是独立 oracle（不受接口 bug 干扰）；本 test 补"真资产 × 真查询接口"的
// 集成覆盖——经 UBoardDataAssetHost::GetTilesInGroup 对 8 色组计数（AC-6 规格指定方法）。
// 复用 bd-004 的 CreateTestWorld + BoardToLoad seam + OnWorldBeginPlay 加载模式。
//
// 非 vacuous：GetTilesInGroup 若过滤/色组映射有 bug，计数会错→FAIL；
//   加载未达 Active（资产缺失/校验拒绝）则早退报错，不静默 pass。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClassic40_TC_AC6b_ColorGroupViaGetTilesInGroup,
    "Rento.Board.Classic40AssetValidation.TC_AC6b_ColorGroupViaGetTilesInGroup",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClassic40_TC_AC6b_ColorGroupViaGetTilesInGroup::RunTest(const FString& Parameters)
{
    // 先确认资产存在（与其它 test 同前提）
    if (!LoadClassic40(*this)) { return false; }

    // 创建最小 Game World + 取 host 子系统（复用 bd-004 模式）
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("TC_AC6b_World"));
    if (!TestNotNull(TEXT("应能创建 Game World"), World)) { return false; }

    UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
    if (!TestNotNull(TEXT("应能取 UBoardDataAssetHost 子系统"), Host))
    {
        World->DestroyWorld(/*bInformEngineOfWorld=*/false);
        return false;
    }

    // 经 BoardToLoad seam 注入真磁盘资产 + 推进至 Active（OnWorldBeginPlay 触发加载，
    // 真磁盘资产由 FlushAsyncLoading pump，与 bd-004 transient fixture 同 seam）
    Host->BoardToLoad = TSoftObjectPtr<UBoardDataAsset>(FSoftObjectPath(GClassic40ObjectPath));
    Host->OnWorldBeginPlay(*World);
    FlushAsyncLoading();

    if (!TestEqual(TEXT("host 应推进至 Active 态（真资产加载+校验通过）"),
        static_cast<int32>(Host->GetLoadState()), static_cast<int32>(EHostLoadState::Active)))
    {
        World->DestroyWorld(/*bInformEngineOfWorld=*/false);
        return false;
    }

    // 经 GetTilesInGroup 对 8 色组计数（AC-6 规格指定方法）
    auto GroupCount = [Host](EColorGroup G) { return Host->GetTilesInGroup(G).Num(); };

    TestEqual(TEXT("棕色组（GetTilesInGroup）"),   GroupCount(EColorGroup::Brown),     2);
    TestEqual(TEXT("浅蓝色组（GetTilesInGroup）"), GroupCount(EColorGroup::LightBlue), 3);
    TestEqual(TEXT("粉色组（GetTilesInGroup）"),   GroupCount(EColorGroup::Magenta),   3);
    TestEqual(TEXT("橙色组（GetTilesInGroup）"),   GroupCount(EColorGroup::Orange),    3);
    TestEqual(TEXT("红色组（GetTilesInGroup）"),   GroupCount(EColorGroup::Red),       3);
    TestEqual(TEXT("黄色组（GetTilesInGroup）"),   GroupCount(EColorGroup::Yellow),    3);
    TestEqual(TEXT("绿色组（GetTilesInGroup）"),   GroupCount(EColorGroup::Green),     3);
    TestEqual(TEXT("深蓝色组（GetTilesInGroup）"), GroupCount(EColorGroup::DarkBlue),  2);

    const int32 Total =
        GroupCount(EColorGroup::Brown)  + GroupCount(EColorGroup::LightBlue) +
        GroupCount(EColorGroup::Magenta)+ GroupCount(EColorGroup::Orange)    +
        GroupCount(EColorGroup::Red)    + GroupCount(EColorGroup::Yellow)    +
        GroupCount(EColorGroup::Green)  + GroupCount(EColorGroup::DarkBlue);
    TestEqual(TEXT("色组成员合计（GetTilesInGroup）"), Total, 22);

    World->DestroyWorld(/*bInformEngineOfWorld=*/false);
    return true;
}

// =============================================================================
// TC-AC7 — 经典布局逐格匹配（CR-6）
//
// 逐格比对 (TileType, Property 的 ColorGroup) 与独立 oracle GClassic40Oracle，全 40 格匹配。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClassic40_TC_AC7_LayoutMatchesOracle,
    "Rento.Board.Classic40AssetValidation.TC_AC7_LayoutMatchesOracle",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClassic40_TC_AC7_LayoutMatchesOracle::RunTest(const FString& Parameters)
{
    UBoardDataAsset* Asset = LoadClassic40(*this);
    if (!Asset) { return false; }

    for (int32 Idx = 0; Idx < 40; ++Idx)
    {
        const FBoardTileData& Tile = Asset->Tiles[Idx];
        const FExpectedTile& Expected = GClassic40Oracle[Idx];

        // TileType 匹配
        TestEqual(FString::Printf(TEXT("索引 %d TileType"), Idx),
            static_cast<int32>(Tile.TileType), static_cast<int32>(Expected.Type));

        // ColorGroup 匹配（Property 格须匹配 oracle 色组；非 Property 格须为 None）
        TestEqual(FString::Printf(TEXT("索引 %d ColorGroup"), Idx),
            static_cast<int32>(Tile.ColorGroup), static_cast<int32>(Expected.Group));

        // TileIndex 与数组下标一致（顺序契约）
        TestEqual(FString::Printf(TEXT("索引 %d TileIndex 与下标一致"), Idx),
            Tile.TileIndex, Idx);
    }

    return true;
}

// =============================================================================
// TC-AC7b — Go 格 schema（CR-6）
//
// 索引 0 Go 格：SalaryAmount>0 且 SpecialAction==CollectSalary。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClassic40_TC_AC7b_GoTileSchema,
    "Rento.Board.Classic40AssetValidation.TC_AC7b_GoTileSchema",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClassic40_TC_AC7b_GoTileSchema::RunTest(const FString& Parameters)
{
    UBoardDataAsset* Asset = LoadClassic40(*this);
    if (!Asset) { return false; }

    const FBoardTileData& GoTile = Asset->Tiles[0];

    TestEqual(TEXT("索引 0 是 Go 格"),
        static_cast<int32>(GoTile.TileType), static_cast<int32>(ETileType::Go));
    TestTrue(TEXT("Go 格 SalaryAmount>0"), GoTile.SalaryAmount > 0);
    TestEqual(TEXT("Go 格 SpecialAction==CollectSalary"),
        static_cast<int32>(GoTile.SpecialAction), static_cast<int32>(ETileAction::CollectSalary));

    return true;
}
