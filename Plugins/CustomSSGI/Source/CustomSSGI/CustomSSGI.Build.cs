// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class CustomSSGI : ModuleRules
{
    public CustomSSGI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // 렌더러 Private 헤더 접근 (SceneTextureParameters.h 등)
        // 주의: Private 경로 의존은 엔진 마이너 버전 업그레이드 시 깨질 수 있음
        string RendererPrivatePath = Path.Combine(GetModuleDirectory("Renderer"), "Private");
        PrivateIncludePaths.Add(RendererPrivatePath);

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "RenderCore",   // RDG, 셰이더 파라미터
                "RHI",          // GPU 리소스
                "Projects",     // IPluginManager (셰이더 경로 매핑)
                "Renderer"      // G-Buffer / 씬 텍스처 접근
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore"
            }
        );
    }
}
