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
		// Framework 側でウィンドウ、DirectX、共通描画マネージャを先に初期化する。
		Framework::Initialize();

#ifdef USE_IMGUI
		// DebugビルドはEditor Mode ON、Release相当ではGame Preview Modeとして初期化する。
		EditorModeController::GetInstance()->Initialize();
#endif // USE_IMGUI

		/// ---------- 入力の初期化 ---------- ///
		// WinApp のウィンドウハンドルを使って、キーボード・マウス・ゲームパッド入力を受け取れる状態にする。
		Input::GetInstance()->Initialize(winApp_);

		// JSON で保存されたグローバルパラメータを読み込み、起動直後から調整値を反映できるようにする。
		ParameterManager::GetInstance()->LoadFiles();

		// JSON アセット確認用のエディタウィンドウを初期化する。
		JsonEditorWindow::GetInstance()->Initialize();

		// シーンマネージャーの初期化
		SceneManager::GetInstance()->Initialize();

		// 文字列のシーン名から実際の Scene インスタンスを作れるよう、Factory を SceneManager へ登録する。
		auto sceneFactory = std::make_unique<SceneFactory>();
		SceneManager::GetInstance()->SetAbstractSceneFactory(std::move(sceneFactory));

#ifdef _DEBUG
		// Debugビルドでは最初からDebugSceneを起動して、ゲームプレイ中もすぐ切り替えられるようにする。
		const std::string startSceneName = "DebugScene";
#else
		// Releaseビルドでは最初のシーンをTitleSceneにする。
		const std::string startSceneName = "TitleScene";
#endif
		// 起動直後に表示するシーンを SceneManager へ依頼する。
		SceneManager::GetInstance()->ChangeScene(startSceneName);
	}

	/// -------------------------------------------------------------
	///				　			更新処理
	/// -------------------------------------------------------------
	void GameApplication::Update()
	{
		// フレーム開始時刻を記録し、DeltaTime や各フェーズ計測の基準を作る。
		GameTimer::GetInstance()->BeginFrame();

		// Update フェーズの処理時間を計測する。
		GameTimer::GetInstance()->BeginUpdate();

		// 前フレームとの差分を取れるよう、ゲーム処理より前に入力状態を更新する。
		Input::GetInstance()->Update();

#ifdef USE_IMGUI
		// F1でEditor Mode / Game Preview Modeを切り替え、Preview中はゲーム入力を優先する。
		EditorModeController::GetInstance()->Update(Input::GetInstance());
#endif // USE_IMGUI

		// FPSカメラを使っていない場面でも、メインカメラの行列を最新状態にしておく。
		if (defaultCamera_)
		{
			defaultCamera_->Update();
		}

		// CameraManager や Particle など、ゲーム全体で共通する更新を実行する。
		Framework::Update();

		// 現在シーン固有の Update を呼び出す。
		SceneManager::GetInstance()->Update();

		// ポストエフェクトや JSON エディタなど、シーン外の補助機能を更新する。
		PostEffectManager::GetInstance()->Update();
		JsonEditorWindow::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime());

		// Update フェーズ終了後にフレーム時間を確定させる。
		GameTimer::GetInstance()->EndFrame();
	}

	/// -------------------------------------------------------------
	///				　			描画処理
	/// -------------------------------------------------------------
	void GameApplication::Draw()
	{
		// Draw フェーズの処理時間を計測する。
		GameTimer::GetInstance()->BeginDraw();

		// 描画開始（DebugはGameViewportRenderTarget経由、Releaseは後段でBackBufferへ直接描画する）
		dxCommon_->BeginDraw();

		// ライティング用の深度情報を先に作るため、シャドウマップ専用パスを開始する。
		dxCommon_->BeginShadowMapPass();

		// 影を落とす3Dオブジェクトだけを描画し、シャドウマップへ深度を書き込む。
		SceneManager::GetInstance()->DrawShadowObjects();

		// 通常描画へ戻れるよう、シャドウマップパスを終了する。
		dxCommon_->EndShadowMapPass();

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
	///				　			終了処理
	/// -------------------------------------------------------------
	void GameApplication::Finalize()
	{
		// シーンマネージャーの終了処理
		SceneManager::GetInstance()->Finalize();

		// 基底クラスの終了処理
		Framework::Finalize();
	}

	/// -------------------------------------------------------------
	///						ゲーム本編3D描画の共通処理
	/// -------------------------------------------------------------
	void GameApplication::DrawCurrentScene3DPass()
	{
		// Scene 3D -> Debug Wireframe -> GPU Particle -> CPU Particle の順を Debug / Release で固定する。
		Object3DCommon::GetInstance()->BeginObject3DPass();

		// 現在シーンが持つ通常の3Dモデルを最初に描画する。
		SceneManager::GetInstance()->Draw3DObjects();

		// デバッグ表示、GPUパーティクル、CPUパーティクルをモデル描画後に重ねる。
		Wireframe::GetInstance()->Draw();
		GpuParticleManager::GetInstance()->Draw();
		ParticleManager::GetInstance()->Draw();
	}

	/// -------------------------------------------------------------
	///						HUD/UI/Sprite描画の共通処理
	/// -------------------------------------------------------------
	void GameApplication::DrawCurrentScene2DOverlay()
	{
		// GamePlayScene::Draw2DSprites() 内で HUDManager / Reticle / Ammo / HP / Fade まで描画する。
		SceneManager::GetInstance()->Draw2DSprites();
	}

	/// -------------------------------------------------------------
	///						SceneRenderTargetへのゲーム本編描画処理
	/// -------------------------------------------------------------
	void GameApplication::DrawGameWorldToSceneTarget()
	{
		// Debug / Release とも SceneRenderTarget へ 3D World + Particle を描画し、PostEffect 入力を必ず作る。
		PostEffectManager::GetInstance()->BeginDraw();
		DrawCurrentScene3DPass();
		PostEffectManager::GetInstance()->EndDraw();
	}

	/// -------------------------------------------------------------
	///						BackBufferへのポストエフェクト反映処理
	/// -------------------------------------------------------------
	void GameApplication::ApplyPostEffectToBackBuffer()
	{
		// SceneRenderTarget を入力にした PostEffect 結果を BackBuffer へ出力する。
		PostEffectManager::GetInstance()->RenderPostEffectToBackBuffer();
	}

	/// -------------------------------------------------------------
	///						BackBufferへのUI描画処理
	/// -------------------------------------------------------------
	void GameApplication::DrawGameUIToBackBuffer()
	{
		// HUD / UI / Sprite / Font は PostEffect 後の BackBuffer へ直接描画する。
		DrawCurrentScene2DOverlay();
	}

} // namespace Ken4lowEngine
