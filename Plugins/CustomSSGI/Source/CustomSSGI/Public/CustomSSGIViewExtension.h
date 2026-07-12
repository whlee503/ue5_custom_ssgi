#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "SceneView.h"
#include "RenderTargetPool.h" // IPooledRenderTarget

// 엔진의 포스트 프로세스 파이프라인에 SSGI 패스를 삽입하는 커스텀 ViewExtension
class FCustomSSGIViewExtension : public FSceneViewExtensionBase
{
public:
	FCustomSSGIViewExtension(const FAutoRegister& AutoRegister);

	// 뷰포트 크기가 존재하는 진짜 화면만 통과 (에디터 전용 모듈 에러 방지)
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		return Context.Viewport && Context.Viewport->GetSizeXY().X > 0 && Context.Viewport->GetSizeXY().Y > 0;
	}

	// FSceneViewExtensionBase 필수 구현 가상 함수들
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	// 포스트 프로세싱 파이프라인 단계에서 호출될 핵심 함수
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

private:
	// 프레임 간 시간적 누적을 위한 히스토리 버퍼 (뷰별 분리).
	// 디퓨즈/스펙큘러는 시간적 특성이 달라 히스토리를 분리해서 관리한다.
	struct FViewHistory
	{
		TRefCountPtr<IPooledRenderTarget> Diffuse;
		TRefCountPtr<IPooledRenderTarget> Specular;
		uint64 LastUsedFrame = 0;
	};

	// 멀티 뷰포트(에디터/PIE 동시 사용) 시 뷰끼리 히스토리를 덮어쓰지 않도록
	// View.GetViewKey()로 구분한다. 렌더 스레드에서만 접근.
	// 값을 TUniquePtr로 힙에 두는 이유: QueueTextureExtraction이 잡아둔 포인터가
	// 같은 프레임에 다른 뷰가 항목을 추가해 TMap이 리해시되어도 유효해야 하기 때문.
	TMap<uint32, TUniquePtr<FViewHistory>> ViewHistories;
};
