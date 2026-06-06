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
	}
}
