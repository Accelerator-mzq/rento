// RentoGameModeBase.h — 默认 GameMode（脚手架锚点；对局宿主/Subsystem 在 foundation-framework epic 实现时挂载）
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RentoGameModeBase.generated.h"

/**
 * 骰子大亨默认 GameMode。
 * MVP 阶段为空壳；per-match 服务（回合/经济/棋盘/RNG）按 ADR-0001 挂 UWorldSubsystem，
 * 不挂在 GameMode 上（GameMode 不持游戏状态语义）。
 */
UCLASS()
class RENTO_API ARentoGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
};
