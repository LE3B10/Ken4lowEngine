#include "GameEngine.h"
#include "SceneFactory.h"
#include "ParameterManager.h"
#include "ParticleManager.h"
#include <DebugCamera.h>
#include <Wireframe.h>
#include <DirectXCommon.h>
#include "Object3DCommon.h"
#include "PostEffectManager.h"
#include "LightManager.h"


/// -------------------------------------------------------------
///				　		　　初期化処理
/// -------------------------------------------------------------
void GameEngine::Initialize()
{
	// 基底クラスの初期化処理
	Framework::Initialize();

	/// ---------- 入力の初期化 ---------- ///
	Input::GetInstance()->Initialize(winApp_);

	// グローバル変数の読み込み
	ParameterManager::GetInstance()->LoadFiles();

	// シーンファクトリーの生成と設定
	auto sceneFactory = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetAbstractSceneFactory(std::move(sceneFactory));

	// 最初のシーンを設定
	SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void GameEngine::Update()
{
	// 基底クラスの更新処理
	Framework::Update();

	// 入力の更新
	Input::GetInstance()->Update();

	if (Object3DCommon::GetInstance()->GetDebugCamera())
	{
		DebugCamera::GetInstance()->Update(); // 追加
	}
	else
	{
		// デフォルトカメラの更新処理
		defaultCamera_->Update();
	}

	/// ---------- ImGuiフレーム開始 ---------- ///
	imguiManager_->BeginFrame();

	// シーンマネージャーの更新
	SceneManager::GetInstance()->Update();

#ifdef _DEBUG // デバッグモードの場合

	// グローバル変数の更新
	ParameterManager::GetInstance()->Update();

	defaultCamera_->DrawImGui();

	// ImGuiを描画
	Object3DCommon::GetInstance()->DrawImGui();

	// シーンのImGuiの描画処理
	SceneManager::GetInstance()->DrawImGui();

	// ParticleManagerのImGuiの描画処理
	ParticleManager::GetInstance()->DrawImGui();

	// PostEffectManagerのImGuiの描画処理
	PostEffectManager::GetInstance()->ImGuiRender();

#endif // _DEBUG
	/// ---------- ImGuiフレーム終了 ---------- ///
	imguiManager_->EndFrame();
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void GameEngine::Draw()
{
	// 描画開始処理
	dxCommon_->BeginDraw();

	// オフスクリーンレンダリング開始
	PostEffectManager::GetInstance()->BeginDraw();

	// SRVの事前設定処理
	SRVManager::GetInstance()->PreDraw();

	// シーンマネージャーの描画処理
	SceneManager::GetInstance()->Draw();

	// ParticleMangerの描画処理
	ParticleManager::GetInstance()->Draw();

	// ワイヤフレームの描画処理＆リセット
	Wireframe::GetInstance()->Draw();

	// オフスクリーン描画終了
	PostEffectManager::GetInstance()->EndDraw();

	// ImGui描画開始 (ポストエフェクトの結果にオーバーレイ)
	imguiManager_->Draw();

	// 描画終了処理
	dxCommon_->EndDraw();
}


/// -------------------------------------------------------------
///				　			終了処理
/// -------------------------------------------------------------
void GameEngine::Finalize()
{
	// 基底クラスの終了処理
	Framework::Finalize();

	// シーンマネージャーの終了処理
	SceneManager::GetInstance()->Finalize();
}
