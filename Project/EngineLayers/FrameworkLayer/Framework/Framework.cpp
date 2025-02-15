#include "Framework.h"
#include <Windows.h>
#include <WinApp.h>
#include <DirectXCommon.h>
#include <SRVManager.h>
#include <TextureManager.h>
#include <ParticleManager.h>
#include <Object3DCommon.h>
#include <DebugCamera.h>
#include <Wireframe.h>


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
#pragma region ---------- ウィンドウアプリケーションの初期化処理 ----------
	// ウィンドウアプリケーションの生成
	winApp_ = WinApp::GetInstance();
	winApp_->CreateMainWindow();
#pragma endregion ---------------------------------------------------------


#pragma region ---------- 基盤システムの初期化処理 ----------
	// DirectX共通クラスの生成
	dxCommon_ = DirectXCommon::GetInstance();
	dxCommon_->Initialize(winApp_, WinApp::kClientWidth, WinApp::kClientHeight);
	
	// SRVマネージャーの生成
	SRVManager::GetInstance()->Initialize(dxCommon_);

	// テクスチャマネージャーの生成
	TextureManager::GetInstance()->Initialize(dxCommon_);
	
	// Object3DCommonの生成
	Object3DCommon::GetInstance()->Initialize(dxCommon_);

	// デバッグカメラの生成
	DebugCamera::GetInstance()->Initialize();

	// デフォルトカメラの生成と初期化
	defaultCamera_ = std::make_unique<Camera>();
	defaultCamera_->SetRotate({ 0.3f,0.0f,0.0f });
	defaultCamera_->SetTranslate({ 0.0f,15.0f,-50.0f });
	
	// デフォルトカメラの設定
	Object3DCommon::GetInstance()->SetDefaultCamera(defaultCamera_.get());

	// ImGuiManagerの生成
	ImGuiManager::GetInstance()->Initialize(winApp_, dxCommon_);

	// ワイヤーフレームのカメラ設定
	Wireframe::GetInstance()->SetCamera(defaultCamera_.get());
	
	// ワイヤーフレームの生成
	Wireframe::GetInstance()->Initialize(dxCommon_);

	// ParticleManagerの生成
	ParticleManager::GetInstance()->Initialize(dxCommon_, defaultCamera_.get());

#pragma endregion -------------------------------------------

	// シーンマネージャーの生成
	//sceneManager_ = std::make_unique<SceneManager>();
}


/// -------------------------------------------------------------
///				　		ゲーム全体の更新処理
/// -------------------------------------------------------------
void Framework::Update()
{
	// ウィンドウアプリケーションのメッセージ処理
	if (winApp_->ProcessMessage())
	{
		endRequest_ = true; // 終了リクエストを出す
		return;				// 終了リクエストが来たら抜ける
	}

	// ワイヤーフレームの更新処理
	Wireframe::GetInstance()->Update();

	// Object3DCommonの更新処理
	Object3DCommon::GetInstance()->Update();

	// ParticleManagerの更新処理
	ParticleManager::GetInstance()->Update();

	// シーンマネージャーの更新処理
	//sceneManager_->Update();
}


/// -------------------------------------------------------------
///				　		ゲーム全体の終了処理
/// -------------------------------------------------------------
void Framework::Finalize()
{
	// ウィンドウアプリケーションの終了処理
	winApp_->Finalize();

	// DirectX共通クラスの終了処理
	dxCommon_->Finalize();

#ifdef _DEBUG // デバッグモードの場合
	// ImGuiManagerの終了処理
	ImGuiManager::GetInstance()->Finalize();
#endif // _DEBUG

	// ワイヤーフレームの終了処理
	Wireframe::GetInstance()->Finalize();

	// ParticleManagerの終了処理
	ParticleManager::GetInstance()->Finalize();

	// シーンマネージャーの解放
	//sceneManager_.reset();

	// その他の終了処理

}

