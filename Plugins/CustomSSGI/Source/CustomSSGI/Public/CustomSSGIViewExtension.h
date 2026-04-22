#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "SceneView.h" // View
#include "RenderTargetPool.h" // IPooledRenderTarget 포함

// 엔진의 렌더링 파이프라인에 기생할 우리의 커스텀 클래스
class FCustomSSGIViewExtension : public FSceneViewExtensionBase
{
public:
	FCustomSSGIViewExtension(const FAutoRegister& AutoRegister);

	// 에디터 전용 모듈 에러 방지: 뷰포트 크기가 존재하는 진짜 화면만 통과
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		return Context.Viewport && Context.Viewport->GetSizeXY().X > 0 && Context.Viewport->GetSizeXY().Y > 0;
	}

	// FSceneViewExtensionBase 필수 구현 가상 함수들
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override 
	{
		// 뷰포트가 한 프레임을 그리기 시작할 때 가장 먼저 호출됩니다.
		//UE_LOG(LogTemp, Warning, TEXT("==== [추적 1] SetupViewFamily (렌더링 시작) ===="));
	}
	
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override 
	{
		if (InViewFamily.bRealtimeUpdate)
		{
			// 엔진이 진짜로 이 화면에 포스트 프로세싱을 할 의향이 있는지 플러그를 검사합니다
			if (InViewFamily.EngineShowFlags.PostProcessing)
			{
				//UE_LOG(LogTemp, Warning, TEXT("==== [추적 1.2] 엔진 포스트 프로세싱 플래그 ON! ===="));
			}
			else
			{
				//UE_LOG(LogTemp, Error, TEXT("==== [추적 1.2] 경고: 포스트 프로세싱 플래그가 꺼져있음! ===="));
			}
		}
	}

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	// FRHICommandListImmediate 대신 FRDGBuilder를 사용합니다
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override {}

	// 포스트 프로세스 공정 직전에 무조건 거쳐가는 '렌더 스레드' 생존 신고 함수 추가
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
		
	// 포스트 프로세싱 파이프라인 단계에서 호출될 핵심 함수 (UE5 시그니처)
	//virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

private:
	//  반드시 클래스의 멤버 변수로 선언되어야 합니다
	//TRefCountPtr<IPooledRenderTarget> GIHistoryTexture;

	//  디퓨즈(난반사)와 스펙큘러(정반사)의 역사를 따로 기록할 두 권의 책을 준비합니다.
	TRefCountPtr<IPooledRenderTarget> DiffuseHistoryTexture;
	TRefCountPtr<IPooledRenderTarget> SpecularHistoryTexture;
};
