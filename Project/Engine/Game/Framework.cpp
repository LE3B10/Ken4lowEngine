#include "Framework.h"
#include <Windows.h>


/// -------------------------------------------------------------
///				　		ゲーム全体の実行処理
/// -------------------------------------------------------------
void Framework::Run()
{
	// 初期化処理
	Initialize();

	// ゲームループ
	while (!IsEndRequest())// 終了リクエストが来たら抜ける
	{
		// 毎フレーム更新
		Update();

		// 描画
		Draw();
	}

	// ゲームの終了
	Finalize();
}


/// -------------------------------------------------------------
///				　　 　ゲーム全体の初期化処理
/// -------------------------------------------------------------
void Framework::Initialize()
{
	// シーンマネージャーの生成

	/// ---------- GamePlaySceneの初期化 ---------- ///
	/*scene_ = std::make_unique<TitleScene>();
	scene_->Initialize();

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
	pipelineStateManager_->Initialize(dxCommon);*/

	
}


/// -------------------------------------------------------------
///				　		ゲーム全体の更新処理
/// -------------------------------------------------------------
void Framework::Update()
{
//	// ウィンドウメッセージ処理
//	if (winApp->ProcessMessage()) // ウィンドウクローズイベントをチェック
//	{
//		endRequest_ = true; // 終了フラグを設定
//		return;             // 必要なら早期リターン
//	}
//
//	// 入力の更新
//	input->Update();
//
//	if (input->TriggerKey(DIK_0))
//	{
//		OutputDebugStringA("Hit 0\n");
//	}
//
//	/// ---------- ImGuiフレーム開始 ---------- ///
//	imguiManager->BeginFrame();
//
//#ifdef _DEBUG
//	// 開発用のUIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
//	ImGui::ShowDemoWindow();
//
//	// シーンのImGuiの描画処理
//	//scene_->DrawImGui();
//
//#endif // _DEBUG
//	/// ---------- ImGuiフレーム終了 ---------- ///
//	imguiManager->EndFrame();
//
//	// シーンの更新処理
//	scene_->Update();
}


/// -------------------------------------------------------------
///				　		ゲーム全体の描画処理
/// -------------------------------------------------------------
void Framework::Finalize()
{
	//winApp->Finalize();
	//dxCommon->Finalize();
	//imguiManager->Finalize();

	//// シーンの終了処理
	//scene_->Finalize();
}
