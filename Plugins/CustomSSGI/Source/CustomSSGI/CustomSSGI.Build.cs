//// Copyright Epic Games, Inc. All Rights Reserved.

//using System.IO;
//using UnrealBuildTool;

//public class CustomSSGI : ModuleRules
//{
//	public CustomSSGI(ReadOnlyTargetRules Target) : base(Target)
//	{
//		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

//        // 🔥 가장 확실하게 렌더러 Private 폴더 경로를 찾아내는 마법의 코드!
//        string RendererPrivatePath = Path.Combine(GetModuleDirectory("Renderer"), "Private");
//        PrivateIncludePaths.Add(RendererPrivatePath);

//        PublicIncludePaths.AddRange(
//			new string[] {
//				// ... add public include paths required here ...
//				//System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Private")
//            }
//			);
				
		
//		PrivateIncludePaths.AddRange(
//			new string[] { 
//				// ... add other private include paths required here ...
//				//System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Private")
//            }
//			);

//        PublicDependencyModuleNames.AddRange(
//			new string[]
//			{
//				"Core",
//				// ... add other public dependencies that you statically link with here ...
//			}
//			);

//		PublicDependencyModuleNames.AddRange(    
//			new string[] 
//			{        
//				"Core",        
//				"CoreUObject",      
//				"Engine",
//				"RenderCore",		// 렌더링 파이프라인 제어를 위해 필수!
//				"RHI",				// GPU 하드웨어 자원 통제를 위해 필수!
//                "Projects",			// <-- 플러그인 경로를 찾기 위해 추가!
//                "Renderer"			// 🔥 이 모듈이 추가되어야 G-Buffer를 읽어올 수 있습니다!
//			}
//			);
			
		
//		PrivateDependencyModuleNames.AddRange(
//			new string[]
//			{
//				"CoreUObject",
//				"Engine",
//				"Slate",
//				"SlateCore",
//				// ... add private dependencies that you statically link with here ...	
//			}
//			);
		
		
//		DynamicallyLoadedModuleNames.AddRange(
//			new string[]
//			{
//				// ... add any modules that your module loads dynamically here ...
//			}
//			);
//	}
//}

// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class CustomSSGI : ModuleRules
{
    public CustomSSGI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // 🔥 가장 확실하게 렌더러 Private 폴더 경로를 찾아내는 마법의 코드! (이것만 남깁니다)
        string RendererPrivatePath = Path.Combine(GetModuleDirectory("Renderer"), "Private");
        PrivateIncludePaths.Add(RendererPrivatePath);

        // 중복된 선언들을 하나로 합치고 깔끔하게 정리했습니다.
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "RenderCore",		// 렌더링 파이프라인 제어를 위해 필수!
				"RHI",				// GPU 하드웨어 자원 통제를 위해 필수!
				"Projects",			// 플러그인 경로를 찾기 위해 추가!
				"Renderer"			// 🔥 G-Buffer 등 렌더러 코어 접근을 위해 필수!
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