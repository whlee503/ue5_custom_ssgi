#include "CustomSSGIViewExtension.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"

// 셰이더 바인딩용 핵심 헤더
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"

// Inputs.SceneTextures(G-Buffer 등)를 읽기 위한 헤더
#include "SceneTextureParameters.h"

#include "SceneView.h"

DECLARE_GPU_STAT_NAMED(Stat_CustomSSGI, TEXT("Custom_SSGI Total"));

// ================== 런타임 튜닝용 콘솔 변수 (r.CustomSSGI.*) ==================
static TAutoConsoleVariable<int32> CVarCustomSSGIEnable(
	TEXT("r.CustomSSGI.Enable"), 1,
	TEXT("Enable the custom SSGI pipeline. 0: off, 1: on"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarCustomSSGIMaxRayLength(
	TEXT("r.CustomSSGI.MaxRayLength"), 2000.0f,
	TEXT("Maximum screen-space trace distance in world units (cm). Default 2000 (20 m)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarCustomSSGIMaxSteps(
	TEXT("r.CustomSSGI.MaxSteps"), 40,
	TEXT("Number of ray marching steps per pixel. Default 40."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarCustomSSGIDiffuseAlpha(
	TEXT("r.CustomSSGI.Temporal.DiffuseAlpha"), 0.0025f,
	TEXT("Temporal blend weight for diffuse GI (0..1). Lower accumulates longer. ")
	TEXT("Default 0.0025 (~400 frame convergence), viable because depth-based ")
	TEXT("history rejection keeps stale history from ghosting."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarCustomSSGIClampGamma(
	TEXT("r.CustomSSGI.Temporal.ClampGamma"), 99.0f,
	TEXT("Std-dev multiplier for temporal variance clipping of history. ")
	TEXT("With sparse 1spp GI a tight bound couples history to kernel-scale noise ")
	TEXT("splotches, so the default is intentionally very loose: history is only ")
	TEXT("reset where the neighborhood variance is ~zero (e.g. consistently no hits). ")
	TEXT("Lower to 1-2 for scenes with dynamic lighting to suppress radiance-change ")
	TEXT("ghosting that depth-based rejection cannot detect. <= 0 disables. Default 99."),
	ECVF_RenderThreadSafe);

// ================== Pass 1: 레이 마칭 (MainPS) ==================
class FCustomSSGIShaderPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCustomSSGIShaderPS);
	SHADER_USE_PARAMETER_STRUCT(FCustomSSGIShaderPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// 카메라(View) 유니폼 버퍼 (깊이/위치 재구성에 사용)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		// 엔진이 관리하는 씬 텍스처(G-Buffer) 묶음
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)

		// 현재 렌더링 중인 씬 컬러 (히트 지점 radiance 샘플링용)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MySceneColor)
		SHADER_PARAMETER_SAMPLER(SamplerState, MySceneColorSampler)

		// CVar 기반 튜닝 파라미터
		SHADER_PARAMETER(float, MaxRayLength)
		SHADER_PARAMETER(int32, MaxSteps)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FCustomSSGIShaderPS, "/Plugin/CustomSSGI/CustomSSGI.usf", "MainPS", SF_Pixel);


// ================== Pass 2: 가로 블러 (DenoiseX_PS) ==================
class FCustomSSGIDenoiseXPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCustomSSGIDenoiseXPS);
	SHADER_USE_PARAMETER_STRUCT(FCustomSSGIDenoiseXPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NoisyDiffuseTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NoisySpecularTexture)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCustomSSGIDenoiseXPS, "/Plugin/CustomSSGI/CustomSSGI.usf", "DenoiseX_PS", SF_Pixel);


// ========== Pass 3: 세로 블러 + 시간적 누적 + 합성 (DenoiseY_PS) ==========
class FCustomSSGIDenoiseYPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCustomSSGIDenoiseYPS);
	SHADER_USE_PARAMETER_STRUCT(FCustomSSGIDenoiseYPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		// CalcSceneDepth 등을 쓰기 위해 필요
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)

		// 원본 화면 (최종 합성용)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MySceneColor)
		SHADER_PARAMETER_SAMPLER(SamplerState, MySceneColorSampler)

		// Pass 1의 원시 스펙큘러 (매끈한 표면은 블러 대신 원시값 사용)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NoisySpecularTexture)

		// 이전 프레임 히스토리
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryDiffuseTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistorySpecularTexture)

		// Pass 2(가로 블러)의 결과물
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, IntermediateDiffuseTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, IntermediateSpecularTexture)

		SHADER_PARAMETER_SAMPLER(SamplerState, CommonSampler)

		// CVar 기반 튜닝 파라미터
		SHADER_PARAMETER(float, DiffuseTemporalAlpha)
		SHADER_PARAMETER(float, TemporalClampGamma)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCustomSSGIDenoiseYPS, "/Plugin/CustomSSGI/CustomSSGI.usf", "DenoiseY_PS", SF_Pixel);


// 생성자 (부모 클래스에 AutoRegister를 넘겨줘야 엔진에 정상 등록됨)
FCustomSSGIViewExtension::FCustomSSGIViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

// 렌더 패스 구독: DOF 직전 단계에 SSGI 패스 체인을 삽입한다
void FCustomSSGIViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (PassId == EPostProcessingPass::BeforeDOF)
	{
		// r.CustomSSGI.Enable 0 이면 패스를 등록하지 않고, 히스토리도 비워서
		// 재활성화 시 이전 프레임 잔상이 남지 않게 한다.
		if (CVarCustomSSGIEnable.GetValueOnRenderThread() == 0)
		{
			ViewHistories.Empty();
			return;
		}

		InOutPassCallbacks.Add(
			FAfterPassCallbackDelegate::CreateLambda(
				[this](FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
				{
					// GPU 프로파일링: stat gpu 항목 + GPU Visualizer 이벤트 그룹
					RDG_GPU_STAT_SCOPE(GraphBuilder, Stat_CustomSSGI);
					RDG_EVENT_SCOPE(GraphBuilder, "Custom_SSGI Pipeline");

					FScreenPassTexture SceneColor = Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);

					// GI 출력용 텍스처 Desc (씬 컬러와 동일 규격)
					FRDGTextureDesc Desc = SceneColor.Texture->Desc;
					Desc.Flags |= TexCreate_RenderTargetable;

					// 이 뷰의 히스토리 슬롯 (없으면 생성)
					const uint32 ViewKey = View.GetViewKey();
					TUniquePtr<FViewHistory>& HistorySlot = ViewHistories.FindOrAdd(ViewKey);
					if (!HistorySlot)
					{
						HistorySlot = MakeUnique<FViewHistory>();
					}
					FViewHistory* History = HistorySlot.Get();
					History->LastUsedFrame = GFrameCounterRenderThread;

					// 오래 사용되지 않은 뷰(닫힌 PIE 뷰포트 등)의 히스토리는 정리해서
					// GPU 메모리가 새지 않게 한다.
					for (auto It = ViewHistories.CreateIterator(); It; ++It)
					{
						if (It->Value->LastUsedFrame + 300 < GFrameCounterRenderThread)
						{
							It.RemoveCurrent();
						}
					}

					// 히스토리 버퍼 가져오기 (없거나 해상도가 바뀌었으면 검은색 더미로 리셋)
					FRDGTextureRef CurrentHistoryDiffuse = nullptr;
					if (History->Diffuse.IsValid() && History->Diffuse->GetDesc().Extent == Desc.Extent)
						CurrentHistoryDiffuse = GraphBuilder.RegisterExternalTexture(History->Diffuse);
					else
					{
						CurrentHistoryDiffuse = GraphBuilder.CreateTexture(Desc, TEXT("DummyHistoryDiffuse"));
						AddClearRenderTargetPass(GraphBuilder, CurrentHistoryDiffuse, FLinearColor::Black);
					}

					FRDGTextureRef CurrentHistorySpecular = nullptr;
					if (History->Specular.IsValid() && History->Specular->GetDesc().Extent == Desc.Extent)
						CurrentHistorySpecular = GraphBuilder.RegisterExternalTexture(History->Specular);
					else
					{
						CurrentHistorySpecular = GraphBuilder.CreateTexture(Desc, TEXT("DummyHistorySpecular"));
						AddClearRenderTargetPass(GraphBuilder, CurrentHistorySpecular, FLinearColor::Black);
					}

					// 세 패스가 공유하는 씬 텍스처 파라미터와 샘플러
					FSceneTextureShaderParameters SceneTextureParams = CreateSceneTextureShaderParameters(GraphBuilder, View, ESceneTextureSetupMode::All);
					FRHISamplerState* BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

					// ==========================================
					// Pass 1: 레이 마칭 (1spp 노이즈 GI 생성)
					// ==========================================
					FRDGTextureRef NoisyDiffuse = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_NoisyDiffuse"));
					FRDGTextureRef NoisySpecular = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_NoisySpecular"));

					FCustomSSGIShaderPS::FParameters* Pass1Params = GraphBuilder.AllocParameters<FCustomSSGIShaderPS::FParameters>();
					Pass1Params->RenderTargets[0] = FRenderTargetBinding(NoisyDiffuse, ERenderTargetLoadAction::ENoAction);
					Pass1Params->RenderTargets[1] = FRenderTargetBinding(NoisySpecular, ERenderTargetLoadAction::ENoAction);
					Pass1Params->View = View.ViewUniformBuffer;
					Pass1Params->SceneTextures = SceneTextureParams;
					Pass1Params->MySceneColor = SceneColor.Texture;
					Pass1Params->MySceneColorSampler = BilinearClampSampler;
					Pass1Params->MaxRayLength = FMath::Max(CVarCustomSSGIMaxRayLength.GetValueOnRenderThread(), 1.0f);
					Pass1Params->MaxSteps = FMath::Clamp(CVarCustomSSGIMaxSteps.GetValueOnRenderThread(), 1, 256);

					TShaderMapRef<FCustomSSGIShaderPS> PixelShader1(GetGlobalShaderMap(View.GetFeatureLevel()));
					FPixelShaderUtils::AddFullscreenPass(
						GraphBuilder,
						GetGlobalShaderMap(View.GetFeatureLevel()),
						RDG_EVENT_NAME("CustomSSGI_Pass1_RayMarch"),
						PixelShader1, Pass1Params, SceneColor.ViewRect);

					// ==========================================
					// Pass 2: 가로 방향 바이래터럴 블러
					// ==========================================
					FRDGTextureRef IntermediateDiffuse = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_IntermDiffuse"));
					FRDGTextureRef IntermediateSpecular = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_IntermSpecular"));

					FCustomSSGIDenoiseXPS::FParameters* PassXParams = GraphBuilder.AllocParameters<FCustomSSGIDenoiseXPS::FParameters>();
					PassXParams->RenderTargets[0] = FRenderTargetBinding(IntermediateDiffuse, ERenderTargetLoadAction::ENoAction);
					PassXParams->RenderTargets[1] = FRenderTargetBinding(IntermediateSpecular, ERenderTargetLoadAction::ENoAction);
					PassXParams->View = View.ViewUniformBuffer;
					PassXParams->SceneTextures = SceneTextureParams;
					PassXParams->NoisyDiffuseTexture = NoisyDiffuse;
					PassXParams->NoisySpecularTexture = NoisySpecular;

					TShaderMapRef<FCustomSSGIDenoiseXPS> PixelShaderX(GetGlobalShaderMap(View.GetFeatureLevel()));
					FPixelShaderUtils::AddFullscreenPass(
						GraphBuilder,
						GetGlobalShaderMap(View.GetFeatureLevel()),
						RDG_EVENT_NAME("CustomSSGI_Pass2_DenoiseX"),
						PixelShaderX, PassXParams, SceneColor.ViewRect);

					// ==========================================
					// Pass 3: 세로 블러 + 시간적 누적 + 최종 합성 (3-Way MRT)
					// ==========================================
					FRDGTextureRef FinalTexture = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_Final"));
					FRDGTextureRef PureDiffuse = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_PureDiffuse"));
					FRDGTextureRef PureSpecular = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_PureSpecular"));

					FCustomSSGIDenoiseYPS::FParameters* PassYParams = GraphBuilder.AllocParameters<FCustomSSGIDenoiseYPS::FParameters>();
					PassYParams->RenderTargets[0] = FRenderTargetBinding(FinalTexture, ERenderTargetLoadAction::ENoAction);
					PassYParams->RenderTargets[1] = FRenderTargetBinding(PureDiffuse, ERenderTargetLoadAction::ENoAction);
					PassYParams->RenderTargets[2] = FRenderTargetBinding(PureSpecular, ERenderTargetLoadAction::ENoAction);
					PassYParams->View = View.ViewUniformBuffer;
					PassYParams->SceneTextures = SceneTextureParams;

					PassYParams->IntermediateDiffuseTexture = IntermediateDiffuse;
					PassYParams->IntermediateSpecularTexture = IntermediateSpecular;
					PassYParams->NoisySpecularTexture = NoisySpecular;

					PassYParams->HistoryDiffuseTexture = CurrentHistoryDiffuse;
					PassYParams->HistorySpecularTexture = CurrentHistorySpecular;

					PassYParams->CommonSampler = BilinearClampSampler;
					PassYParams->MySceneColor = SceneColor.Texture;
					PassYParams->MySceneColorSampler = BilinearClampSampler;
					PassYParams->DiffuseTemporalAlpha = FMath::Clamp(CVarCustomSSGIDiffuseAlpha.GetValueOnRenderThread(), 0.0f, 1.0f);
					PassYParams->TemporalClampGamma = CVarCustomSSGIClampGamma.GetValueOnRenderThread();

					TShaderMapRef<FCustomSSGIDenoiseYPS> PixelShaderY(GetGlobalShaderMap(View.GetFeatureLevel()));
					FPixelShaderUtils::AddFullscreenPass(
						GraphBuilder,
						GetGlobalShaderMap(View.GetFeatureLevel()),
						RDG_EVENT_NAME("CustomSSGI_Pass3_DenoiseY_Temporal"),
						PixelShaderY, PassYParams, SceneColor.ViewRect);

					// 다음 프레임에 쓸 히스토리 저장 (RDG 수명 밖으로 추출)
					// History는 힙(TUniquePtr)에 있으므로 그래프 실행 시점까지 포인터가 안전하다.
					GraphBuilder.QueueTextureExtraction(PureDiffuse, &History->Diffuse);
					GraphBuilder.QueueTextureExtraction(PureSpecular, &History->Specular);

					// 디노이즈까지 완료된 최종 텍스처를 다음 포스트 프로세스 단계로 전달
					return FScreenPassTexture(FinalTexture, SceneColor.ViewRect);
				}
			)
		);
	}
}
