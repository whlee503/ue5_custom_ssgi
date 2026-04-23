// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomSSGI.h"
#include "CustomSSGIViewExtension.h" //  추가
#include "Interfaces/IPluginManager.h" // 추가
#include "ShaderCore.h"                // 추가
// 엔진 타이밍 예약을 위해 델리게이트 헤더 추가
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FCustomSSGIModule"


void FCustomSSGIModule::StartupModule() 
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	

	// 플러그인 코어 로딩 확인용 에러 로그
	UE_LOG(LogTemp, Error, TEXT("==== CustomSSGI 플러그인 로딩 완료! ===="));

	// 플러그인을 찾을 때까지 안전하게 확인합니다. (Null 크래시 방지)
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CustomSSGI"));
	if (Plugin.IsValid())
	{
		// 가상 경로와 실제 Shaders 폴더 매핑
		FString PluginShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/CustomSSGI"), PluginShaderDir);
	}

	// 기존에 작성했던 훅 등록 코드	
	// 플러그인이 로드될 때, 렌더 파이프라인에 우리의 훅을 던져서 등록합니다.
	FCoreDelegates::OnPostEngineInit.AddLambda([this]()
		{
			CustomSSGIViewExtension = FSceneViewExtensions::NewExtension<FCustomSSGIViewExtension>();
			UE_LOG(LogTemp, Warning, TEXT("==== [성공] GEngine 준비 완료! 톨게이트(ViewExtension) 정상 등록됨! ===="));
		});
}

void FCustomSSGIModule::ShutdownModule() 
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
		
	// 플러그인이 꺼질 때 훅을 회수
	CustomSSGIViewExtension.Reset();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCustomSSGIModule, CustomSSGI)