#include "NoSkeletonAnimation.h"
#include "AnimationModel.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <LightManager.h>

void NoSkeletonAnimation::Initialize(AnimationModel* model, const std::string& fileName)
{
	model->animationTime_ = 0.0f;
	model->animation = model->LoadAnimationFile("Resources/AnimatedCube", fileName);

	model->wvpResource = ResourceManager::CreateBufferResource(model->dxCommon_->GetDevice(), sizeof(TransformationMatrix));
	model->wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&model->wvpData_)); // 書き込むためのアドレスを取得
	model->wvpData_->World = Matrix4x4::MakeIdentity();
	model->wvpData_->WVP = Matrix4x4::MakeIdentity();
	model->wvpData_->WorldInversedTranspose = Matrix4x4::MakeIdentity();
}

void NoSkeletonAnimation::Update(AnimationModel* model, float deltaTime)
{
	model->animationTime_ += deltaTime;
	model->animationTime_ = std::fmod(model->animationTime_, model->animation.duration);
	model->UpdateAnimation(model->animationTime_);
}

void NoSkeletonAnimation::Draw(AnimationModel* model)
{
	auto commandList = model->dxCommon_->GetCommandList();

	commandList->IASetVertexBuffers(0, 1, &model->vertexBufferView);
	commandList->IASetIndexBuffer(&model->indexBufferView);

	model->material_.SetPipeline();
	commandList->SetGraphicsRootConstantBufferView(1, model->wvpResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, model->modelData.material.gpuHandle);
	commandList->SetGraphicsRootConstantBufferView(3, model->cameraResource->GetGPUVirtualAddress());

	LightManager::GetInstance()->PreDraw();

	commandList->DrawIndexedInstanced(UINT(model->modelData.indices.size()), 1, 0, 0, 0);
}
