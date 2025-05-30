#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include "SkyBoxManager.h"
#include <ParameterManager.h>
#include <ParticleManager.h>
#include "Wireframe.h"
#include "AudioManager.h"

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

	ParticleManager::GetInstance()->CreateParticleGroup("Flash", "flash.png", ParticleEffectType::Flash);
	ParticleManager::GetInstance()->CreateParticleGroup("Ring", "gradationLine.png", ParticleEffectType::Ring);
	ParticleManager::GetInstance()->CreateParticleGroup("Spark", "spark.png", ParticleEffectType::Spark);
	ParticleManager::GetInstance()->CreateParticleGroup("Smoke", "smoke.png", ParticleEffectType::Smoke);
	ParticleManager::GetInstance()->CreateParticleGroup("EnergyGather", "circle2.png", ParticleEffectType::EnergyGather);
	ParticleManager::GetInstance()->CreateParticleGroup("Charge", "circle2.png", ParticleEffectType::Charge);
	ParticleManager::GetInstance()->CreateParticleGroup("Explosion", "spark.png", ParticleEffectType::Explosion);

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();
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
		ParticleManager::GetInstance()->SetDebugCamera(!ParticleManager::GetInstance()->GetDebugCamera());
		//skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
		Input::GetInstance()->SetLockCursor(isDebugCamera_);
		ShowCursor(!isDebugCamera_);// 表示・非表示も連動（オプション）
	}
#endif // _DEBUG

	// 衝突判定と応答
	CheckAllCollisions();

	// 毎フレーム更新
	if (isCharging)	chargeTimer += kDeltaTime; // 時間加算


	// マウス左クリックを押した瞬間に収束開始
	if (input_->PushMouse(0) && !isCharging) {
		isCharging = true;
		chargeTimer = 0.0f;

		// 初回のみ Emit（粒子再利用のため、1回だけ生成）
		ParticleManager::GetInstance()->Emit("Charge", {}, 500, ParticleEffectType::Charge);
	}

	if (input_->ReleaseMouse(0) && isCharging) {
		isCharging = false;

		auto& group = ParticleManager::GetInstance()->GetGroup("Charge");
		for (auto& particle : group.particles) {
			if (particle.mode == ParticleMode::Orbit) {
				particle.mode = ParticleMode::Explode;
				Vector3 tVec = particle.transform.translate_ - particle.orbitCenter;
				tVec = Vector3::Normalize(tVec);
				particle.velocity = tVec * 10.0f;
			}
		}

		// 🔥 爆発エフェクトの生成
		Vector3 explosionCenter = { 0.0f,0.0f,0.0f };
		ParticleManager::GetInstance()->Emit("Ring", explosionCenter, 10, ParticleEffectType::Ring);
		ParticleManager::GetInstance()->Emit("Spark", explosionCenter, 50, ParticleEffectType::Spark);
		ParticleManager::GetInstance()->Emit("Smoke", explosionCenter, 5, ParticleEffectType::Smoke);
		ParticleManager::GetInstance()->Emit("Explosion", explosionCenter, 50, ParticleEffectType::Explosion);
		ParticleManager::GetInstance()->Emit("Flash", explosionCenter, 30, ParticleEffectType::Flash);
	}

	if (isCharging) {
		auto& group = ParticleManager::GetInstance()->GetGroup("Charge");
		for (auto& particle : group.particles)
		{
			if (particle.mode == ParticleMode::Explode)
			{
				// 再チャージで回転軌道に戻す
				particle.mode = ParticleMode::Orbit;

				// 回転開始位置を更新（位置を軌道上に強制補正）
				particle.currentTime = 0.0f;

				// スケール・速度リセット
				particle.velocity = { 0, 0, 0 };
			}
		}
	}
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	SkyBoxManager::GetInstance()->SetRenderSetting();
	//skyBox_->Draw();

#pragma endregion


#pragma region オブジェクト3Dの描画

	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();


	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(1000.0f, 100.0f, { 0.25f, 0.25f, 0.25f,1.0f });
#pragma endregion
}


void GamePlayScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();

#pragma endregion
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
	AudioManager::GetInstance()->StopBGM();
}


/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
	// ライト
	LightManager::GetInstance()->DrawImGui();
}


/// -------------------------------------------------------------
///				　			衝突判定と応答
/// -------------------------------------------------------------
void GamePlayScene::CheckAllCollisions()
{
	// 衝突マネージャのリセット
	collisionManager_->Reset();

	// コライダーをリストに登録
	// collisionManager_->AddCollider();

	// 複数について

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}
