#include "NoSkeletonAnimation.h"
#include "AnimationModel.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>


void NoSkeletonAnimation::Initialize(AnimationModel* model, const std::string& fileName)
{
	model->animationTime_ = 0.0f;
	model->wvpResource = ResourceManager::CreateBufferResource(model->dxCommon_->GetDevice(), sizeof(TransformationMatrix));
	model->wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&model->wvpData)); // 書き込むためのアドレスを取得
	model->animation = model->LoadAnimationFile("Resources/AnimatedCube", fileName);
}

void NoSkeletonAnimation::Update(AnimationModel* model, float deltaTime)
{
	model->animationTime_ += deltaTime;
	model->animationTime_ = std::fmod(model->animationTime_, model->animation.duration);
	model->ApplyAnimation(model->animationTime_);
	model->UpdateAnimation(model->animationTime_);
}
