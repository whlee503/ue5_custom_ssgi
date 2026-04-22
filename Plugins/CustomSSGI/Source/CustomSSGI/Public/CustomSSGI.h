// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// 우리가 만든 ViewExtension 전방 선언
class FCustomSSGIViewExtension;

class FCustomSSGIModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	private:
		// 플러그인이 켜져 있는 동안 ViewExtension의 수명을 관리할 스마트 포인터
		TSharedPtr<FCustomSSGIViewExtension, ESPMode::ThreadSafe> CustomSSGIViewExtension;
};
