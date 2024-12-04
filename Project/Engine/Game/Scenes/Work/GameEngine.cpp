#include "GameEngine.h"
#include "SceneFactory.h"


/// -------------------------------------------------------------
///				　		　　初期化処理
/// -------------------------------------------------------------
void GameEngine::Initialize()
{
	// 基底クラスの初期化処理
	Framework::Initialize();

	// SceneManager の初期化
	sceneManager_ = std::make_unique<SceneManager>();
	sceneManager_->SetNextScene(std::make_unique<TitleScene>());

	// シーンファクトリーの生成
	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetAbstractSceneFactory(std::move(sceneFactory_));

	/// ---------- シングルトンインスタンス ---------- ///
	winApp = WinApp::GetInstance();
	dxCommon = DirectXCommon::GetInstance();
	srvManager = std::make_unique<SRVManager>();
	input = Input::GetInstance();
	imguiManager = ImGuiManager::GetInstance();
	textureManager = TextureManager::GetInstance();
	modelManager = ModelManager::GetInstance();
	pipelineStateManager_ = std::make_unique<PipelineStateManager>();

	/// ---------- WindowsAPIのウィンドウ作成 ---------- ///
	winApp->CreateMainWindow(kClientWidth, kClientHeight);

	/// ---------- 入力の初期化 ---------- ///
	input->Initialize(winApp);

	/// ---------- DirectXの初期化 ----------///
	dxCommon->Initialize(winApp, kClientWidth, kClientHeight);

	/// ---------- SRVManagerの初期化 ---------- ///
	srvManager->Initialize(dxCommon);

	textureManager->Initialize(dxCommon, srvManager.get());

	/// ---------- ImGuiManagerの初期化 ---------- ///
	imguiManager->Initialize(winApp, dxCommon, srvManager.get());

	/// ---------- PipelineStateManagerの初期化 ---------- ///
	pipelineStateManager_->Initialize(dxCommon);
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void GameEngine::Update()
{
	// 基底クラスの更新処理
	Framework::Update();

	// ウィンドウメッセージ処理
	if (winApp->ProcessMessage()) // ウィンドウクローズイベントをチェック
	{
		endRequest_ = true; // 終了フラグを設定
		return;             // 必要なら早期リターン
	}

	// 入力の更新
	input->Update();

	/// ---------- ImGuiフレーム開始 ---------- ///
	imguiManager->BeginFrame();

	// シーンマネージャーの更新
	sceneManager_->Update();

#ifdef _DEBUG
	// 開発用のUIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
	ImGui::ShowDemoWindow();

	// シーンのImGuiの描画処理
	sceneManager_->DrawImGui();

#endif // _DEBUG
	/// ---------- ImGuiフレーム終了 ---------- ///
	imguiManager->EndFrame();
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void GameEngine::Draw()
{
	// 描画開始処理
	dxCommon->BeginDraw();

	// SRVの処理
	srvManager->PreDraw();

	/*-----シーン（モデル）の描画設定と描画-----*/
	pipelineStateManager_->SetGraphicsPipeline(dxCommon->GetCommandList()); // ルートシグネチャとパイプラインステートの設定

	// シーンマネージャーの描画処理
	sceneManager_->Draw();

	/*-----ImGuiの描画-----*/
	// ImGui描画のコマンドを積む
	imguiManager->Draw();

	// 描画終了処理
	dxCommon->EndDraw();
}


/// -------------------------------------------------------------
///				　			終了処理
/// -------------------------------------------------------------
void GameEngine::Finalize()
{
	winApp->Finalize();
	dxCommon->Finalize();
	imguiManager->Finalize();

	sceneManager_->Fainalize();

	// 基底クラスの終了処理
	Framework::Finalize();
}

