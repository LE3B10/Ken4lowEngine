#define NOMINMAX
#include "PhysicalScene.h"
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include <AnimationPipelineBuilder.h>
#include "SkyBoxManager.h"
#include <Wireframe.h>
#include <SceneManager.h>
#include <CollisionUtility.h>
#include "LevelLoader.h"
#include <CollisionTypeIdDef.h>
#include <DirectXCommon.h>

#include <GpuParticleManager.h>
#include <PostEffectManager.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI


void PhysicalScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	camera = Object3DCommon::GetInstance()->GetDefaultCamera();
	camera->SetTranslate({ 0.0f, 2.0f, -20.0f });
	camera->SetRotate({ 0.0f, 0.0f, 0.0f });

	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize("cube.gltf");

	// 最初はロック状態の見た目にしておく
	state_ = StageState::Locked;
	unlockTimer_ = 0.0f;
	unlockDuration_ = 1.0f;
	ApplyLockedVisual();

	//// GPUパーティクルエミッターの作成
	//GpuParticleEmitter::EmitterInfo info{};
	//info.type = GpuParticleType::Default;
	//info.billboardMode = BillboardMode::Camera; // ビルボードしない
	//info.radius = 10.0f;
	//info.loopCount = 0;        // 一回で10個
	//info.loopFrequency = 0.0f;  // 0.5秒に一回

	//unlockEmitter_ = GpuParticleManager::GetInstance()->CreateEmitter("StageUnlock", info);

	//if (unlockEmitter_)
	//{
	//	unlockEmitter_->SetPosition({ 0.0f, 1.5f, 0.0f }); // キューブの少し上あたり
	//}

	floatTimer_ = 0.0f;
	isSelected_ = true; // このシーンでは常に「選択中」という扱いでOK
}

void PhysicalScene::Update()
{
	float dt = dxCommon_->GetFPSCounter().GetDeltaTime();

	Vector3 move = { 0.0f,0.0f,0.0f };

	// カメラの移動
	if (input_->PushKey(DIK_W)) move.z += 0.2f;
	if (input_->PushKey(DIK_S)) move.z -= 0.2f;
	if (input_->PushKey(DIK_A)) move.x -= 0.2f;
	if (input_->PushKey(DIK_D)) move.x += 0.2f;

	if (input_->PushKey(DIK_Q)) move.y += 0.2f;
	if (input_->PushKey(DIK_E)) move.y -= 0.2f;

	Vector3 position = camera->GetTranslate();
	position += move;

	camera->SetTranslate(position);
	camera->Update();

	bool canFloat =
		isSelected_ &&
		(state_ == StageState::Available || state_ == StageState::Cleared);

	if (canFloat)
	{
		floatTimer_ += dt;

		const float amplitude = 0.3f;   // 上下の幅
		const float speed = 0.5f;   // 1秒あたりの上下サイクル数

		float offsetY = std::sinf(floatTimer_ * speed * 2.0f * std::numbers::pi_v<float>) * amplitude;

		Vector3 pos = baseTranslate_;
		pos.y += offsetY;

		object3D_->SetTranslate(pos);
	}
	else
	{
		// ロック中・アンロック演出中・非選択のときは常に基準位置
		object3D_->SetTranslate(baseTranslate_);
	}

	// 1キー: ロック状態
	if (input_->TriggerKey(DIK_1))
	{
		state_ = StageState::Locked;
		ApplyLockedVisual();
	}

	// 2キー: 未クリア状態（プレイ可能）
	if (input_->TriggerKey(DIK_2))
	{
		state_ = StageState::Available;
		ApplyAvailableVisual();
	}

	// SPACE: Locked → Unlocking（解放演出スタート）
	if (input_->TriggerKey(DIK_SPACE) && state_ == StageState::Locked)
	{
		StartUnlock();
	}

	// Unlocking 中は演出進行
	if (state_ == StageState::Unlocking)
	{
		UpdateUnlock(dt);
	}

	// エミッタ位置をキューブに追従させたい場合
	if (unlockEmitter_)
	{
		Vector3 p = object3D_->GetTranslate();
		p.y += 1.5f;
		unlockEmitter_->SetPosition(p);
	}

	if (state_ == StageState::Cleared)
	{
		const float slowSpinSpeed = std::numbers::pi_v<float> *0.125f; // 0.25回転/秒くらい

		Vector3 rot = object3D_->GetRotate();
		rot.y += slowSpinSpeed * dt;
		object3D_->SetRotate(rot);
	}

	// BackSpace でタイトルに戻る（デバッグ用）
	if (input_->TriggerKey(DIK_BACK))
	{
		// タイトル側の Initialize で PixelateEffect をOFFにしているので、
		// ここではシーン切り替えだけでOK
		SceneManager::GetInstance()->ChangeScene("TitleScene");
		return; // このフレームの後続処理はスキップ
	}

	object3D_->Update();
}

void PhysicalScene::Draw3DObjects()
{
	object3D_->Draw();
}

void PhysicalScene::Draw2DSprites()
{
	SpriteManager::GetInstance()->SetRenderSetting_Background();

	// 2Dスプライトの描画処理をここに追加

	SpriteManager::GetInstance()->SetRenderSetting_UI();


}

void PhysicalScene::Finalize()
{

}

void PhysicalScene::DrawImGui()
{
#ifdef USE_IMGUI
	camera->DrawImGui();

	object3D_->DrawImGui();
#endif // USE_IMGUI
}

void PhysicalScene::ApplyLockedVisual()
{
	object3D_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	object3D_->SetDissolveThreshold(1.0f);
	object3D_->SetDissolveEdgeThickness(0.0f);
	object3D_->SetDissolveEdgeColor({ 1.0f, 1.0f, 1.0f, 1.0f });

	// ロック中は小さめ
	object3D_->SetScale({ 1.0f, 1.0f, 1.0f });
	object3D_->SetRotate({ 0.0f, 0.0f, 0.0f });
}

void PhysicalScene::ApplyAvailableVisual()
{
	// まだクリアしてないけど選べるステージ：少し暗めの色
	object3D_->SetColor({ 0.3f, 0.3f, 0.4f, 1.0f });
	object3D_->SetDissolveThreshold(1.0f);
	object3D_->SetDissolveEdgeThickness(0.0f);
}

void PhysicalScene::ApplyClearedVisual()
{
	object3D_->SetColor({ 0.2f, 0.7f, 1.0f, 1.0f });
	object3D_->SetDissolveThreshold(1.0f);
	object3D_->SetDissolveEdgeThickness(0.05f);
	object3D_->SetDissolveEdgeColor({ 1.0f, 0.9f, 0.4f, 1.0f });

	// 解放後はドンと大きく
	object3D_->SetScale({ 3.0f, 3.0f, 3.0f });
}

void PhysicalScene::StartUnlock()
{
	state_ = StageState::Unlocking;
	unlockTimer_ = 0.0f;

	// 解放の瞬間にパーティクルをバースト
	if (unlockEmitter_)
	{
		// 50〜150はお好みで調整
		GpuParticleManager::GetInstance()->BurstEmitter("StageUnlock", 80);
	}
}

void PhysicalScene::UpdateUnlock(float deltaTime)
{
	unlockTimer_ += deltaTime;

	float t = unlockTimer_ / unlockDuration_;
	if (t > 1.0f) t = 1.0f;

	// --- カラー：黒 → クリア済みカラー ---
	Vector4 lockedCol = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector4 clearedCol = { 0.2f, 0.7f, 1.0f, 1.0f };

	Vector4 col;
	col.x = lockedCol.x + (clearedCol.x - lockedCol.x) * t;
	col.y = lockedCol.y + (clearedCol.y - lockedCol.y) * t;
	col.z = lockedCol.z + (clearedCol.z - lockedCol.z) * t;
	col.w = 1.0f;
	object3D_->SetColor(col);

	// --- スケール：小さい → 大きい（＋ちょいバウンド） ---
	const float startScale = 1.0f;   // ロック中サイズ
	const float endScale = 3.0f;   // 解放後サイズ

	float baseScale = startScale + (endScale - startScale) * t;
	float bounce = 1.0f + 0.2f * sinf(t * std::numbers::pi_v<float>);
	float s = baseScale * bounce;

	object3D_->SetScale({ s, s, s });

	// --- 回転：高速スピンして「パーン」 ---
	const float spinSpeed = std::numbers::pi_v<float> *10.0f; // 5回転/秒くらい
	float spinFactor = 1.0f - t * 0.7f;                        // 終わりに向かって減速
	spinFactor = std::max(spinFactor, 0.2f);

	Vector3 rot = object3D_->GetRotate();
	rot.y += spinSpeed * spinFactor * deltaTime;
	object3D_->SetRotate(rot);

	// --- ディゾルブエッジ強調 ---
	object3D_->SetDissolveThreshold(1.0f);
	object3D_->SetDissolveEdgeThickness(0.1f * t);
	object3D_->SetDissolveEdgeColor({ 1.0f, 0.9f, 0.4f, 1.0f });

	// 演出が終わったら Cleared 状態へ確定
	if (unlockTimer_ >= unlockDuration_)
	{
		state_ = StageState::Cleared;
		ApplyClearedVisual();
	}
}
