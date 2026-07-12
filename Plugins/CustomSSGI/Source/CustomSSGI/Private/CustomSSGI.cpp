// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomSSGI.h"
#include "CustomSSGIViewExtension.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FCustomSSGIModule"

void FCustomSSGIModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("CustomSSGI plugin loaded."));

	// 플러그인 셰이더 가상 경로(/Plugin/CustomSSGI)와 실제 Shaders 폴더 매핑
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CustomSSGI"));
	if (Plugin.IsValid())
	{
		FString PluginShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/CustomSSGI"), PluginShaderDir);
	}

	// GEngine 준비가 끝난 뒤 ViewExtension을 렌더 파이프라인에 등록
	OnPostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([this]()
		{
			CustomSSGIViewExtension = FSceneViewExtensions::NewExtension<FCustomSSGIViewExtension>();
			UE_LOG(LogTemp, Log, TEXT("CustomSSGI view extension registered."));
		});
}

void FCustomSSGIModule::ShutdownModule()
{
	// 등록해 둔 델리게이트와 ViewExtension 회수
	FCoreDelegates::OnPostEngineInit.Remove(OnPostEngineInitHandle);
	CustomSSGIViewExtension.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCustomSSGIModule, CustomSSGI)
