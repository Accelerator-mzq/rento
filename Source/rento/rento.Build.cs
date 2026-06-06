// rento.Build.cs — 主游戏模块（C++ 权威逻辑 + 呈现绑定）
using UnrealBuildTool;

public class rento : ModuleRules
{
	public rento(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",     // ADR-0011 Enhanced Input（UBoardInputManager:UWorldSubsystem）
			"UMG",               // 呈现层 WBP_ widget
			"Slate",
			"SlateCore",
			"CommonUI",          // ADR-0012 CommonUI 激活栈 + 输入路由
			"CommonInput",
			"DeveloperSettings"  // DA_*Config / 项目设置（数据驱动，ADR-0002/0008/0009）
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// MetaSound（ADR-0010）按需在音频系统实现时追加 "MetasoundEngine" 等依赖。
	}
}
