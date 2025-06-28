#include "DummyModel.h"
#include <Input.h>
#include <CollisionTypeIdDef.h>
#include <CollisionManager.h>

#include <imgui.h>

void DummyModel::Initialize()
{
	model_ = std::make_unique<AnimationModel>();
	model_->Initialize("humanWalking01.gltf");
	model_->SetSkinningEnabled(true);
}

void DummyModel::Update()
{
	// 入力取得
	auto input = Input::GetInstance();

	// 移動速度
	const float moveSpeed = 2.0f;   // 単位: m/s
	const float deltaTime = 1.0f / 60.0f;

	// 入力ベクトル
	Vector3 move = { 0.0f, 0.0f, 0.0f };

	if (input->PushKey(DIK_UP)) move.z += 1.0f;
	if (input->PushKey(DIK_DOWN)) move.z -= 1.0f;
	if (input->PushKey(DIK_LEFT)) move.x -= 1.0f;
	if (input->PushKey(DIK_RIGHT)) move.x += 1.0f;

	// 正規化して斜め移動時も速度一定に
	if (Vector3::Length(move) > 1.0f)
	{
		move = Vector3::Normalize(move);
	}

	// 現在位置を取得
	Vector3 pos = model_->GetTranslate();

	// 移動
	pos.x += move.x * moveSpeed * deltaTime;
	pos.z += move.z * moveSpeed * deltaTime;
	model_->SetTranslate(pos);
	model_->Update();

	// 毎フレームカプセルを骨に追従
	auto partCaps = model_->GetBodyPartCapsulesWorld();

	if (bodyCols_.size() != partCaps.size()) {
		// Bone 本数が変わったら作り直し（開発中のモデル差し替え対策）
		bodyCols_.clear();
		for (auto& [name, cap] : partCaps) {
			auto c = std::make_unique<Collider>();
			c->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));
			c->SetCapsule(cap);
			bodyCols_.push_back({ name, std::move(c) });
		}
	}

	for (size_t i = 0; i < partCaps.size(); ++i) {
		bodyCols_[i].col->SetCapsule(partCaps[i].second);
	}
}

void DummyModel::Draw()
{
	model_->Draw();
	model_->DrawSkeletonWireframe();
	for (auto& pc : bodyCols_) { pc.col->Draw(); }
}

void DummyModel::DrawImGui()
{
	model_->DrawImGui();

	ImGui::Begin("Dummy Model Settings");
	if (ImGui::CollapsingHeader("Dummy Colliders"))
	{
		// 全部まとめて可視化トグル
		static bool visible = true;
		if (ImGui::Checkbox("Show All Capsules", &visible)) {
			for (auto& pc : bodyCols_) {
				pc.col->SetCapsuleVisible(visible);
			}
		}

		// 個別編集したいときは…
		for (size_t i = 0; i < bodyCols_.size(); ++i) {
			if (ImGui::TreeNode(bodyCols_[i].name.c_str())) {
				bodyCols_[i].col->DrawImGui();   // 既存 UI を再利用
				ImGui::TreePop();
			}
		}
	}
	ImGui::End();
}

void DummyModel::RegisterColliders(CollisionManager* collisionManager) const
{
	// 本体（必要なら）も先に登録
	collisionManager->AddCollider(const_cast<DummyModel*>(this));

	// 部位カプセルをすべて登録
	for (const auto& pc : bodyCols_) {
		collisionManager->AddCollider(pc.col.get());
	}
}

void DummyModel::OnCollision(Collider* other)
{

}
