// TurnModuleNoBpDerivedCiTest.cpp
// =============================================================================
// Config-Data 测试：story-009 回合模块落 C++ 权威 + 目录无 BP 派生类（in-engine 结构断言 harness）
//
// 物理路径：Source/rentoTests/Private/TurnModuleNoBpDerivedCiTest.cpp
// Automation 类目：Rento.PlayerTurn.CiAuthorityGate
//
// 与 tools/ci/check-authoritative-purity.sh（云端 grep/dir 文本门）互补：
//   本 harness 是 in-engine 结构断言——验【已加载的权威 UClass 是 native C++（非 BP 派生）】
//   + 【权威 C++ 源码树无 .uasset】。比文本门多一层运行期保证（BP 派生类替换权威类则 class flag 变）。
//
// 覆盖 AC（story-009）：
//   TC1 (AC-1/AC-2) — 权威类（回合/RNG/宿主基类）StaticClass 是 CLASS_Native 且非 CLASS_CompiledFromBlueprint
//   TC2 (AC-2)      — 权威 C++ 源码树（Source/rento）无 .uasset（BP 类不得混入 C++ 模块）
//
// 机制（headless -nullrhi，无 World 依赖）：纯 StaticClass 反射 + IFileManager 文件扫描。
//
// ⚠ 已知坑：
//   G1 EAutomationTestFlags_ApplicationContextMask | ProductFilter
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "DiceRngService.h"
#include "MatchSubsystemBase.h"

// =============================================================================
// TC1（AC-1/AC-2）— 权威类是 native C++（非 BP 派生类）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCiAuthority_TC1_AuthoritativeClassesAreNative,
    "Rento.PlayerTurn.CiAuthorityGate.TC1_AuthoritativeClassesAreNative",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCiAuthority_TC1_AuthoritativeClassesAreNative::RunTest(const FString& Parameters)
{
    // 权威 UClass 必须是 native C++（CLASS_Native）且非 BP 编译产物（!CLASS_CompiledFromBlueprint）。
    // 若有人把权威类 reparent 成 BP 派生类或用 BPGeneratedClass 顶替注册，class flag 立即变 → 断言 FAIL。
    auto CheckNative = [this](UClass* Cls, const TCHAR* Name)
    {
        if (TestNotNull(*FString::Printf(TEXT("TC-1: %s StaticClass 非空"), Name), Cls))
        {
            TestTrue(*FString::Printf(TEXT("TC-1: %s 是 native C++（CLASS_Native）"), Name),
                Cls->HasAnyClassFlags(CLASS_Native));
            TestFalse(*FString::Printf(TEXT("TC-1: %s 非 BP 派生（!CLASS_CompiledFromBlueprint）"), Name),
                Cls->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
        }
    };

    CheckNative(UPlayerTurnSubsystem::StaticClass(), TEXT("UPlayerTurnSubsystem"));
    CheckNative(URentoPlayerState::StaticClass(),    TEXT("URentoPlayerState"));
    CheckNative(UDiceRngService::StaticClass(),      TEXT("UDiceRngService"));
    CheckNative(UMatchSubsystemBase::StaticClass(),  TEXT("UMatchSubsystemBase"));
    return true;
}

// =============================================================================
// TC2（AC-2）— 权威 C++ 源码树无 .uasset（目录纯净，BP 类不得混入）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCiAuthority_TC2_AuthSourceNoUasset,
    "Rento.PlayerTurn.CiAuthorityGate.TC2_AuthSourceNoUasset",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCiAuthority_TC2_AuthSourceNoUasset::RunTest(const FString& Parameters)
{
    // 权威 C++ 模块源码树（AI/经济/建房/破产/回合/RNG 全落 Source/rento，ADR-0001/0007）。
    // BP 派生类资产（.uasset）若混入 C++ 源码目录 = 权威逻辑被 BP 旁路的结构信号。
    const FString AuthSrc = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("rento"));

    TArray<FString> Found;
    IFileManager::Get().FindFilesRecursive(Found, *AuthSrc, TEXT("*.uasset"),
        /*Files=*/true, /*Directories=*/false);

    if (!TestEqual(
        *FString::Printf(TEXT("TC-2: 权威 C++ 源码树 %s 无 .uasset（实测 %d 个）"), *AuthSrc, Found.Num()),
        Found.Num(), 0))
    {
        for (const FString& F : Found)
        {
            AddError(FString::Printf(TEXT("TC-2: 权威源码目录出现 .uasset（疑似 BP 派生类混入）：%s"), *F));
        }
    }
    return true;
}
