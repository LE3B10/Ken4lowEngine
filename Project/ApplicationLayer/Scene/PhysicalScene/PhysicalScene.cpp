#include "PhysicalScene.h"
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include <AnimationPipelineBuilder.h>
#include "SkyBoxManager.h"
#include <Wireframe.h>
#include <SceneManager.h>
#include <CollisionUtility.h>

#include <imgui.h>

void PhysicalScene::Initialize()
{
	input_ = Input::GetInstance();

	// カプセルの初期化
	capsule_.segment = { { 0.0f, 0.5f, 0.0f }, { 0.0f, 1.5f, 0.0f } };
	capsule_.radius = 0.3f;

	// AABBの初期化（地面）
	AABB ground;
	ground.min = { -2.0f, -0.5f, -4.0f };
	ground.max = { +4.0f, +0.0f, +4.0f };
	aabbs_.emplace_back(ColliderType::Ground, ground);

	// AABBの初期化（壁）
	AABB wall;
	wall.min = { +2.0f, -0.5f, -4.0f };
	wall.max = { +2.2f, +1.5f, +4.0f };
	aabbs_.emplace_back(ColliderType::Wall, wall);
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

	// --- カプセルの移動適用 ---
	capsule_.segment.origin += move;
	capsule_.segment.diff = Vector3{ 0.0f, 1.0f, 0.0f }; // 高さ1固定

	const int kMaxCorrection = 4; // 多段補正回数の最大
	for (int step = 0; step < kMaxCorrection; ++step) {
		Vector3 bestPushVec{};
		float bestDistSq = FLT_MAX;
		ColliderType bestType = ColliderType::None;

		for (const auto& [type, aabb] : aabbs_) {
			if (!CollisionUtility::IsCollision(capsule_, aabb)) { continue; }

			const int kSteps = 10;
			for (int i = 0; i <= kSteps; ++i) {
				float t = static_cast<float>(i) / kSteps;
				Vector3 pt = capsule_.segment.origin + capsule_.segment.diff * t;

				Vector3 closest = {
					std::clamp(pt.x, aabb.min.x, aabb.max.x),
					std::clamp(pt.y, aabb.min.y, aabb.max.y),
					std::clamp(pt.z, aabb.min.z, aabb.max.z),
				};

				Vector3 diff = pt - closest;
				float distSq = Vector3::Dot(diff, diff);

				if (distSq < bestDistSq && distSq < capsule_.radius * capsule_.radius && distSq > 0.00001f) {
					bestDistSq = distSq;
					bestPushVec = diff;
					bestType = type;
				}
			}
		}

		if (bestType == ColliderType::None) break;

		float dist = std::sqrt(bestDistSq);
		Vector3 pushDir = bestPushVec / dist;
		float pushLen = capsule_.radius - dist;

		Vector3 correction{};

		if (bestType == ColliderType::Ground || bestType == ColliderType::Wall) {
			if (pushDir.y > 0.0f) {
				correction.y = pushDir.y * pushLen;

				// 上から接触していれば接地扱い（Ground or Wall問わず）
				if (pushDir.y > 0.5f) {
					isGrounded_ = true;
					jumpVelocity_ = 0.0f;
				}
			}

			// 横方向補正（壁の場合のみ必要）
			if (bestType == ColliderType::Wall) {
				if (fabs(pushDir.x) > fabs(pushDir.z)) {
					correction.x = pushDir.x * pushLen;
				}
				else {
					correction.z = pushDir.z * pushLen;
				}
			}
		}

		capsule_.segment.origin += correction;
	}

	for (const auto& [type, aabb] : aabbs_) {
		Vector4 color;
		switch (type) {
		case ColliderType::Ground:
			color = { 0.2f, 1.0f, 0.2f, 1.0f }; // 緑
			break;
		case ColliderType::Wall:
			color = { 0.2f, 0.5f, 1.0f, 1.0f }; // 青
			break;
		default:
			color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白
			break;
		}
		Wireframe::GetInstance()->DrawAABB(aabb, color);
	}

	// カプセル描画（常に赤で表示）
	Wireframe::GetInstance()->DrawCapsule(capsule_, { 1.0f, 0.0f, 0.0f, 1.0f });
}

void PhysicalScene::Draw3DObjects()
{
	SkyBoxManager::GetInstance()->SetRenderSetting();



	Object3DCommon::GetInstance()->SetRenderSetting();



	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();



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
	ImGui::Begin("Physical Scene Debug");
	ImGui::Text("AABB Min: (%.2f, %.2f, %.2f)", aabb_.min.x, aabb_.min.y, aabb_.min.z);
	ImGui::Text("AABB Max: (%.2f, %.2f, %.2f)", aabb_.max.x, aabb_.max.y, aabb_.max.z);
	ImGui::Text("Capsule Start: (%.2f, %.2f, %.2f)", capsule_.segment.origin.x, capsule_.segment.origin.y, capsule_.segment.origin.z);
	ImGui::Text("Capsule End: (%.2f, %.2f, %.2f)",
		capsule_.segment.origin.x + capsule_.segment.diff.x,
		capsule_.segment.origin.y + capsule_.segment.diff.y,
		capsule_.segment.origin.z + capsule_.segment.diff.z);
	ImGui::Text("Capsule Radius: %.2f", capsule_.radius);
	ImGui::End();

	ImGui::Begin("Physical Scene Controls");
	ImGui::DragFloat3("AABB Min", &aabb_.min.x, 0.1f);
	ImGui::DragFloat3("AABB Max", &aabb_.max.x, 0.1f);
	ImGui::DragFloat3("Capsule Start", &capsule_.segment.origin.x, 0.1f);
	ImGui::DragFloat3("Capsule End", &capsule_.segment.diff.x, 0.1f);
	ImGui::DragFloat("Capsule Radius", &capsule_.radius, 0.1f, 0.0f, FLT_MAX);
	ImGui::End();
}
