// BoardCookGateCommandlet.cpp
// =============================================================================
// Commandlet 实现：cook 前 bIsPlaceholderData 门控检测
//
// story-007 AC-4c，ADR-0002 §Implementation Guidelines #3
// 调用：UnrealEditor-Cmd rento.uproject -run=BoardCookGate
//
// 扫描策略：
//   1. 用 AssetRegistry 枚举所有 UBoardDataAsset 类的资产（GetAssetsByClass）
//   2. 对每个资产调 LoadObject<UBoardDataAsset> 同步加载（资产小，~数KB，安全）
//   3. 读 bIsPlaceholderData（#if WITH_EDITORONLY_DATA，commandlet 以 Editor 模式运行）
//   4. 有任意 true：UE_LOG Error + 累积失败列表；遍历完毕若有失败则 return 1
//   5. 全部 false：return 0
//
// ⚠ WITH_EDITORONLY_DATA 在 Editor target（commandlet 运行环境）下恒为 1，
//    bIsPlaceholderData 字段在此环境始终可读，无需运行时分支。
//    但保留 #if 包裹作为编译期文档化，防止误在 Shipping 环境调用。
// =============================================================================

// 仅在 Editor 构建中编译（Shipping 构建剥离）
#if WITH_EDITOR

#include "Commandlets/BoardCookGateCommandlet.h"

// UE 引擎头文件
#include "AssetRegistry/AssetRegistryModule.h"  // FAssetRegistryModule / IAssetRegistry
#include "AssetRegistry/AssetData.h"            // FAssetData / FTopLevelAssetPath

// 本项目头文件
#include "BoardDataAsset.h"

// =============================================================================
// Main — Commandlet 入口
// =============================================================================
int32 UBoardCookGateCommandlet::Main(const FString& Params)
{
    UE_LOG(LogTemp, Display,
        TEXT("BoardCookGate: 开始扫描所有 UBoardDataAsset 资产的 bIsPlaceholderData 字段..."));

    // --------------------------------------------------------------------------
    // Step 1：获取 AssetRegistry，确保扫描完整（搜索所有路径）
    // --------------------------------------------------------------------------
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // 强制 AssetRegistry 完成所有后台扫描（确保获取全量资产）
    AssetRegistry.SearchAllAssets(/*bSynchronousSearch=*/true);

    // --------------------------------------------------------------------------
    // Step 2：枚举所有 UBoardDataAsset 类的资产
    //
    // GetAssetsByClass 5.7 签名：
    //   bool GetAssetsByClass(FTopLevelAssetPath ClassPathName,
    //                          TArray<FAssetData>& OutAssetData,
    //                          bool bSearchSubClasses = false) const
    //
    // FTopLevelAssetPath 由 UClass::GetClassPathName() 获取（5.7 推荐方式）
    // --------------------------------------------------------------------------
    TArray<FAssetData> BoardAssets;
    const FTopLevelAssetPath BoardAssetClassPath =
        UBoardDataAsset::StaticClass()->GetClassPathName();

    const bool bFound = AssetRegistry.GetAssetsByClass(
        BoardAssetClassPath,
        BoardAssets,
        /*bSearchSubClasses=*/false);

    if (!bFound || BoardAssets.Num() == 0)
    {
        // 未找到任何棋盘资产——可能是生成器未跑/生成失败/路径未注册/AssetRegistry 缓存 stale。
        // ⚠ Gate fail-closed（code-review F-1）：门控不得 fail-open。Rento 棋盘是必备资产，
        //   "零棋盘"永远是坏构建（生成链路断裂），若静默 return 0 会让 Alpha CI 虚通过、
        //   失败点后移到 cook 阶段、定位链路延长。故无资产=硬失败 return 1。
        //   MVP/nightly CI 本就忽略 exit 1（与 placeholder=true 同档），不会误报；
        //   Alpha CI 将 exit 1 视为硬失败，正确拦下"棋盘未生成"。
        UE_LOG(LogTemp, Error,
            TEXT("BoardCookGate: 未找到任何 UBoardDataAsset 资产（生成器未跑或生成失败）。"
                 "门控 fail-closed：返回 1。若资产未生成，请先运行 GenerateClassic40 commandlet。"));
        return 1;
    }

    UE_LOG(LogTemp, Display,
        TEXT("BoardCookGate: 共找到 %d 个 UBoardDataAsset 资产，逐一检测..."),
        BoardAssets.Num());

    // --------------------------------------------------------------------------
    // Step 3：逐个加载并检测 bIsPlaceholderData
    // --------------------------------------------------------------------------
    TArray<FString> PlaceholderAssets;  // 收集所有占位资产的路径，用于错误报告

    for (const FAssetData& AssetData : BoardAssets)
    {
        const FString AssetPathStr = AssetData.GetObjectPathString();

        // 同步加载资产（棋盘资产极小，~数KB，同步加载安全）
        // LoadObject<T> 在资产已加载时直接返回已有对象；未加载时同步从磁盘读取
        UBoardDataAsset* BoardAsset = LoadObject<UBoardDataAsset>(
            /*Outer=*/nullptr,
            *AssetPathStr);

        if (!BoardAsset)
        {
            // 加载失败——资产可能损坏或路径无效，记录警告但不因此阻断
            UE_LOG(LogTemp, Warning,
                TEXT("BoardCookGate: 无法加载资产 %s，跳过（资产损坏或路径无效）"),
                *AssetPathStr);
            continue;
        }

        // 读取 bIsPlaceholderData（WITH_EDITORONLY_DATA 在 Editor 模式下恒为 1）
#if WITH_EDITORONLY_DATA
        if (BoardAsset->bIsPlaceholderData)
        {
            // 发现占位资产——记录错误日志 + 加入失败列表
            UE_LOG(LogTemp, Error,
                TEXT("BoardCookGate: [PLACEHOLDER] 资产 %s 的 bIsPlaceholderData==true。"
                     "此资产未通过 Alpha 门控，不得进入 Alpha/Beta 构建。"
                     "请完成设计数据填写后将 bIsPlaceholderData 置 false。"),
                *AssetPathStr);
            PlaceholderAssets.Add(AssetPathStr);
        }
        else
        {
            UE_LOG(LogTemp, Display,
                TEXT("BoardCookGate: [OK] %s bIsPlaceholderData==false"),
                *AssetPathStr);
        }
#else
        // Shipping target（WITH_EDITORONLY_DATA=0）：bIsPlaceholderData 字段不存在。
        // commandlet 应在 Editor 模式下运行，不应到达此分支。
        UE_LOG(LogTemp, Warning,
            TEXT("BoardCookGate: 资产 %s 在非 Editor 环境下无法读取 bIsPlaceholderData，跳过"),
            *AssetPathStr);
#endif // WITH_EDITORONLY_DATA
    }

    // --------------------------------------------------------------------------
    // Step 4：汇总报告 + 返回退出码
    // --------------------------------------------------------------------------
    if (PlaceholderAssets.Num() > 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("BoardCookGate: 门控失败——发现 %d 个含占位数据的棋盘资产："),
            PlaceholderAssets.Num());
        for (const FString& Path : PlaceholderAssets)
        {
            UE_LOG(LogTemp, Error, TEXT("BoardCookGate:   - %s"), *Path);
        }
        UE_LOG(LogTemp, Error,
            TEXT("BoardCookGate: 退出码=1（Alpha gate 阻断）。"
                 "MVP/nightly CI 可忽略此退出码；Alpha 里程碑 CI 须将此视为硬失败。"));
        return 1;
    }

    // 全部通过
    UE_LOG(LogTemp, Display,
        TEXT("BoardCookGate: 门控通过——%d 个棋盘资产均 bIsPlaceholderData==false。退出码=0。"),
        BoardAssets.Num());
    return 0;
}

#endif // WITH_EDITOR
