#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include "Object3DCommon.h"
#include <ParameterManager.h>
#include <ParticleManager.h>
#include "Wireframe.h"

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG


/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();
	textureManager = TextureManager::GetInstance();
	particleManager = ParticleManager::GetInstance();

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/Get-Ready.wav";
	wavLoader_ = std::make_unique<WavLoader>();
	wavLoader_->StreamAudioAsync(fileName, 0.0f, 1.0f, false);

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// プレイヤーの初期化
	player_ = std::make_unique<Player>();
	hammer_ = std::make_unique<Hammer>();

	player_->SetHammer(hammer_.get());
	player_->Initialize();
	player_->SetRadius(3.0f);

	hammer_->Initialize();

	// スカイドームの初期化
	skydome_ = std::make_unique<Skydome>();
	skydome_->Initialize();

	// 地面の初期化
	ground_ = std::make_unique<Ground>();
	ground_->Initialize();

	// 追従カメラの初期化
	followCamera_ = std::make_unique<FollowCamera>();
	followCamera_->Initialize();
	followCamera_->SetTarget(player_->GetWorldTransform());
	followCamera_->SetPlayer(player_.get());

	// 敵の初期化
	for (int i = 0; i < 5; i++)
	{
		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize();
		enemy->SetRadius(2.0f);

		// ランダムな位置と角度を設定
		float randomAngle = (static_cast<float>(rand() % 360) * std::numbers::pi_v<float>) / 180.0f; // 0～360度をラジアンに変換
		enemy->SetInitialAngle(randomAngle); // 初期角度を設定

		// 各エネミーの回転の中心をランダムに設定
		float randomCenterX = static_cast<float>(rand() % 200 - 100); // -100 ～ 100 の範囲
		float randomCenterZ = static_cast<float>(rand() % 200 - 100); // -100 ～ 100 の範囲
		enemy->SetCenterPosition({ randomCenterX, 0.0f, randomCenterZ });

		enemies_.push_back(std::move(enemy));
	}

	// ロックオンの生成と初期化処理
	lockOn_ = std::make_unique<LockOn>();
	lockOn_->Initialize();
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_F12))
	{
		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
	}
#endif // _DEBUG

	// プレイヤーの更新処理
	player_->Update();

	// スカイドームの更新処理
	skydome_->Update();

	// 地面の更新処理
	ground_->Update();

	// 追従カメラの更新処理
	followCamera_->Update();

	// 敵の更新処理
	for (auto& enemy : enemies_)
	{
		enemy->Update();
	}

	// ロックオンの更新処理
	lockOn_->Update(enemies_);

	// コリジョンマネージャの更新処理
	collisionManager_->Update();

	// 衝突判定と応答
	CheckAllCollisions();

}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw()
{



	/// ---------------------------------------- ///
	/// ---------- オブジェクト3D描画 ---------- ///
	/// ---------------------------------------- ///
	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();

	// プレイヤーの描画
	player_->Draw();

	// スカイドームの描画
	skydome_->Draw();

	// 地面の描画
	ground_->Draw();

	// 敵の描画
	for (auto& enemy : enemies_)
	{
		enemy->Draw();
	}

	// ロックオンの描画
	lockOn_->Draw();

	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 20.0f, { 0.25f, 0.25f, 0.25f,1.0f });

	// コリジョンマネージャの描画処理
	collisionManager_->Draw();


	/// ---------------------------------------- ///
	/// ---------- オブジェクト3D描画 ---------- ///
	/// ---------------------------------------- ///



}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{

}


/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{

}


/// -------------------------------------------------------------
///				　			衝突判定と応答
/// -------------------------------------------------------------
void GamePlayScene::CheckAllCollisions()
{
	// 衝突マネージャのリセット
	collisionManager_->Reset();

	// コライダーをリストに登録
	collisionManager_->AddCollider(player_.get());

	// 複数について
	for (auto& enemy : enemies_)
	{
		collisionManager_->AddCollider(enemy.get());
	}

	// ハンマーを登録
	if (player_->GetIsAttack())
	{
		collisionManager_->AddCollider(hammer_.get());
	}

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}
