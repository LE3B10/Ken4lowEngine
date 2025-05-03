#include "SkeletonAnimation.h"
#include "AnimationModel.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <SRVManager.h>
#include <Wireframe.h> 
#include "Skeleton.h"
#include <AnimationModelManager.h>


void SkeletonAnimation::Initialize(AnimationModel* model, const std::string& fileName)
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();
	uint32_t descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	model->animationTime_ = 0.0f;

	model->wvpResource = ResourceManager::CreateBufferResource(device, sizeof(TransformationMatrix));
	model->wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&model->wvpData_)); // 書き込むためのアドレスを取得

	model->wvpData_->World = Matrix4x4::MakeIdentity();
	model->wvpData_->WVP = Matrix4x4::MakeIdentity();
	model->wvpData_->WorldInversedTranspose = Matrix4x4::MakeIdentity();

	// アニメーションファイルを読み込む
	model->animation = model->LoadAnimationFile("Resources/AnimatedCube", fileName);

	// スケルトンの生成と初期更新
	model->skeleton_ = Skeleton::CreateFromRootNode(model->GetModelData().rootNode);

	// スキンクラスターの初期化
	model->skinCluster_ = std::make_unique<SkinCluster>();
	model->skinCluster_->Initialize(model->GetModelData(), *model->skeleton_, descriptorSize);
}

void SkeletonAnimation::Update(AnimationModel* model, float deltaTime)
{
	// 1. アニメーションの時間を更新
	model->animationTime_ += deltaTime;
	model->animationTime_ = std::fmod(model->animationTime_, model->animation.duration);

	if (!model->skeleton_) return;

	auto& nodeAnimations = model->animation.nodeAnimations;
	auto& joints = model->skeleton_->GetJoints();

	// 2. ノードアニメーションの適用
	for (auto& joint : joints)
	{
		auto it = nodeAnimations.find(joint.name);

		// ノードアニメーションが見つからなかった場合は、親の行列を使用
		if (it != nodeAnimations.end())
		{
			NodeAnimation& nodeAnim = (*it).second;
			Vector3 translate = model->CalculateValue(nodeAnim.translate, model->animationTime_);
			Quaternion rotate = model->CalculateValue(nodeAnim.rotate, model->animationTime_);
			Vector3 scale = model->CalculateValue(nodeAnim.scale, model->animationTime_);

			// 座標系調整（Z軸反転で伸びを防ぐ）
			joint.transform.translate = translate;
			joint.transform.rotate = rotate;
			joint.transform.scale = scale;

			joint.localMatrix = Matrix4x4::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
		}
	}

	// 3. スケルトンの更新
	model->skeleton_->UpdateSkeleton();

	// 4. スキンクラスターの更新
	model->skinCluster_->UpdatePaletteMatrix(*model->skeleton_);

	// 5. WVPの更新
	model->UpdateAnimation(model->animationTime_);

	for (const Joint& joint : joints)
	{
		if (joint.parent)
		{
			Vector3 parentPos = joints[*joint.parent].skeletonSpaceMatrix.GetTranslation();
			Vector3 jointPos = joint.skeletonSpaceMatrix.GetTranslation();

			Wireframe::GetInstance()->DrawLine(parentPos, jointPos, { 1.0f, 1.0f, 0.0f, 1.0f });
		}
	}
}

void SkeletonAnimation::Draw(AnimationModel* model)
{
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	// 骨ありモデルの描画
	commandList->SetGraphicsRootSignature(AnimationModelManager::GetInstance()->GetRootSignature());
	commandList->SetPipelineState(AnimationModelManager::GetInstance()->GetPipelineState());

	D3D12_VERTEX_BUFFER_VIEW vbvs[2] = { model->vertexBufferView, model->skinCluster_->GetInfluenceBufferView() };
	commandList->IASetVertexBuffers(0, 2, vbvs);

	commandList->IASetIndexBuffer(&model->indexBufferView);

	model->material_.SetPipeline();
	commandList->SetGraphicsRootConstantBufferView(1, model->wvpResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, model->modelData.material.gpuHandle);
	commandList->SetGraphicsRootConstantBufferView(3, model->cameraResource->GetGPUVirtualAddress());

	LightManager::GetInstance()->PreDraw();

	commandList->SetGraphicsRootDescriptorTable(7, model->skinCluster_->GetPaletteSrvHandle().second);

	commandList->DrawIndexedInstanced(UINT(model->modelData.indices.size()), 1, 0, 0, 0);
}
