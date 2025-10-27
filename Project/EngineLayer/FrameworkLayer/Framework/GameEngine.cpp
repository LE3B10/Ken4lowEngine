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
	SceneManager::GetInstance()->SetNextScene(std::make_unique<GamePlayScene>());
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
		DebugCamera::GetInstance()->Update(); // デバッグカメラの更新処理
	}
	else
	{
		// デフォルトカメラの更新処理
		defaultCamera_->Update();
	}

	// シーンマネージャーの更新
	SceneManager::GetInstance()->Update();

	// ポストエフェクトの更新
	PostEffectManager::GetInstance()->Update();
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void GameEngine::Draw()
{
	// 描画開始（バックバッファのクリア）
	dxCommon_->BeginDraw();

	/// ---------- ImGuiフレーム開始 ---------- ///
	ImGuiManager::GetInstance()->BeginFrame();

#ifdef _DEBUG // デバッグモードの場合

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

#endif // _DEBUG
	/// ---------- ImGuiフレーム終了 ---------- ///
	ImGuiManager::GetInstance()->EndFrame();

	//--------------------------------------------
	// 1. オフスクリーンレンダリングの開始（3D用）
	//--------------------------------------------
	PostEffectManager::GetInstance()->BeginDraw(); // RTV/DSVの設定・Clear

	// --- 2. 3Dオブジェクトの描画 ---
	SceneManager::GetInstance()->Draw3DObjects();

	// --- パーティクル（UIエフェクトなどあれば） ---
	ParticleManager::GetInstance()->Draw();

	// --- デバッグ描画（3D用） ---
	Wireframe::GetInstance()->Draw();

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

	//--------------------------------------------
	// 6. ImGui描画
	//--------------------------------------------
	ImGuiManager::GetInstance()->Draw();

	//--------------------------------------------
	// 7. 描画終了
	//--------------------------------------------
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
