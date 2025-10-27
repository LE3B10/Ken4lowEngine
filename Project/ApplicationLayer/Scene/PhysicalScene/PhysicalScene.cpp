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

#include <imgui.h>
#include <CollisionTypeIdDef.h>

namespace {

	// OBB → その外接AABB（8頂点を出してmin/max）
	AABB ToAABB(const OBB& obb) {
		// 8頂点（±size）
		Vector3 s = obb.size;
		Vector3 local[8] = {
			{-s.x,-s.y,-s.z}, { s.x,-s.y,-s.z}, { s.x, s.y,-s.z}, {-s.x, s.y,-s.z},
			{-s.x,-s.y, s.z}, { s.x,-s.y, s.z}, { s.x, s.y, s.z}, {-s.x, s.y, s.z},
		};
		AABB aabb;
		aabb.min = { FLT_MAX,  FLT_MAX,  FLT_MAX };
		aabb.max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

		for (int i = 0; i < 8; ++i) {
			Vector3 w =
				obb.center +
				obb.orientations[0] * local[i].x +
				obb.orientations[1] * local[i].y +
				obb.orientations[2] * local[i].z;
			aabb.min.x = std::min(aabb.min.x, w.x);
			aabb.min.y = std::min(aabb.min.y, w.y);
			aabb.min.z = std::min(aabb.min.z, w.z);
			aabb.max.x = std::max(aabb.max.x, w.x);
			aabb.max.y = std::max(aabb.max.y, w.y);
			aabb.max.z = std::max(aabb.max.z, w.z);
		}
		return aabb;
	}

	// プレイヤー（いまは軸揃えOBB）→ AABB
	AABB MakePlayerAABB(const OBB& obb) {
		// orientationsが単位基底なら center ± size でOK
		return { obb.center - obb.size, obb.center + obb.size };
	}

} // namespace

void PhysicalScene::Initialize()
{
	input_ = Input::GetInstance();

	camera = Object3DCommon::GetInstance()->GetDefaultCamera();
	camera->SetTranslate({ 0.0f, 2.0f, -20.0f });
	camera->SetRotate({ 0.0f, 0.0f, 0.0f });

	obb_.center = { 0.0f, 4.0f, 0.0f };
	obb_.orientations[0] = { 1.0f, 0.0f, 0.0f };
	obb_.orientations[1] = { 0.0f, 1.0f, 0.0f };
	obb_.orientations[2] = { 0.0f, 0.0f, 1.0f };
	obb_.size = { 1.0f, 2.0f, 1.0f };

	playerCollider_ = std::make_unique<Collider>();
	playerCollider_->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	// 初期 OBB を PhysicalScene::obb_ からコピー（half サイズの取り扱いに注意）
	playerCollider_->SetCenterPosition(obb_.center);
	playerCollider_->SetOBBHalfSize(obb_.size);        // ← Collider は「半サイズ」を持ちます。:contentReference[oaicite:7]{index=7}
	playerCollider_->SetOrientation({ 0,0,0 });          // 必要なら回転を設定

	auto levelLoader = std::make_unique<LevelLoader>();
	levelObjectManager_ = std::make_unique<LevelObjectManager>();
	levelObjectManager_->Initialize(*levelLoader->LoadLevel("Stage1.json"), "Stage1.gltf");

	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();
}

void PhysicalScene::Update()
{
	// --- 入力による移動方向の決定 ---
	Vector3 move = { 0.0f, 0.0f, 0.0f };

	if (input_->PushKey(DIK_W)) { move.z += 1.0f; }
	if (input_->PushKey(DIK_S)) { move.z -= 1.0f; }
	if (input_->PushKey(DIK_A)) { move.x -= 1.0f; }
	if (input_->PushKey(DIK_D)) { move.x += 1.0f; }

	if (Vector3::Length(move) > 0.0f) {
		move = Vector3::Normalize(move) * 0.05f; // 移動スピード
	}

	// --- ジャンプ ---
	if (isGrounded_ && input_->TriggerKey(DIK_SPACE)) {
		jumpVelocity_ = jumpPower_;
		isGrounded_ = false;
	}

	// --- Y軸速度に重力を適用 ---
	jumpVelocity_ -= gravity_;
	move.y += jumpVelocity_;

	// レベル側の衝突形状をAABB化（OBB→AABB）
	const auto worldAABBs = levelObjectManager_->GetWorldAABBs(); // 安全なAABB群:contentReference[oaicite:1]{index=1}

	const float kEps = 0.002f; // すき間

	auto resolveAxis = [&](int axis, float delta)
		{
			if (delta == 0.0f) return;

			// 移動前の中心（どちら側から来たか判定に使う）
			const Vector3 old = obb_.center;

			// 予定位置へその軸だけ動かす
			if (axis == 0) obb_.center.x += delta;
			if (axis == 1) obb_.center.y += delta;
			if (axis == 2) obb_.center.z += delta;

			// 予定位置のプレイヤーAABB（軸揃えOBB想定）
			AABB p{ obb_.center - obb_.size, obb_.center + obb_.size };

			bool hit = false;
			float bestFix = 0.0f;
			float bestDist = FLT_MAX;

			// 最も近い“正しい側”の面へクランプ候補を取る
			for (const auto& w : worldAABBs) {
				// AABB×AABB重なり判定（簡易）
				if (!(p.min.x <= w.max.x && p.max.x >= w.min.x &&
					p.min.y <= w.max.y && p.max.y >= w.min.y &&
					p.min.z <= w.max.z && p.max.z >= w.min.z)) {
					continue;
				}

				float cand = 0.0f; // この軸の新しい center 座標
				bool  valid = false;

				if (axis == 0) {
					// old（移動前）がどちら側にいたかで面を固定
					if (old.x + obb_.size.x <= w.min.x) { // 左（min側）から来た
						cand = (w.min.x - obb_.size.x) - kEps;
						valid = true;
					}
					else if (old.x - obb_.size.x >= w.max.x) { // 右（max側）から来た
						cand = (w.max.x + obb_.size.x) + kEps;
						valid = true;
					}
					else {
						// すでに内部にスポーンしていた：近い方の面へ
						float dMin = std::abs((w.min.x - obb_.size.x) - old.x);
						float dMax = std::abs((w.max.x + obb_.size.x) - old.x);
						cand = (dMin <= dMax) ? (w.min.x - obb_.size.x - kEps)
							: (w.max.x + obb_.size.x + kEps);
						valid = true;
					}
					if (valid) {
						float dist = std::abs(cand - obb_.center.x); // 今の予定位置からの修正量の小さい方
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
				else if (axis == 2) {
					if (old.z + obb_.size.z <= w.min.z) { // 手前→min側面
						cand = (w.min.z - obb_.size.z) - kEps; valid = true;
					}
					else if (old.z - obb_.size.z >= w.max.z) { // 奥→max側面
						cand = (w.max.z + obb_.size.z) + kEps; valid = true;
					}
					else {
						float dMin = std::abs((w.min.z - obb_.size.z) - old.z);
						float dMax = std::abs((w.max.z + obb_.size.z) - old.z);
						cand = (dMin <= dMax) ? (w.min.z - obb_.size.z - kEps)
							: (w.max.z + obb_.size.z + kEps);
						valid = true;
					}
					if (valid) {
						float dist = std::abs(cand - obb_.center.z);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
				else { // Y（床/天井）
					if (old.y - obb_.size.y >= w.max.y) { // 上から落ちて床（max面）へ
						cand = (w.max.y + obb_.size.y) + kEps; valid = true;
					}
					else if (old.y + obb_.size.y <= w.min.y) { // 下から上昇して天井（min面）へ
						cand = (w.min.y - obb_.size.y) - kEps; valid = true;
					}
					else {
						// 既に内部：近い方の面へ
						float dToFloor = std::abs((w.max.y + obb_.size.y) - old.y);
						float dToCeil = std::abs((w.min.y - obb_.size.y) - old.y);
						cand = (dToFloor <= dToCeil) ? (w.max.y + obb_.size.y + kEps)
							: (w.min.y - obb_.size.y - kEps);
						valid = true;
					}
					if (valid) {
						float dist = std::abs(cand - obb_.center.y);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
			}

			if (hit) {
				if (axis == 0) obb_.center.x = bestFix;         // X壁にくっつく（横スライドOK）
				if (axis == 2) obb_.center.z = bestFix;         // Z壁にくっつく
				if (axis == 1) {                                // 床/天井
					obb_.center.y = bestFix;
					if (delta < 0.0f) { isGrounded_ = true; jumpVelocity_ = 0.0f; } // 床
					else { if (jumpVelocity_ > 0.0f) jumpVelocity_ = 0.0f; } // 天井
				}
			}
		};

	// フレーム先頭で接地フラグは一旦false（Yで床に触れたら立て直す）
	isGrounded_ = false;

	// X → Z → Y（Yは最後：床処理）
	resolveAxis(0, move.x);
	resolveAxis(2, move.z);
	resolveAxis(1, move.y);

	// プレイヤーColliderへ反映
	playerCollider_->SetCenterPosition(obb_.center);
	playerCollider_->SetOBBHalfSize(obb_.size); // HalfExtentで統一

	// ===== ここから既存処理 =====
	camera->Update();
	levelObjectManager_->Update();

	collisionManager_->Update();
	collisionManager_->Reset();

	collisionManager_->AddCollider(playerCollider_.get());
	for (auto& collider : levelObjectManager_->GetWorldColliders()) {
		collisionManager_->AddCollider(collider.get());
	}
	collisionManager_->CheckAllCollisions();
}

void PhysicalScene::Draw3DObjects()
{
	levelObjectManager_->Draw();

	Wireframe::GetInstance()->DrawOBB(obb_, { 1.0f, 1.0f, 0.0f, 1.0f });

	collisionManager_->Draw();
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
	camera->DrawImGui();

	// プレイヤーOBB情報
	ImGui::Begin("Player OBB");
	ImGui::Text("Center: (%.2f, %.2f, %.2f)", obb_.center.x, obb_.center.y, obb_.center.z);
	ImGui::Text("Size: (%.2f, %.2f, %.2f)", obb_.size.x, obb_.size.y, obb_.size.z);
	ImGui::End();

}
