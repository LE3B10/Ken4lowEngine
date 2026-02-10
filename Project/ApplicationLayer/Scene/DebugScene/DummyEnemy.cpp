#include "DummyEnemy.h"
#include <Input.h>
#include "CollisionTypeIdDef.h"

using namespace Ken4lowEngine;

void DummyEnemy::Initialize()
{
    input_ = K4E::Input::GetInstance();

    Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));
    Collider::SetOwner(this);

    model_ = std::make_unique<K4E::Object3D>();
    model_->Initialize("cube.gltf");
    Collider::SetOBBHalfSize(model_->GetScale());

    debugColor_ = { 0.0f, 1.0f, 0.0f, 1.0f };
    model_->SetColor(debugColor_);
    model_->Update();

    contactRecord_.Clear();
    hp_ = 10;
}

void DummyEnemy::Update()
{
    // 移動処理（矢印キー）
    moveVelocity_ = { 0.0f, 0.0f, 0.0f };
    if (input_->PushKey(DIK_UP)) { moveVelocity_.z += 0.1f; }
    if (input_->PushKey(DIK_DOWN)) { moveVelocity_.z -= 0.1f; }
    if (input_->PushKey(DIK_LEFT)) { moveVelocity_.x -= 0.1f; }
    if (input_->PushKey(DIK_RIGHT)) { moveVelocity_.x += 0.1f; }

    model_->SetTranslate(K4E::Vector3::Add(model_->GetTranslate(), moveVelocity_));
    Collider::SetCenterPosition(model_->GetTranslate());

    model_->Update();
}

void DummyEnemy::Draw()
{
    if (model_) model_->Draw();
}

void DummyEnemy::DrawImGui()
{
    if (model_) model_->DrawImGui();
#ifdef USE_IMGUI
    // HP表示を追加したい場合はここにImGuiを足す
#endif
}

void DummyEnemy::TakeDamage(int damage)
{
    hp_ -= damage;
    if (hp_ < 0) hp_ = 0;

    // ヒットしたら赤（接触が切れたらExitで戻る）
    debugColor_ = { 1.0f, 0.0f, 0.0f, 1.0f };
    if (model_)
    {
        model_->SetColor(debugColor_);
        model_->Update();
    }
}

void DummyEnemy::SetCenterPosition(const K4E::Vector3& pos)
{
    Collider::SetCenterPosition(pos);
    if (model_) { model_->SetTranslate(pos); }
}

void DummyEnemy::OnCollisionEnter(K4E::Collider* other)
{
    if (!other) return;

    // 接触中リストに登録（重複はContactRecord側で防止）
    contactRecord_.Add(other->GetUniqueID());

    // 何かと当たっている間は赤にする（デバッグ用）
    debugColor_ = { 1.0f, 0.0f, 0.0f, 1.0f };
    if (model_)
    {
        model_->SetColor(debugColor_);
        model_->Update();
    }
}

void DummyEnemy::OnCollisionExit(K4E::Collider* other)
{
    if (!other) return;

    contactRecord_.Remove(other->GetUniqueID());

    // 接触相手がいなくなったら元の色に戻す
    if (contactRecord_.GetRecordCount() == 0)
    {
        debugColor_ = { 0.0f, 1.0f, 0.0f, 1.0f };
        if (model_)
        {
            model_->SetColor(debugColor_);
            model_->Update();
        }
    }
}
