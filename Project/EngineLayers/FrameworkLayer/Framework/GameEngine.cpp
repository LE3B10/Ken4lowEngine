#include "GameEngine.h"
#include "SceneFactory.h"
#include "ParameterManager.h"
#include "ParticleManager.h"

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

	/// ---------- シングルトンインスタンス ---------- ///
	modelManager = ModelManager::GetInstance();

	/// ---------- PipelineStateManagerの初期化 ---------- ///
	pipelineStateManager_ = std::make_unique<PipelineStateManager>();
	pipelineStateManager_->Initialize(dxCommon_);
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

	/// ---------- ImGuiフレーム開始 ---------- ///
	imguiManager_->BeginFrame();

	// シーンマネージャーの更新
	SceneManager::GetInstance()->Update();

#ifdef _DEBUG // デバッグモードの場合

	// グローバル変数の更新
	ParameterManager::GetInstance()->Update();

	// シーンのImGuiの描画処理
	SceneManager::GetInstance()->DrawImGui();

	// ParticleManagerのImGuiの描画処理
	ParticleManager::GetInstance()->DrawImGui();

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

	// SRVの処理
	SRVManager::GetInstance()->PreDraw();

	/*-----シーン（モデル）の描画設定と描画-----*/
	pipelineStateManager_->SetGraphicsPipeline(dxCommon_->GetCommandList()); // ルートシグネチャとパイプラインステートの設定

	// シーンマネージャーの描画処理
	SceneManager::GetInstance()->Draw();

	// ParticleMangerの描画処理
	ParticleManager::GetInstance()->Draw();

	/*-----ImGuiの描画-----*/
	// ImGui描画のコマンドを積む
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
