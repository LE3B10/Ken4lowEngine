#include "DummyModel.h"
#include <Input.h>

void DummyModel::Initialize()
{
	model_ = std::make_unique<AnimationModel>();
	model_->Initialize("humanWalking.gltf");
	model_->SetSkinningEnabled(true);

}

void DummyModel::Update()
{
	model_->Update();

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

	// 位置反映
	model_->SetTranslate(pos);
}

void DummyModel::Draw()
{
	model_->Draw();
	model_->DrawSkeletonWireframe();
	model_->DrawBodyPartColliders();
}

void DummyModel::DrawImGui()
{
	model_->DrawImGui();
}
