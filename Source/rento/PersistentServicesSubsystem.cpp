// PersistentServicesSubsystem.cpp
// =============================================================================
// UPersistentServicesSubsystem 实现（story-004 跨局持久服务宿主框架）
//
// 本文件为 stub 框架实现。各服务实体（Save/Audio）由各自 epic 填充真实逻辑。
// 规范依据：ADR-0001、story-004 AC-1~6、control-manifest Foundation Layer §宿主与生命周期
// =============================================================================

#include "PersistentServicesSubsystem.h"
#include "Engine/GameInstance.h"

// ----------------------------------------------------------------------------
// Initialize — GameInstance 初始化时调用（进程启动 / 第一次 PIE）
// ----------------------------------------------------------------------------

void UPersistentServicesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // 调用基类（UGameInstanceSubsystem 引擎内部初始化）
    Super::Initialize(Collection);

    UE_LOG(LogTemp, Log,
        TEXT("UPersistentServicesSubsystem::Initialize — 跨局持久服务宿主初始化。"
             "GameInstance: %s"),
        GetGameInstance() ? *GetGameInstance()->GetName() : TEXT("null"));

    // §22 Audio 服务初始化（挂载点调用——Audio epic ADR-0010 在此填充真实逻辑）
    InitializeAudioService();

    // §21 Save 框架初始化挂载点（Save epic ADR-0005 在此填充：加载默认槽、建立缓存等）
    // 当前 stub：无操作，等待 Save epic。

    // §20 Setup 入口初始化挂载点（pt-001 / 回合 epic 在此填充：注册主菜单流程等）
    // 当前 stub：无操作，等待 pt-001。
}

// ----------------------------------------------------------------------------
// Deinitialize — GameInstance 销毁时调用（进程退出 / 编辑器关闭）
// ----------------------------------------------------------------------------

void UPersistentServicesSubsystem::Deinitialize()
{
    // §22 Audio 服务清理（挂载点调用——Audio epic ADR-0010 在此填充：停止音频、释放混音资源）
    DeinitializeAudioService();

    // §21 Save 框架清理挂载点（Save epic ADR-0005 在此填充：刷新存档缓存等）
    // 当前 stub：无操作，等待 Save epic。

    UE_LOG(LogTemp, Log,
        TEXT("UPersistentServicesSubsystem::Deinitialize — 跨局持久服务宿主销毁。"));

    // 调用基类（UGameInstanceSubsystem 引擎内部清理）
    Super::Deinitialize();
}

// ----------------------------------------------------------------------------
// §20 StartNewGame — Setup/主菜单入口 stub（AC-2 冻结签名）
// ----------------------------------------------------------------------------

void UPersistentServicesSubsystem::StartNewGame(const FGameSetupConfig& Config)
{
    // ⚠ 最小 seam stub — pt-001 / 回合 epic 在此填充完整实现：
    //   - 验证 Config 字段（PlayerCount、BoardAsset 等）
    //   - 通过 UGameplayStatics::OpenLevel 或 World 切换触发新局
    //   - Seed 注入由 per-match World Subsystem::OnWorldBeginPlay 完成（ADR-0001 §2）
    //
    // 当前仅记录 Log，证明入口可达（AC-2 测试验证挂载点存在 + 跨局可达）。
    UE_LOG(LogTemp, Log,
        TEXT("UPersistentServicesSubsystem::StartNewGame — 入口调用（最小 seam stub）。"
             "完整实现由 pt-001 / 回合 epic 填充。"));
}

// ----------------------------------------------------------------------------
// §21 SaveGameToSlot — 存档写入挂载点 stub（AC-3 冻结挂载位置）
// ----------------------------------------------------------------------------

bool UPersistentServicesSubsystem::SaveGameToSlot(const FString& SlotName, int32 UserIndex)
{
    // ⚠ 最小 seam stub — Save epic（ADR-0005）填充真实实现：
    //   - 序列化契约（UPROPERTY(SaveGame) 标记字段）
    //   - 四重完整性门（magic/version/checksum/manifest）
    //   - 原子写盘（临时文件 → CRC32 → IFileManager::Move）
    UE_LOG(LogTemp, Log,
        TEXT("UPersistentServicesSubsystem::SaveGameToSlot — stub 调用（SlotName=%s, UserIndex=%d）。"
             "完整实现由 Save epic ADR-0005 填充。"),
        *SlotName, UserIndex);

    // stub 恒返 false（标示未真实实现，Save epic 填充后改为真实结果）
    return false;
}

// ----------------------------------------------------------------------------
// §21 LoadGameFromSlot — 存档读取挂载点 stub（AC-3 冻结挂载位置）
// ----------------------------------------------------------------------------

bool UPersistentServicesSubsystem::LoadGameFromSlot(const FString& SlotName, int32 UserIndex)
{
    // ⚠ 最小 seam stub — Save epic（ADR-0005）填充真实实现：
    //   - 读档前置完整性门顺序（magic → version → checksum → manifest）
    //   - 读档重建拓扑序（CR-5：DA → 经济/地产 → 回合 → Seed → 重绑）
    //   - 广播 OnGameLoaded（恰一次，AC-11）
    UE_LOG(LogTemp, Log,
        TEXT("UPersistentServicesSubsystem::LoadGameFromSlot — stub 调用（SlotName=%s, UserIndex=%d）。"
             "完整实现由 Save epic ADR-0005 填充。"),
        *SlotName, UserIndex);

    // stub 恒返 false（Save epic 填充后改为真实结果）
    return false;
}

// ----------------------------------------------------------------------------
// §22 InitializeAudioService — Audio 宿主初始化挂载点 stub
// ----------------------------------------------------------------------------

void UPersistentServicesSubsystem::InitializeAudioService()
{
    // ⚠ stub — Audio epic（ADR-0010）填充真实实现：
    //   - 初始化混音总线（SC_SFX / SC_BGM / SC_Critical、SSX_SFX / SSX_BGM）
    //   - 加载 MetaSound 资产（G-1：MetaSoundSource 程序化音效）
    //   - BindAll()（G-4：集中绑定 owner delegate，在 Initialize/OnGameLoaded 调用）
    //   - 初始化 FAudioRNG（G-5：非权威流，Initialize 时以系统时间初始化）
    UE_LOG(LogTemp, Verbose,
        TEXT("UPersistentServicesSubsystem::InitializeAudioService — stub（Audio epic ADR-0010 填充）。"));
}

// ----------------------------------------------------------------------------
// §22 DeinitializeAudioService — Audio 宿主清理挂载点 stub
// ----------------------------------------------------------------------------

void UPersistentServicesSubsystem::DeinitializeAudioService()
{
    // ⚠ stub — Audio epic（ADR-0010）填充真实实现：
    //   - 停止所有活跃音频组件
    //   - UnbindAll()（G-4：集中解绑，在 Deinitialize 调用）
    //   - 释放 FAudioRNG 资源
    UE_LOG(LogTemp, Verbose,
        TEXT("UPersistentServicesSubsystem::DeinitializeAudioService — stub（Audio epic ADR-0010 填充）。"));
}
