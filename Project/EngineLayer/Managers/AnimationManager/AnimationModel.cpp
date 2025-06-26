#include "AnimationModel.h"
#include "ModelManager.h"
#include <TextureManager.h>
#include <DirectXCommon.h>
#include <Object3DCommon.h>
#include <SRVManager.h>
#include <ResourceManager.h>
#include <Wireframe.h>
#include "AssimpLoader.h"
#include "LightManager.h"
#include <imgui.h>
#include <numeric>


/// -------------------------------------------------------------
///				　		 初期化処理
/// -------------------------------------------------------------
void AnimationModel::Initialize(const std::string& fileName)
{
	dxCommon_ = DirectXCommon::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// モデル読み込み
	modelData = AssimpLoader::LoadModel("Resources/AnimatedCube", fileName);

	// アニメーションモデルを読み込む
	animation = LoadAnimationFile("Resources/AnimatedCube", fileName);

	// ファイルの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);

	// 読み込んだテクスチャ番号を取得
	modelData.material.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureFilePath);

	skeleton_ = std::make_unique<Skeleton>();
	skeleton_ = Skeleton::CreateFromRootNode(modelData.rootNode);

	// スキンクラスターの初期化
	skinCluster_ = std::make_unique<SkinCluster>();
	skinCluster_->Initialize(modelData, *skeleton_, dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	// マテリアルデータの初期化処理
	material_.Initialize();

	// アニメーション用の頂点とインデックスバッファを作成
	animationMesh_ = std::make_unique<AnimationMesh>();
	animationMesh_->Initialize(dxCommon_->GetDevice(), modelData);

	// 行列データの初期化
	wvpResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationAnimationMatrix));
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_)); // 書き込むためのアドレスを取得
	wvpData_->World = Matrix4x4::MakeIdentity();
	wvpData_->WVP = Matrix4x4::MakeIdentity();
	wvpData_->WorldInversedTranspose = Matrix4x4::MakeIdentity();

#pragma region カメラ用のリソースを作成
	// カメラ用のリソースを作る
	cameraResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU));
	// 書き込むためのアドレスを取得
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// カメラの初期位置
	cameraData->worldPosition = camera_->GetTranslate();
#pragma endregion

	// スキニング設定リソースの作成
	CreateSkinningSettingResource();

	animationTime_ = 0.0f; // アニメーションを止める

	InitializeBones();
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void AnimationModel::Update()
{
	// FPSの取得 deltaTimeの計算
	deltaTime = 1.0f / dxCommon_->GetFPSCounter().GetFPS();
	animationTime_ += deltaTime;
	animationTime_ = std::fmod(animationTime_, animation.duration);

	if (skinningSetting_->isSkinning)
	{
		auto& nodeAnimations = animation.nodeAnimations;
		auto& joints = skeleton_->GetJoints();

		// 2. ノードアニメーションの適用
		for (auto& joint : joints)
		{
			auto it = nodeAnimations.find(joint.name);

			// ノードアニメーションが見つからなかった場合は、親の行列を使用
			if (it != nodeAnimations.end())
			{
				NodeAnimation& nodeAnim = (*it).second;
				Vector3 translate = CalculateValue(nodeAnim.translate, animationTime_);
				Quaternion rotate = CalculateValue(nodeAnim.rotate, animationTime_);
				Vector3 scale = CalculateValue(nodeAnim.scale, animationTime_);

				// 座標系調整（Z軸反転で伸びを防ぐ）
				joint.transform.translate = translate;
				joint.transform.rotate = rotate;
				joint.transform.scale = scale;

				joint.localMatrix = Matrix4x4::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
			}
		}

		// 3. スケルトンの更新
		skeleton_->UpdateSkeleton();

		// 4. スキンクラスターの更新
		skinCluster_->UpdatePaletteMatrix(*skeleton_);
	}

	// スキンニング設定の更新
	skinningSetting_->isSkinning = this->skinningSetting_->isSkinning;

	UpdateAnimation();

	// マテリアルの更新処理
	material_.Update();
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void AnimationModel::Draw()
{
	SRVManager::GetInstance()->PreDraw();

	auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

	if (skinningSetting_->isSkinning)
	{
		D3D12_VERTEX_BUFFER_VIEW vbvs[2] = { animationMesh_->GetVertexBufferView(), skinCluster_->GetInfluenceBufferView() };
		commandList->IASetVertexBuffers(0, 2, vbvs);
		commandList->IASetIndexBuffer(&animationMesh_->GetIndexBufferView());
		commandList->SetGraphicsRootDescriptorTable(7, skinCluster_->GetPaletteSrvHandle().second); // スキニングのパレットをセット
	}
	else
	{
		commandList->IASetVertexBuffers(0, 1, &GetAnimationMesh()->GetVertexBufferView());
		commandList->IASetIndexBuffer(&GetAnimationMesh()->GetIndexBufferView());
	}

	material_.SetPipeline();

	commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, modelData.material.gpuHandle);
	commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(8, skinningSettingResource_->GetGPUVirtualAddress()); // スキニング設定をセット

	commandList->DrawIndexedInstanced(UINT(modelData.indices.size()), 1, 0, 0, 0);
}

void AnimationModel::DrawImGui()
{

}

void AnimationModel::DrawSkeletonWireframe()
{
	if (!skeleton_) { return; }

	const auto& joints = skeleton_->GetJoints();
	for (const auto& joint : joints) {
		if (joint.parent.has_value()) {
			const auto& parentJoint = joints[*joint.parent];

			Vector3 parentPos = parentJoint.skeletonSpaceMatrix.GetTranslation();
			Vector3 jointPos = joint.skeletonSpaceMatrix.GetTranslation();

			Wireframe::GetInstance()->DrawLine(parentPos, jointPos, { 1.0f, 0.0f, 0.0f, 1.0f });
		}
	}
}

void AnimationModel::DrawBodyPartColliders()
{
	if (!skeleton_) { return; }

	const auto& joints = skeleton_->GetJoints();

	for (const auto& part : bodyPartColliders_) {
		if (part.endJointIndex < 0) {
			// スフィア用
			const auto& joint = joints[part.startJointIndex];
			Matrix4x4 jointMatrix = joint.skeletonSpaceMatrix;

			Vector3 center = jointMatrix.GetTranslation() + part.offset;

			Wireframe::GetInstance()->DrawSphere(center, part.radius, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
		else {
			// カプセル用
			const auto& jointA = joints[part.startJointIndex];
			const auto& jointB = joints[part.endJointIndex];

			Vector3 a = jointA.skeletonSpaceMatrix.GetTranslation();
			Vector3 b = jointB.skeletonSpaceMatrix.GetTranslation();

			Vector3 center = (a + b) * 0.5f;
			Vector3 axis = Vector3::Normalize(b - a);
			float height = Vector3::Length(b - a);

			Wireframe::GetInstance()->DrawCapsule(center, part.radius, height, axis, 8, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
	}
}


/// -------------------------------------------------------------
///				　	アニメーションの更新処理
/// -------------------------------------------------------------
void AnimationModel::UpdateAnimation()
{
	if (skeleton_ && !skeleton_->GetJoints().empty() && skinningSetting_->isSkinning)
	{
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
		Matrix4x4 worldViewProjectionMatrix;

		if (camera_)
		{
			const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
			worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);
		}
		else
		{
			worldViewProjectionMatrix = worldMatrix;
		}

		wvpData_->World = worldMatrix;
		wvpData_->WVP = worldViewProjectionMatrix;
		wvpData_->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
	}
	else
	{
		// アニメーション無し（通常モデル用のWVP更新）
		NodeAnimation& rootNodeAnimation = animation.nodeAnimations[modelData.rootNode.name];
		Vector3 translate = CalculateValue(rootNodeAnimation.translate, animationTime_);
		Quaternion rotate = CalculateValue(rootNodeAnimation.rotate, animationTime_);
		Vector3 scale = CalculateValue(rootNodeAnimation.scale, animationTime_);

		Matrix4x4 localMatrix = Matrix4x4::MakeAffineMatrix(scale, rotate, translate);

		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
		Matrix4x4 worldViewProjectionMatrix;

		if (camera_)
		{
			const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
			worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);
		}
		else
		{
			worldViewProjectionMatrix = worldMatrix;
		}

		wvpData_->WVP = localMatrix * worldViewProjectionMatrix;
		wvpData_->World = localMatrix * worldMatrix;
		wvpData_->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(localMatrix * worldMatrix));
	}
}


/// -------------------------------------------------------------
///				　アニメーションファイルを読み込む
/// -------------------------------------------------------------
Animation AnimationModel::LoadAnimationFile(const std::string& directoryPath, const std::string& fileName)
{
	// アニメーションを解析
	Animation animation{};
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + fileName;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
	assert(scene->mNumAnimations != 0); // アニメーションがない
	aiAnimation* animationAssimp = scene->mAnimations[0]; // 最初のアニメーションだけ採用。もちろん複数対応するに越したことない
	animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond); // 時間の単位を秒に変換

	// NodeAnimationを解析する

	// Assimpでは個々のNodeのAnimationをchannelと読んでいるのでchannelをまわしてNodeAnimationの情報を撮ってくる
	for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex)
	{
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
		NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

		// 座標（transform）のキーフレームを追加
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // ここも秒に変換
			keyframe.value = { -keyAssimp.mValue.x, keyAssimp.mValue.y,keyAssimp.mValue.z }; // 右手 → 左手
			nodeAnimation.translate.push_back(keyframe);
		}

		// 回転（rotate）のキーフレームを追加
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex)
		{
			aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
			KeyframeQuaternion keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
			keyframe.value = { keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w }; // 右手 → 左手
			nodeAnimation.rotate.push_back(keyframe);
		}

		// スケール（scale）のキーフレームを追加
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
			keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
			nodeAnimation.scale.push_back(keyframe);
		}
	}
	// 解析終了
	return animation;
}

void AnimationModel::CreateSkinningSettingResource()
{
	// スキニング用のリソースを作成
	skinningSettingResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(SkinningSetting));
	skinningSettingResource_->Map(0, nullptr, reinterpret_cast<void**>(&skinningSetting_));

	skinningSetting_->isSkinning = false;
}

void AnimationModel::InitializeBones()
{
	auto& jointMap = skeleton_->GetJointMap();

	// ----- Head, Neck (スフィア)
	if (auto it = jointMap.find("mixamorig:Head"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "Head", it->second, -1, {0, 0.12f, 0}, 0.12f, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:Neck"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "Neck", it->second, -1, {0, 0.05f, 0}, 0.1f, 0.0f });
	}

	// ----- Spine（Spine → Spine1）
	if (jointMap.contains("mixamorig:Spine") && jointMap.contains("mixamorig:Spine1")) {
		bodyPartColliders_.push_back({ "SpineLower", jointMap.at("mixamorig:Spine"), jointMap.at("mixamorig:Spine1"), {}, 0.15f, 0.0f });
	}
	if (jointMap.contains("mixamorig:Spine1") && jointMap.contains("mixamorig:Spine2")) {
		bodyPartColliders_.push_back({ "SpineUpper", jointMap.at("mixamorig:Spine1"), jointMap.at("mixamorig:Spine2"), {}, 0.15f, 0.0f });
	}

	// ----- Left Arm（Arm → Elbow → Wrist）
	if (jointMap.contains("mixamorig:LeftArm") && jointMap.contains("mixamorig:LeftForeArm")) {
		bodyPartColliders_.push_back({ "LeftUpperArm", jointMap.at("mixamorig:LeftArm"), jointMap.at("mixamorig:LeftForeArm"), {}, 0.1f, 0.0f });
	}
	if (jointMap.contains("mixamorig:LeftForeArm") && jointMap.contains("mixamorig:LeftHand")) {
		bodyPartColliders_.push_back({ "LeftLowerArm", jointMap.at("mixamorig:LeftForeArm"), jointMap.at("mixamorig:LeftHand"), {}, 0.09f, 0.0f });
	}

	// ----- Right Arm
	if (jointMap.contains("mixamorig:RightArm") && jointMap.contains("mixamorig:RightForeArm")) {
		bodyPartColliders_.push_back({ "RightUpperArm", jointMap.at("mixamorig:RightArm"), jointMap.at("mixamorig:RightForeArm"), {}, 0.1f, 0.0f });
	}
	if (jointMap.contains("mixamorig:RightForeArm") && jointMap.contains("mixamorig:RightHand")) {
		bodyPartColliders_.push_back({ "RightLowerArm", jointMap.at("mixamorig:RightForeArm"), jointMap.at("mixamorig:RightHand"), {}, 0.09f, 0.0f });
	}

	// ----- Left Leg
	if (jointMap.contains("mixamorig:LeftUpLeg") && jointMap.contains("mixamorig:LeftLeg")) {
		bodyPartColliders_.push_back({ "LeftUpperLeg", jointMap.at("mixamorig:LeftUpLeg"), jointMap.at("mixamorig:LeftLeg"), {}, 0.12f, 0.0f });
	}
	if (jointMap.contains("mixamorig:LeftLeg") && jointMap.contains("mixamorig:LeftFoot")) {
		bodyPartColliders_.push_back({ "LeftLowerLeg", jointMap.at("mixamorig:LeftLeg"), jointMap.at("mixamorig:LeftFoot"), {}, 0.1f, 0.0f });
	}

	// ----- Right Leg
	if (jointMap.contains("mixamorig:RightUpLeg") && jointMap.contains("mixamorig:RightLeg")) {
		bodyPartColliders_.push_back({ "RightUpperLeg", jointMap.at("mixamorig:RightUpLeg"), jointMap.at("mixamorig:RightLeg"), {}, 0.12f, 0.0f });
	}
	if (jointMap.contains("mixamorig:RightLeg") && jointMap.contains("mixamorig:RightFoot")) {
		bodyPartColliders_.push_back({ "RightLowerLeg", jointMap.at("mixamorig:RightLeg"), jointMap.at("mixamorig:RightFoot"), {}, 0.1f, 0.0f });
	}


	// ----- Left Toe（Foot → ToeBase）
	if (jointMap.contains("mixamorig:LeftFoot") && jointMap.contains("mixamorig:LeftToeBase")) {
		bodyPartColliders_.push_back({ "LeftToe", jointMap.at("mixamorig:LeftFoot"), jointMap.at("mixamorig:LeftToeBase"), {}, 0.07f, 0.0f });
	}

	// ----- Right Toe（Foot → ToeBase）
	if (jointMap.contains("mixamorig:RightFoot") && jointMap.contains("mixamorig:RightToeBase")) {
		bodyPartColliders_.push_back({ "RightToe", jointMap.at("mixamorig:RightFoot"), jointMap.at("mixamorig:RightToeBase"), {}, 0.07f, 0.0f });
	}
}
