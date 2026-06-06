// rento.Target.cs — Game target（打包/Shipping 用，不含测试模块）
using UnrealBuildTool;
using System.Collections.Generic;

public class rentoTarget : TargetRules
{
	public rentoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Latest 跨 5.x 安全，避免硬编码具体版本号（VERSION.md 知识缺口纪律）。
		// 若 5.7 编译报版本枚举错误，改为编辑器提示的具体值。
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("rento");
	}
}
