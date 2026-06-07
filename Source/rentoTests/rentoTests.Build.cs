// rentoTests.Build.cs — UE Automation 测试模块（Editor target 专用，不进 Shipping）
using UnrealBuildTool;

public class rentoTests : ModuleRules
{
	public rentoTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"rento"   // 被测主模块（economy/dice/board/turn 公式与逻辑）
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AutomationController",
			"FunctionalTesting"
		});

		// PIE 自动化（ff-007 spike）需驱动编辑器 PIE：
		//   FStartPIECommand/FEndPlayMapCommand/FAutomationEditorCommonUtils 住 UnrealEd 模块，
		//   FStartPIECommand 内部 GetFirstActiveViewport() 住 LevelEditor 模块。
		// 仅 editor target 编译（本测试模块本就只挂 rentoEditor.Target，不进 Game/Shipping）；
		// 用 bBuildEditor 门控自我说明 + 防未来误并入 Game target。
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"LevelEditor"
			});
		}
	}
}
