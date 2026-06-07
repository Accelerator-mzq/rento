// PersistentServicesSubsystem.h
// =============================================================================
// UPersistentServicesSubsystem — 跨局持久服务宿主框架（story-004）
//
// 挂载：UGameInstanceSubsystem（进程内单例，跨局 PIE Start/Stop 存活）。
//
// 职责（ADR-0001 Decision 表 + control-manifest Foundation Layer）：
//   - Save/Load 框架（21）挂载点
//   - Audio（22）资产/混音宿主挂载点
//   - Setup/主菜单（20）入口挂载点，含 StartNewGame(const FGameSetupConfig&)
//   - 跨局资产缓存语义注释（OQ-6 reconcile）
//
// 关键约束（ADR-0001 Forbidden + control-manifest）：
//   - 不承载任何 per-match 对局态字段（Cash / house_count / owner map / 回合 phase）
//   - per-match 对局态全挂 UWorldSubsystem（生命周期边界 = 一局）
//   - UObject 引用成员须 TObjectPtr<T> + UPROPERTY() 防 GC
//
// 生命周期：
//   - Initialize：GameInstance 初始化时调用（进程启动/第一次 PIE）
//   - Deinitialize：GameInstance 销毁时调用（进程退出/编辑器关闭）
//   - 跨 PIE 局（Play→Stop→Play）：不销毁不重建，维持同一实例（AC-6 单例性）
//
// OQ-6 张力 reconcile（ADR-0001 §OQ-6）：
//   UAssetManager 缓存 DA_Board 底层资产跨局不释放（满足"避免反复磁盘 IO"）；
//   "当前局正在用哪块棋盘"的引用 + tile 访问服务仍挂 per-match UWorldSubsystem（story-001）。
//   即：资产缓存在 GameInstance 级，棋盘服务实例在 World 级——二者不矛盾（AC-5 注释）。
//
// Out of Scope（严守，story-004 §Out of Scope）：
//   - Save 序列化契约、四重完整性门、原子写盘 → Save epic（ADR-0005）
//   - Audio 混音总线、MetaSound 资产 → Audio epic（ADR-0010）
//   - FGameSetupConfig 完整字段定义 → pt-001 / 回合 epic
//   - per-match World Subsystem 宿主 → story-001
//
// 规范依据：
//   - ADR-0001（primary）— UObject 宿主与生命周期边界
//   - story-004 AC-1~6
//   - control-manifest Foundation Layer §宿主与生命周期（ADR-0001）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameSetupConfig.h"
#include "PersistentServicesSubsystem.generated.h"

/**
 * UPersistentServicesSubsystem — 跨局持久服务宿主
 *
 * 提供 Save/Audio/Setup 三类跨局服务的挂载点分类框架（AC-1）。
 * 作为 UGameInstanceSubsystem，随 GameInstance 生命周期存活，跨 PIE 局不重建（AC-6）。
 *
 * 访问方式（标准 Subsystem 发现，O(1) 哈希，ADR-0001 Performance Guardrail）：
 * @code
 *   UGameInstance* GI = GetGameInstance();
 *   UPersistentServicesSubsystem* Svc = GI->GetSubsystem<UPersistentServicesSubsystem>();
 * @endcode
 *
 * 或经 World：
 * @code
 *   UGameInstance* GI = GetWorld()->GetGameInstance();
 *   UPersistentServicesSubsystem* Svc = GI->GetSubsystem<UPersistentServicesSubsystem>();
 * @endcode
 */
UCLASS()
class RENTO_API UPersistentServicesSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // =========================================================================
    // 生命周期 override（UGameInstanceSubsystem）
    // =========================================================================

    /**
     * 初始化入口（GameInstance 初始化时调用，进程启动 / 第一次 PIE）。
     *
     * 本 stub 仅建立框架骨架——各服务实体（Save/Audio）由各自 epic 在此处注入初始化逻辑：
     *   - Save epic（ADR-0005）：初始化存档槽访问、加载默认槽
     *   - Audio epic（ADR-0010）：初始化混音总线、MetaSound 资产
     *
     * 约束（ADR-0001）：不在此注入 RNG Seed（Seed 注入在 per-match OnWorldBeginPlay）。
     *
     * @param Collection 用于声明对其他 Subsystem 的依赖
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * 销毁入口（GameInstance 销毁时调用，进程退出 / 编辑器关闭）。
     *
     * 各服务实体由各自 epic 在此处注入清理逻辑：
     *   - Save epic：刷新存档缓存
     *   - Audio epic：停止所有音频、释放混音资源
     */
    virtual void Deinitialize() override;

    // =========================================================================
    // §20 Setup / 主菜单 入口（AC-2 冻结签名）
    // =========================================================================

    /**
     * 开始新游戏入口（Setup/主菜单 20 分量）。
     *
     * ⚠ 最小 seam — 接口名冻结（ADR-0001 §Impl Note 3）：
     *   签名 StartNewGame(const FGameSetupConfig&) 在此冻结，body 为占位 stub。
     *   完整实现（创建 World、触发 OnWorldBeginPlay、Seed 注入等）由 pt-001 / 回合 epic 填充。
     *   pt-001 不得重声明同名入口，应在此函数上扩展实现。
     *
     * 跨局存活保证（AC-2）：本入口挂跨局 GameInstanceSubsystem，随进程存活，
     *   不随单局 World 销毁——PIE Stop 后仍可调用，不丢失入口可达性。
     *
     * @param Config 新局配置（最小 seam 占位；完整字段由 pt-001 填充，见 GameSetupConfig.h）
     */
    UFUNCTION(BlueprintCallable, Category="Setup|GameSetup",
        meta=(DisplayName="Start New Game"))
    void StartNewGame(const FGameSetupConfig& Config);

    // =========================================================================
    // §21 Save/Load 框架 挂载点（AC-3 冻结挂载位置）
    // =========================================================================

    /**
     * 存档写入挂载点（Save/Load 框架 21 分量 — AC-3）。
     *
     * ⚠ 最小 seam — 挂载点冻结：
     *   SaveGameToSlot 调用方归属在此冻结（ADR-0001 Decision 表：SaveGameToSlot 挂此宿主）。
     *   序列化契约、四重完整性门、原子写盘 → Save epic（ADR-0005）填充。
     *   本 stub 仅输出 Log 占位，Save epic 实现后替换。
     *
     * @param SlotName 存档槽名（MVP 单槽，ADR-0005 §MVP 单槽）
     * @param UserIndex 用户索引（PC Steam 单机，通常为 0）
     * @return true 表示写入成功（stub 恒返 false，Save epic 填充真实实现）
     */
    UFUNCTION(BlueprintCallable, Category="Save|SaveLoad",
        meta=(DisplayName="Save Game To Slot"))
    bool SaveGameToSlot(const FString& SlotName, int32 UserIndex);

    /**
     * 存档读取挂载点（Save/Load 框架 21 分量 — AC-3）。
     *
     * ⚠ 最小 seam — 挂载点冻结：
     *   LoadGameFromSlot 调用方归属在此冻结（ADR-0001 Decision 表）。
     *   读档重建拓扑序（CR-5）、四重完整性门 → Save epic（ADR-0005）填充。
     *   本 stub 仅输出 Log 占位，Save epic 实现后替换。
     *
     * @param SlotName 存档槽名（MVP 单槽）
     * @param UserIndex 用户索引（通常为 0）
     * @return true 表示读取成功（stub 恒返 false，Save epic 填充真实实现）
     */
    UFUNCTION(BlueprintCallable, Category="Save|SaveLoad",
        meta=(DisplayName="Load Game From Slot"))
    bool LoadGameFromSlot(const FString& SlotName, int32 UserIndex);

    // =========================================================================
    // §22 Audio 宿主挂载点
    // =========================================================================

    /**
     * Audio 服务初始化挂载点（Audio 22 分量 — AC-1 挂载点分类框架）。
     *
     * ⚠ 最小 seam — 挂载点分类：
     *   音频总线、MetaSound 资产加载、BindAll() 由 Audio epic（ADR-0010）在此填充。
     *   本 stub 输出 Log 占位。
     *
     * 设计说明（ADR-0010 §G-4）：BindAll()/UnbindAll() 集中管理，
     *   在宿主 Initialize/Deinitialize/OnGameLoaded 调用——本宿主 Initialize 中调此方法。
     */
    void InitializeAudioService();

    /**
     * Audio 服务清理挂载点（Audio 22 分量 — AC-1 挂载点分类框架）。
     *
     * 停止所有音频、释放混音资源 → Audio epic（ADR-0010）填充。
     * 在 Deinitialize 中调用。
     */
    void DeinitializeAudioService();

    // =========================================================================
    // §OQ-6 跨局资产缓存访问面（AC-5 注释，实际缓存由 UAssetManager 管理）
    // =========================================================================

    /**
     * 跨局资产缓存说明（AC-5 / ADR-0001 §OQ-6 reconcile）：
     *
     * DA_Board 底层资产由 UAssetManager（GameInstance 级）缓存，跨局不释放，
     * 满足 OQ-6 "避免反复磁盘 IO" 诉求。
     *
     * 访问方式（真实 DA_Board 资产需 bd-007 后方可验证）：
     * @code
     *   UAssetManager& AM = UAssetManager::Get();
     *   // 同步加载（ADR-0002 §加载方式）：
     *   UBoardDataAsset* BoardDA = Cast<UBoardDataAsset>(
     *       AM.GetPrimaryAssetObject(FPrimaryAssetId("BoardDataAsset", "DefaultBoard")));
     * @endcode
     *
     * 注意：本宿主不直接持有 DA_Board 引用——UAssetManager 是底层缓存层，
     * "当前局正在用哪块棋盘"的引用挂 per-match UWorldSubsystem（story-001/003）。
     *
     * tech-debt：跨局缓存真实验证（AC-5 TC-5）需真 DA_Board 资产（bd-007 未做）
     *            + 跨局 PIE 加载，defer ff-007 + bd-007 实现后补测。
     */
    // （无运行时成员，AssetManager 访问通过 UAssetManager::Get() 静态入口）
};
