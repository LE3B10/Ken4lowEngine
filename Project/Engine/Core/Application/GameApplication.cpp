#define NOMINMAX
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
#include "Editor/EditorWindowManager.h"
#include "Editor/EditorModeController.h"
#endif // USE_IMGUI
#include "JsonAssets/JsonEditorWindow.h"
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

#ifdef USE_IMGUI
		// DebugビルドはEditor Mode ON、Release相当ではGame Preview Modeとして初期化する。
		EditorModeController::GetInstance()->Initialize();
#endif // USE_IMGUI

		/// ---------- 入力の初期化 ---------- ///
		Input::GetInstance()->Initialize(winApp_);

		// グローバル変数の読み込み
		ParameterManager::GetInstance()->LoadFiles();
		JsonEditorWindow::GetInstance()->Initialize();

		// シーンマネージャーの初期化
		SceneManager::GetInstance()->Initialize();

		// シーンファクトリーの生成と設定
		auto sceneFactory = std::make_unique<SceneFactory>();
		SceneManager::GetInstance()->SetAbstractSceneFactory(std::move(sceneFactory));

#ifdef _DEBUG
		// Debugビルドでは最初からDebugSceneを起動して、ゲームプレイ中もすぐ切り替えられるようにする。
		const std::string startSceneName = "DebugScene";
#else
		// Releaseビルドでは最初のシーンをTitleSceneにする。
		const std::string startSceneName = "TitleScene";
#endif
		// 最初のシーンを設定
		SceneManager::GetInstance()->ChangeScene(startSceneName);
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

#ifdef USE_IMGUI
		// F1でEditor Mode / Game Preview Modeを切り替え、Preview中はゲーム入力を優先する。
		EditorModeController::GetInstance()->Update(Input::GetInstance());
#endif // USE_IMGUI

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
		JsonEditorWindow::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime());

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

		// 描画開始（DebugはGameViewportRenderTarget経由、Releaseは後段でBackBufferへ直接描画する）
		dxCommon_->BeginDraw();

		dxCommon_->BeginShadowMapPass(); // シャドウマップパス開始

		// シャドウマップ生成用の描画処理
		SceneManager::GetInstance()->DrawShadowObjects(); // シャドウマップ生成用の描画処理

		dxCommon_->EndShadowMapPass(); // シャドウマップパス終了

#ifdef USE_IMGUI
		const bool editorModeEnabled = EditorModeController::GetInstance()->ShouldDrawEditorUi();
		if (editorModeEnabled)
		{
			/// ---------- ImGuiフレーム開始 ---------- ///
			ImGuiManager::GetInstance()->BeginFrame();

			// UE5風エディタUI土台を既存のシーン別ImGuiの前に描画する
			auto* editorWindows = EditorWindowManager::GetInstance();
			editorWindows->Draw();
			auto& editorWindowState = editorWindows->GetWindowState();
			JsonEditorWindow::GetInstance()->Draw(&editorWindowState.showJsonAssetManager);

			// WindowメニューのDisplay表示フラグをWinApp側の×ボタン状態と共有する
			winApp_->DrawDisplaySettingsImGui(&editorWindowState.showDisplay);

			// WindowメニューのParameters表示フラグをParameterManager側の×ボタン状態と共有する
			ParameterManager::GetInstance()->Update(&editorWindowState.showParameters);

			//defaultCamera_->DrawImGui();

			// ImGuiを描画
			Object3DCommon::GetInstance()->DrawImGui();

			// シーンのImGuiの描画処理
			SceneManager::GetInstance()->DrawImGui();

			// ParticleManagerのImGuiの描画処理
			ParticleManager::GetInstance()->DrawImGui();

			// WindowメニューのPost Effect Settings表示フラグをPostEffectManager側の×ボタン状態と共有する
			PostEffectManager::GetInstance()->ImGuiRender(&editorWindowState.showPostEffectSettings);

			/// ---------- ImGuiフレーム終了 ---------- ///
			ImGuiManager::GetInstance()->EndFrame();

			// Debug/Editor時もゲーム内部解像度は固定し、Main Viewport側の表示だけを拡縮する。
			(void)EditorWindowManager::GetInstance()->GetMainViewportSize();

			//--------------------------------------------
			// 1. オフスクリーンレンダリング（3D + Particle）
			//--------------------------------------------
			DrawGameWorldToSceneTarget();

			//--------------------------------------------
			// 4. ポストエフェクト適用（3Dの最終結果をGameRenderTargetへ集約）
			//--------------------------------------------
			PostEffectManager::GetInstance()->RenderPostEffect(); // BackBufferではなくMain Viewport用GameRenderTargetへ描画

			//--------------------------------------------
			// 5. 2Dスプライト（UIなど）をGameRenderTarget上に直接描画
			//--------------------------------------------
			PostEffectManager::GetInstance()->BeginGameRenderTargetOverlay(); // 2DをMain Viewportに含めるためGameRenderTargetをRTVへ戻す
			// Debug/ReleaseでGamePlaySceneのHUD/UI/Sprite/Font描画を必ず同じ入口から呼ぶ。
			DrawCurrentScene2DOverlay();
			PostEffectManager::GetInstance()->EndGameRenderTargetOverlay(); // ImGui::Imageで読むためGameRenderTargetをSRVへ戻す

			//--------------------------------------------
			// 6. ImGui描画
			//--------------------------------------------
			ImGuiManager::GetInstance()->Draw();
		}
		else
		{
			// Game Preview ModeではImGuiウィンドウを生成せず、ゲーム画面だけをBackBufferへ描画する。
			DrawGameWorldToSceneTarget();
			ApplyPostEffectToBackBuffer();
			dxCommon_->RebindBackBufferForGameOverlay();
			DrawGameUIToBackBuffer();
		}
#else
		// Release/Gameでも中間SceneRenderTargetを使い、ParticleとPostEffectの入力をDebugと揃える。
		DrawGameWorldToSceneTarget();

		// PostEffect後の結果だけをBackBufferへ出し、HUD/UIを後段で重ねられる状態にする。
		ApplyPostEffectToBackBuffer();

		// GPU Particle/PostEffectでRTVが切り替わった後、HUD/UI/Sprite/FontだけはBackBufferへ戻して重ねる。
		dxCommon_->RebindBackBufferForGameOverlay();

		// HUD/UIはPostEffect後のBackBufferへ描画し、画面効果に巻き込まない。
		DrawGameUIToBackBuffer();
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
	///						ゲーム本編3D描画の共通処理
	/// -------------------------------------------------------------
	void GameApplication::DrawCurrentScene3DPass()
	{
		// Scene 3D -> Debug Wireframe -> GPU Particle -> Particleの順をDebug/Releaseで固定する。
		Object3DCommon::GetInstance()->BeginObject3DPass();
		SceneManager::GetInstance()->Draw3DObjects();

		Wireframe::GetInstance()->Draw();
		GpuParticleManager::GetInstance()->Draw();
		ParticleManager::GetInstance()->Draw();
	}

	/// -------------------------------------------------------------
	///						HUD/UI/Sprite描画の共通処理
	/// -------------------------------------------------------------
	void GameApplication::DrawCurrentScene2DOverlay()
	{
		// GamePlayScene::Draw2DSprites() 内で HUDManager/Reticle/Ammo/HP/Fade まで描画する。
		SceneManager::GetInstance()->Draw2DSprites();
	}

	void GameApplication::DrawGameWorldToSceneTarget()
	{
		// Debug/ReleaseともSceneRenderTargetへ3D World + Particleを描画し、PostEffect入力を必ず作る。
		PostEffectManager::GetInstance()->BeginDraw();
		DrawCurrentScene3DPass();
		PostEffectManager::GetInstance()->EndDraw();
	}

	void GameApplication::ApplyPostEffectToBackBuffer()
	{
		// SceneRenderTargetを入力にしたPostEffect結果をBackBufferへ出力する。
		PostEffectManager::GetInstance()->RenderPostEffectToBackBuffer();
	}

	void GameApplication::DrawGameUIToBackBuffer()
	{
		// HUD/UI/Sprite/FontはPostEffect後のBackBufferへ直接描画する。
		DrawCurrentScene2DOverlay();
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
