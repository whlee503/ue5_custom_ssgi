// #include "PostProcess/PostProcessing.h"

#include "CustomSSGIViewExtension.h"
// 우리가 람다 함수 안에서 사용한 구조체들의 설계도를 가져옵니다.
#include "ScreenPass.h" 
#include "PostProcess/PostProcessMaterialInputs.h"

// 셰이더를 다루기 위해 추가된 핵심 헤더들
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"

// Inputs.SceneTextures 구조체의 정보를 읽기 위한 헤더
#include "SceneTextureParameters.h"

#include "SceneView.h" 

DECLARE_GPU_STAT_NAMED(Stat_CustomSSGI, TEXT("Custom_SSGI Total"));

// ============= C++ 셰이더 클래스 정의 (언리얼이 .usf 파일을 이해할 수 있게 만드는 껍데기) ===========
class FCustomSSGIShaderPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCustomSSGIShaderPS);
	SHADER_USE_PARAMETER_STRUCT(FCustomSSGIShaderPS, FGlobalShader);

		// RDG 파라미터 구조체: 여기서는 화면에 출력할 '렌더 타겟' 슬롯만 뚫어둡니다.
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// 카메라(View) 구조체 단 하나만 받습니다! (이 안에 Depth가 이미 다 들어있습니다)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
		//SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FViewUniformShaderParameters, View)
		
		// 추가: 현재 렌더링 중인 '진짜 화면(Scene Color)'을 직접 받는 슬롯!
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MySceneColor)
		SHADER_PARAMETER_SAMPLER(SamplerState, MySceneColorSampler)

		// 언리얼이 주는 G-Buffer 패키지를 그대로 받는 슬롯
		RENDER_TARGET_BINDING_SLOTS()

	END_SHADER_PARAMETER_STRUCT()
};


// 매크로를 통해 C++ 클래스와 아까 만든 CustomSSGI.usf 파일의 MainPS 함수를 연결
IMPLEMENT_GLOBAL_SHADER(FCustomSSGIShaderPS, "/Plugin/CustomSSGI/CustomSSGI.usf", "MainPS", SF_Pixel);
// =========================================================================



// ===================== 두 번째 작업장: 가로 블러 =======================
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



// ================ 세 번째 작업장: 세로 블러, 합성하는 셰이더 ============================
class FCustomSSGIDenoiseYPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCustomSSGIDenoiseYPS);
	SHADER_USE_PARAMETER_STRUCT(FCustomSSGIDenoiseYPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		// CalcSceneDepth를 쓰려면 반드시 필요합니다.
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)

		//SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NoisyInputTexture)
		//SHADER_PARAMETER_SAMPLER(SamplerState, NoisyInputSampler)
		
		// 원본 화면(합성 및 디버깅용)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MySceneColor)
		SHADER_PARAMETER_SAMPLER(SamplerState, MySceneColorSampler)

		//SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NoisyDiffuseTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NoisySpecularTexture)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryDiffuseTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistorySpecularTexture)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, IntermediateDiffuseTexture) // X의 결과물
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, IntermediateSpecularTexture) // X의 결과물


		// FCustomSSGIDenoisePS 구조체 안에 추가
		//SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, CommonSampler)

		//SHADER_PARAMETER_SAMPLER(SamplerState, HistorySampler)

		// 모션 벡터 파라미터 슬롯
		//SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VelocityTexture)
		//SHADER_PARAMETER_SAMPLER(SamplerState, VelocitySampler)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCustomSSGIDenoiseYPS, "/Plugin/CustomSSGI/CustomSSGI.usf", "DenoiseY_PS", SF_Pixel);



// 생성자 (부모 클래스에 AutoRegister를 넘겨줘야 엔진에 정상 등록됩니다)
FCustomSSGIViewExtension::FCustomSSGIViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

// 렌더 스레드 진입 확인 (포스트 프로세스 직전)
void FCustomSSGIViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	//UE_LOG(LogTemp, Error, TEXT("==== [추적 1.5] 렌더 스레드 진입 성공! 포스트 프로세싱 대기 중 ===="));
}

// 렌더 패스 구독 함수
void FCustomSSGIViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) 
{
	// 수많은 렌더링 패스 중, '톤매핑(Tonemap)' 단계 직전에만 개입하도록 조건을 겁니다.
	//UE_LOG(LogTemp, Warning, TEXT("==== [추적 2] 패스 검사 중... 현재 PassId: %d ===="), (int32)PassId);

	if (PassId == EPostProcessingPass::BeforeDOF)
	{
		//UE_LOG(LogTemp, Warning, TEXT("==== [추적 3] Tonemap 패스 발견! 람다 예약 완료 ===="));

		InOutPassCallbacks.Add(
			FAfterPassCallbackDelegate::CreateLambda(
				[this](FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
				{
					//UE_LOG(LogTemp, Error, TEXT("==== [추적 4] 람다(First Pixel) 실행 중!!! 화면을 붉게 칠합니다 ===="));
					

					// ==================================== GPU 프로파일링 스코프 (이 괄호 안의 모든 연산이 여기에 묶임) ================================
					// 1. stat gpu 에 "Custom SSGI Total" 항목으로 띄우기
					SCOPED_GPU_STAT(GraphBuilder.RHICmdList, Stat_CustomSSGI);

					// 2. Ctrl+Shift+, (GPU Visualizer) 에 "My SSGI Pipeline" 폴더 만들기
					RDG_EVENT_SCOPE(GraphBuilder, "Custom_SSGI Pipeline");
					// =========================================================================


					FScreenPassTexture SceneColor = Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);

					// 도화지(텍스처) 생성
					FRDGTextureDesc Desc = SceneColor.Texture->Desc;
					Desc.Flags |= TexCreate_RenderTargetable; // 렌더 타겟 권한 확실히 부여


					// (History) 가져오기
					FRDGTextureRef CurrentHistoryDiffuse = nullptr;
					if (this->DiffuseHistoryTexture.IsValid() && this->DiffuseHistoryTexture->GetDesc().Extent == Desc.Extent)
						CurrentHistoryDiffuse = GraphBuilder.RegisterExternalTexture(this->DiffuseHistoryTexture);
					else
					{
						CurrentHistoryDiffuse = GraphBuilder.CreateTexture(Desc, TEXT("DummyHistoryDiffuse"));
						AddClearRenderTargetPass(GraphBuilder, CurrentHistoryDiffuse, FLinearColor::Black);
					}

					FRDGTextureRef CurrentHistorySpecular = nullptr;
					if (this->SpecularHistoryTexture.IsValid() && this->SpecularHistoryTexture->GetDesc().Extent == Desc.Extent)
						CurrentHistorySpecular = GraphBuilder.RegisterExternalTexture(this->SpecularHistoryTexture);
					else
					{
						CurrentHistorySpecular = GraphBuilder.CreateTexture(Desc, TEXT("DummyHistorySpecular"));
						AddClearRenderTargetPass(GraphBuilder, CurrentHistorySpecular, FLinearColor::Black);
					}

					
					
					// ==========================================
					// 1. 첫 번째 패스 실행 (Ray Marching)
					// ==========================================

					FCustomSSGIShaderPS::FParameters* Pass1Params = GraphBuilder.AllocParameters<FCustomSSGIShaderPS::FParameters>();

					// 2. Pass 1 출력 텍스처 (2장)
					FRDGTextureRef NoisyDiffuse = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_NoisyDiffuse"));
					FRDGTextureRef NoisySpecular = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_NoisySpecular"));

					// 2. Pass 1 출력 텍스처 (2장)
					Pass1Params->RenderTargets[0] = FRenderTargetBinding(NoisyDiffuse, ERenderTargetLoadAction::ENoAction);
					Pass1Params->RenderTargets[1] = FRenderTargetBinding(NoisySpecular, ERenderTargetLoadAction::ENoAction);


					Pass1Params->View = View.ViewUniformBuffer;
					Pass1Params->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, View, ESceneTextureSetupMode::All);
					Pass1Params->MySceneColor = SceneColor.Texture;
					Pass1Params->MySceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

					TShaderMapRef<FCustomSSGIShaderPS> PixelShader1(GetGlobalShaderMap(View.GetFeatureLevel()));
					FPixelShaderUtils::AddFullscreenPass(
						GraphBuilder, 
						GetGlobalShaderMap(View.GetFeatureLevel()), 
						RDG_EVENT_NAME("CustomSSGI_Pass1_RayMarch"), 
						PixelShader1, Pass1Params, SceneColor.ViewRect);


					// ==========================================
					// 2. 두번째 패스 실행 (DenoiseX)
					// ==========================================
					// 중간 저장소(Intermediate) 텍스처 생성
					FRDGTextureRef IntermediateDiffuse = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_IntermDiffuse"));
					FRDGTextureRef IntermediateSpecular = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_IntermSpecular"));


					FCustomSSGIDenoiseXPS::FParameters* PassXParams = GraphBuilder.AllocParameters<FCustomSSGIDenoiseXPS::FParameters>();
					PassXParams->RenderTargets[0] = FRenderTargetBinding(IntermediateDiffuse, ERenderTargetLoadAction::ENoAction);
					PassXParams->RenderTargets[1] = FRenderTargetBinding(IntermediateSpecular, ERenderTargetLoadAction::ENoAction);

					PassXParams->View = View.ViewUniformBuffer;
					PassXParams->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, View, ESceneTextureSetupMode::All);
					PassXParams->NoisyDiffuseTexture = NoisyDiffuse; // Pass 1의 디퓨즈 결과
					PassXParams->NoisySpecularTexture = NoisySpecular; 

					TShaderMapRef<FCustomSSGIDenoiseXPS> PixelShaderX(GetGlobalShaderMap(View.GetFeatureLevel()));
					FPixelShaderUtils::AddFullscreenPass(
						GraphBuilder,
						GetGlobalShaderMap(View.GetFeatureLevel()),
						RDG_EVENT_NAME("CustomSSGI_Pass2_Denoise_X"),
						PixelShaderX, PassXParams, SceneColor.ViewRect);
					

					// ==========================================
					// 3. 세 번째 패스 실행 (DenoiseY & Split Screen)
					// ==========================================
					// 3. Pass 2 출력 텍스처 (3장)
					FRDGTextureRef FinalTexture = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_Final"));
					FRDGTextureRef PureDiffuse = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_PureDiffuse"));
					FRDGTextureRef PureSpecular = GraphBuilder.CreateTexture(Desc, TEXT("CustomSSGI_PureSpecular"));


					// 4. Pass 2 파라미터 꽂아주기
					FCustomSSGIDenoiseYPS::FParameters* PassYParams = GraphBuilder.AllocParameters<FCustomSSGIDenoiseYPS::FParameters>();


					// 5. Pass 2의 3-Way MRT 타겟 연결
					PassYParams->RenderTargets[0] = FRenderTargetBinding(FinalTexture, ERenderTargetLoadAction::ENoAction);
					PassYParams->RenderTargets[1] = FRenderTargetBinding(PureDiffuse, ERenderTargetLoadAction::ENoAction);
					PassYParams->RenderTargets[2] = FRenderTargetBinding(PureSpecular, ERenderTargetLoadAction::ENoAction);

					PassYParams->View = View.ViewUniformBuffer;
					// 🔥 추가된 부분! 방금 뚫어놓은 슬롯에 엔진의 씬 텍스처들을 밀어 넣습니다.
					PassYParams->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, View, ESceneTextureSetupMode::All);
					

					// 🔥 텍스처 바인딩 주의!
					PassYParams->IntermediateDiffuseTexture = IntermediateDiffuse; // Pass X의 결과물
					PassYParams->IntermediateSpecularTexture = IntermediateSpecular; // Pass X의 결과물
					PassYParams->NoisySpecularTexture = NoisySpecular;             // (Raw)

					PassYParams->HistoryDiffuseTexture = CurrentHistoryDiffuse;
					PassYParams->HistorySpecularTexture = CurrentHistorySpecular;

					PassYParams->CommonSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

					// 엔진의 SceneTextures 묶음 안에서 '속도 버퍼(Velocity)'만
					//Pass2Params->VelocityTexture = Pass2Params->SceneTextures.GBufferVelocityTexture;
					//Pass2Params->VelocitySampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

					PassYParams->MySceneColor = SceneColor.Texture;
					PassYParams->MySceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();


					TShaderMapRef<FCustomSSGIDenoiseYPS> PixelShaderY(GetGlobalShaderMap(View.GetFeatureLevel()));
					FPixelShaderUtils::AddFullscreenPass(
						GraphBuilder, 
						GetGlobalShaderMap(View.GetFeatureLevel()), 
						RDG_EVENT_NAME("CustomSSGI_Pass2_Denoise_Y & Temporal"), 
						PixelShaderY, PassYParams, SceneColor.ViewRect);


					// 미래를 위한 역사서 저장 (Extraction 2번)
					GraphBuilder.QueueTextureExtraction(PureDiffuse, &this->DiffuseHistoryTexture);
					GraphBuilder.QueueTextureExtraction(PureSpecular, &this->SpecularHistoryTexture);

					// 최종적으로 디노이즈까지 완료된 텍스처를 화면에 출력
					return FScreenPassTexture(FinalTexture, SceneColor.ViewRect);
				}
			)
		);
	}
}