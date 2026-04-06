#include "GameApplication.h"
#include "SceneFactory.h"
#include "ParameterManager.h"
#include "ParticleManager.h"
#include <DebugCamera.h>
#include <Wireframe.h>
#include <DirectXCommon.h>
#include <CameraManager.h>
#include "Object3DCommon.h"
#include "PostEffectManager.h"
#include "LightManager.h"
#include <GpuParticleManager.h>
#include <SceneManager.h>
#include <Input.h>
#include <GameTimer.h>

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI
#include <DisplaySettings.h>
#include <WinApp.h>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				　		　　初期化処理
	/// -------------------------------------------------------------
	void GameApplication::Initialize()
	{
		// 基底クラスの初期化処理
		Framework::Initialize();

		/// ---------- 入力の初期化 ---------- ///
		Input::GetInstance()->Initialize(winApp_);

		// グローバル変数の読み込み
		ParameterManager::GetInstance()->LoadFiles();

		// シーンマネージャーの初期化
		SceneManager::GetInstance()->Initialize();

		// シーンファクトリーの生成と設定
		auto sceneFactory = std::make_unique<SceneFactory>();
		SceneManager::GetInstance()->SetAbstractSceneFactory(std::move(sceneFactory));

		// 最初のシーンを設定
		SceneManager::GetInstance()->ChangeScene("TitleScene");
	}


	/// -------------------------------------------------------------
	///				　			更新処理
	/// -------------------------------------------------------------
	void GameApplication::Update()
	{
		// 時間の更新処理
		GameTimer::GetInstance()->BeginFrame();

		// Update計測開始
		GameTimer::GetInstance()->BeginUpdate();

		// 入力の更新
		Input::GetInstance()->Update();

		// 通常カメラの更新
		// FPSカメラを使っていない場面では main camera を普通に更新
		if (defaultCamera_)
		{
			defaultCamera_->Update();
		}

		// 共通更新
		Framework::Update();

		// シーンマネージャーの更新
		SceneManager::GetInstance()->Update();

		// ポストエフェクトの更新
		PostEffectManager::GetInstance()->Update();

		// 時間の更新処理終了
		GameTimer::GetInstance()->EndFrame();
	}


	/// -------------------------------------------------------------
	///				　			描画処理
	/// -------------------------------------------------------------
	void GameApplication::Draw()
	{
		// Deaw計測開始
		GameTimer::GetInstance()->BeginDraw();

		// 描画開始（バックバッファのクリア）
		dxCommon_->BeginDraw();

		dxCommon_->BeginShadowMapPass(); // シャドウマップパス開始

		// シャドウマップ生成用の描画処理
		SceneManager::GetInstance()->DrawShadowObjects(); // シャドウマップ生成用の描画処理

		dxCommon_->EndShadowMapPass(); // シャドウマップパス終了

#ifdef USE_IMGUI
		/// ---------- ImGuiフレーム開始 ---------- ///
		ImGuiManager::GetInstance()->BeginFrame();

		// ウィンドウ表示用の画面設定の変更
		winApp_->DrawDisplaySettingsImGui();

		// グローバル変数の更新
		ParameterManager::GetInstance()->Update();

		//defaultCamera_->DrawImGui();

		// ImGuiを描画
		Object3DCommon::GetInstance()->DrawImGui();

		// シーンのImGuiの描画処理
		SceneManager::GetInstance()->DrawImGui();

		// ParticleManagerのImGuiの描画処理
		ParticleManager::GetInstance()->DrawImGui();

		// PostEffectManagerのImGuiの描画処理
		PostEffectManager::GetInstance()->ImGuiRender();

		/// ---------- ImGuiフレーム終了 ---------- ///
		ImGuiManager::GetInstance()->EndFrame();
#endif // USE_IMGUI

		//--------------------------------------------
		// 1. オフスクリーンレンダリングの開始（3D用）
		//--------------------------------------------
		PostEffectManager::GetInstance()->BeginDraw(); // RTV/DSVの設定・Clear

		// --- デバッグ描画（3D用） ---
		Wireframe::GetInstance()->Draw();

		// --- 2. 3Dオブジェクトの描画 ---
		SceneManager::GetInstance()->Draw3DObjects();

		// --- Gpuパーティクル ---
		GpuParticleManager::GetInstance()->Draw();

		// --- パーティクル（UIエフェクトなどあれば） ---
		ParticleManager::GetInstance()->Draw();

		//--------------------------------------------
		// 3. オフスクリーン描画終了（SRVへ切り替え）
		//--------------------------------------------
		PostEffectManager::GetInstance()->EndDraw();

		//--------------------------------------------
		// 4. ポストエフェクト適用（3Dの最終結果をフルスクリーンクアッドに描画）
		//--------------------------------------------
		PostEffectManager::GetInstance()->RenderPostEffect(); // swapChainのバックバッファに描画

		//--------------------------------------------
		// 5. 2Dスプライト（UIなど）をその上に直接描画
		//--------------------------------------------
		SceneManager::GetInstance()->Draw2DSprites();

#ifdef USE_IMGUI
		//--------------------------------------------
		// 6. ImGui描画
		//--------------------------------------------
		ImGuiManager::GetInstance()->Draw();
#endif // USE_IMGUI

		// Draw計測終了
		// ※ EndDraw() の中に Present が含まれている可能性が高いので、
		//   その手前までを Draw として区切る
		GameTimer::GetInstance()->EndDraw();

		// Present計測開始
		GameTimer::GetInstance()->BeginPresent();

		//--------------------------------------------
		// 7. 描画終了
		//--------------------------------------------
		dxCommon_->EndDraw();

		// Present計測終了
		GameTimer::GetInstance()->EndPresent();

		// フレーム終了
		GameTimer::GetInstance()->EndFrame();
	}


	/// -------------------------------------------------------------
	///				　			終了処理
	/// -------------------------------------------------------------
	void GameApplication::Finalize()
	{
		// シーンマネージャーの終了処理
		SceneManager::GetInstance()->Finalize();

		// 基底クラスの終了処理
		Framework::Finalize();
	}

} // namespace Ken4lowEngine
