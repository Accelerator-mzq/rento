// rentoEditor.Target.cs — Editor target（含 rentoTests 测试模块，CI headless 跑 Automation 用）
using UnrealBuildTool;
using System.Collections.Generic;

public class rentoEditorTarget : TargetRules
{
	public rentoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		// 主模块 + 测试模块（测试只在 Editor target 编译，不进 Shipping Game）。
		ExtraModuleNames.AddRange(new string[] { "rento", "rentoTests" });
	}
}
