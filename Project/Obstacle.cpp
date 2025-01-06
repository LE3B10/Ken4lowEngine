#include "Obstacle.h"


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void Obstacle::Initialize(Object3DCommon* object3DCommon, const std::string& modelFile, const Transform& initialTransform)
{
    obstacleObject_ = std::make_unique<Object3D>();
    obstacleObject_->Initialize(object3DCommon, modelFile);
    transform_ = initialTransform;
    obstacleObject_->SetTranslate(transform_.translate);
}


/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void Obstacle::Update(float scrollSpeed)
{
    transform_.translate.z -= scrollSpeed;
    obstacleObject_->SetTranslate(transform_.translate);
    obstacleObject_->Update();
}


/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
void Obstacle::Draw(const Camera* camera)
{
    obstacleObject_->Draw();
}
