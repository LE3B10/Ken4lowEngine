#include "SkeletonAnimation.h"
#include "AnimationModel.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <SRVManager.h>
#include <Wireframe.h> 


void SkeletonAnimation::Initialize(AnimationModel* model, const std::string& fileName)
{
	model->animationTime_ = 0.0f;

	model->wvpResource = ResourceManager::CreateBufferResource(model->dxCommon_->GetDevice(), sizeof(TransformationMatrix));
	model->wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&model->wvpData)); // 書き込むためのアドレスを取得

	model->animation = model->LoadAnimationFile("Resources/AnimatedCube", fileName);
	
	model->skeleton = model->CreateSkelton(model->modelData.rootNode);
	model->skinCluster = model->CreateSkinCluster(model->skeleton, model->modelData, SRVManager::GetInstance()->GetDescriptorSize());

	// ✅【ここに追加】スキン行列をアイデンティティに固定（デバッグ用）
	for (size_t i = 0; i < model->skeleton.joints.size(); ++i)
	{
		model->skinCluster.mappedPalette[i].skeletonSpaceMatrix = Matrix4x4::MakeIdentity();
		model->skinCluster.mappedPalette[i].skeletonSpaceInverceTransposeMatrix = Matrix4x4::MakeIdentity();
	}
}

void SkeletonAnimation::Update(AnimationModel* model, float deltaTime)
{
	// 1. アニメーションの時間を更新
	model->animationTime_ += deltaTime;
	model->animationTime_ = std::fmod(model->animationTime_, model->animation.duration);

	// 2. 各ジョイントにアニメーション適用（Translate/Rotate/Scale更新）
	model->ApplyAnimation(model->animationTime_);

	// 3. スケルトン階層に沿ってワールド座標更新
	model->UpdateSkeleton(model->skeleton);

	// 4. GPU に送るスキニング用行列（ボーン行列）を更新
	model->UpdateSkinCluster(model->skinCluster, model->skeleton);

	// 5. モデルのルートノード（modelData.rootNode.name）にアニメーションがあるか確認
	const std::string& rootName = model->modelData.rootNode.name;
	auto it = model->animation.nodeAnimations.find(rootName);

	Vector3 translate(0.0f, 0.0f, 0.0f);
	Quaternion rotate(0.0f, 0.0f, 0.0f, 1.0f);
	Vector3 scale(1.0f, 1.0f, 1.0f);

	if (it != model->animation.nodeAnimations.end())
	{
		const NodeAnimation& rootAnim = it->second;
		if (!rootAnim.translate.empty()) translate = model->CalculateValue(rootAnim.translate, model->animationTime_);
		if (!rootAnim.rotate.empty()) rotate = model->CalculateValue(rootAnim.rotate, model->animationTime_);
		if (!rootAnim.scale.empty()) scale = model->CalculateValue(rootAnim.scale, model->animationTime_);
	}

	// 6. モデル本体のWVP行列更新
	Matrix4x4 localMatrix = Matrix4x4::MakeAffineMatrix(scale, rotate, translate);
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(model->worldTransform.scale_, model->worldTransform.rotate_, model->worldTransform.translate_);
	Matrix4x4 viewProj = model->camera_->GetViewProjectionMatrix();
	Matrix4x4 wvp = Matrix4x4::Multiply(localMatrix * worldMatrix, viewProj);

	model->wvpData->WVP = worldMatrix * viewProj;
	model->wvpData->World = localMatrix * worldMatrix;
	model->wvpData->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(localMatrix * worldMatrix));

	// 7. デバッグ用：ボーンのワイヤーフレーム描画
	if (model->hasSkeleton_ && model->wireframe_)
	{
		for (const Joint& joint : model->skeleton.joints)
		{
			if (joint.parent)
			{
				Vector3 parentPos = model->skeleton.joints[*joint.parent].skeletonSpaceMatrix.GetTranslation();
				Vector3 jointPos = joint.skeletonSpaceMatrix.GetTranslation();
				model->wireframe_->DrawLine(parentPos, jointPos, { 1.0f, 1.0f, 0.0f, 1.0f });
			}
		}
	}
}
