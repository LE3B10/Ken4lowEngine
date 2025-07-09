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
void AnimationModel::Initialize(const std::string& fileName, bool isSkinning)
{
	// リソースを破棄
	Clear();

	dxCommon_ = DirectXCommon::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// モデル読み込み
	modelData = AssimpLoader::LoadModel("Resources", fileName);

	// アニメーションモデルを読み込む
	animation = LoadAnimationFile("Resources", fileName);

	std::string modelDir = "Resources/" + fileName.substr(0, fileName.find_last_of('/'));
	std::string texturePath = modelDir + "/" + modelData.material.textureFilePath;

	// ファイルの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(texturePath);

	// 読み込んだテクスチャ番号を取得
	modelData.material.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texturePath);

	skeleton_ = std::make_unique<Skeleton>();
	skeleton_ = Skeleton::CreateFromRootNode(modelData.rootNode);

	// スキンクラスターの初期化
	skinCluster_ = std::make_unique<SkinCluster>();
	skinCluster_->Initialize(modelData, *skeleton_);

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
	skinningSetting_->isSkinning = isSkinning; // スキニング設定を反映

	const std::string filePath = "Resources/Mask/Noise.png";
	// Dissolve設定リソースの作成
	TextureManager::GetInstance()->LoadTexture(filePath);
	// SRVインデックスを取得（CopySRVせず、既存SRVをそのまま使う）
	dissolveMaskSrvIndex_ = TextureManager::GetInstance()->GetSrvIndex(filePath);
	// Dissolve設定リソースの作成
	CreateDissolveSettingResource();

	InitializeBones();
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void AnimationModel::Update()
{
	if (isAnimationPlaying_)
	{
		// FPSの取得 deltaTimeの計算
		deltaTime = 1.0f / dxCommon_->GetFPSCounter().GetFPS();
		animationTime_ += deltaTime;
		animationTime_ = std::fmod(animationTime_, animation.duration);
	}

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
	//commandList->SetGraphicsRootDescriptorTable(2, modelData.material.gpuHandle);
	commandList->SetGraphicsRootDescriptorTable(2, SRVManager::GetInstance()->GetGPUDescriptorHandle(dissolveMaskSrvIndex_)); // t1
	commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(8, skinningSettingResource_->GetGPUVirtualAddress()); // スキニング設定をセット
	if (dissolveSettingResource_)
	{
		commandList->SetGraphicsRootConstantBufferView(9, dissolveSettingResource_->GetGPUVirtualAddress()); // Dissolve設定をセット
	}

	commandList->DrawIndexedInstanced(UINT(modelData.indices.size()), 1, 0, 0, 0);
}

void AnimationModel::DrawImGui()
{
	ImGui::SliderFloat("Dissolve Threshold", &dissolveSetting_->threshold, 0.0f, 1.05f);
	ImGui::SliderFloat("Edge Thickness", &dissolveSetting_->edgeThickness, 0.0f, 2.0f);
	ImGui::ColorEdit4("Edge Color", &dissolveSetting_->edgeColor.x);
}

void AnimationModel::Clear()
{
	animationMesh_.reset();
	skeleton_.reset();
	skinCluster_.reset();

	wvpResource.Reset();
	cameraResource.Reset();
	skinningSettingResource_.Reset();

	wvpData_ = nullptr;
	cameraData = nullptr;
	skinningSetting_ = nullptr;

	modelData = {};       // モデルデータ初期化
	animation = {};       // アニメーション初期化
	animationTime_ = 0.0f;
	bodyPartColliders_.clear();
}

void AnimationModel::DrawSkeletonWireframe()
{
	if (!skeleton_) { return; }

	const auto& joints = skeleton_->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	for (const auto& joint : joints) {
		if (joint.parent.has_value()) {
			const auto& parentJoint = joints[*joint.parent];

			Vector3 parentLocal = parentJoint.skeletonSpaceMatrix.GetTranslation();
			Vector3 jointLocal = joint.skeletonSpaceMatrix.GetTranslation();

			Vector3 parentPos = Vector3::Transform(parentLocal, worldMatrix);
			Vector3 jointPos = Vector3::Transform(jointLocal, worldMatrix);

			Wireframe::GetInstance()->DrawLine(parentPos, jointPos, { 1.0f, 0.0f, 0.0f, 1.0f });
		}
	}
}

void AnimationModel::DrawBodyPartColliders()
{
	if (!skeleton_) { return; }

	const auto& joints = skeleton_->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	for (const auto& part : bodyPartColliders_) {
		if (part.endJointIndex < 0) {
			// スフィア用
			const auto& joint = joints[part.startJointIndex];
			Vector3 localPos = joint.skeletonSpaceMatrix.GetTranslation() + part.offset;
			Vector3 worldPos = Vector3::Transform(localPos, worldMatrix);

			Wireframe::GetInstance()->DrawSphere(worldPos, part.radius, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
		else {
			// カプセル用
			const auto& jointA = joints[part.startJointIndex];
			const auto& jointB = joints[part.endJointIndex];

			Vector3 a = Vector3::Transform(jointA.skeletonSpaceMatrix.GetTranslation(), worldMatrix);
			Vector3 b = Vector3::Transform(jointB.skeletonSpaceMatrix.GetTranslation(), worldMatrix);

			Vector3 center = (a + b) * 0.5f;
			Vector3 axis = Vector3::Normalize(b - a);
			float height = Vector3::Length(b - a);

			Wireframe::GetInstance()->DrawCapsule(center, part.radius, height, axis, 8, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
	}
}


std::vector<std::pair<std::string, Capsule>> AnimationModel::GetBodyPartCapsulesWorld() const
{
	std::vector<std::pair<std::string, Capsule>> out;
	if (!skeleton_) { return out; }

	const auto& joints = skeleton_->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	for (const auto& part : bodyPartColliders_)
	{
		Capsule capsule{};
		capsule.radius = part.radius;

		if (part.endJointIndex < 0) {
			// Sphere → pointA = pointB
			const Vector3  local = joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation() + part.offset;
			Vector3 world = Vector3::Transform(local, worldMatrix);
			capsule.pointA = capsule.pointB = world;
		}
		else {
			// カプセル → 始点と終点両方に回転適用
			Vector3 a = Vector3::Transform(joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation(), worldMatrix);
			Vector3 b = Vector3::Transform(joints[part.endJointIndex].skeletonSpaceMatrix.GetTranslation(), worldMatrix);
			capsule.pointA = a;
			capsule.pointB = b;
		}
		out.emplace_back(part.name, capsule);
	}
	return out;
}

std::vector<std::pair<std::string, Sphere>> AnimationModel::GetBodyPartSpheresWorld() const
{
	std::vector<std::pair<std::string, Sphere>> out;
	if (!skeleton_) { return out; }

	const auto& joints = skeleton_->GetJoints();
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
		worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

	for (const auto& part : bodyPartColliders_) {
		if (part.endJointIndex < 0) {
			Sphere s{};
			Vector3 local = joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation() + part.offset;
			s.center = Vector3::Transform(local, worldMatrix);
			s.radius = part.radius;
			out.emplace_back(part.name, s);
		}
	}
	return out;
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

	skinningSetting_->isSkinning = true;
}

void AnimationModel::CreateDissolveSettingResource()
{
	// Dissolve設定用のリソースを作成
	dissolveSettingResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DissolveSetting));
	dissolveSettingResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveSetting_));
	// デフォルトのDissolve設定
	dissolveSetting_->threshold = 0.0f; // 閾値
	dissolveSetting_->edgeThickness = 2.0f; // エッジの太さ
	dissolveSetting_->edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // エッジの色
}

void AnimationModel::InitializeBones()
{
	auto& jointMap = skeleton_->GetJointMap();

	/// ---------- 頭・首 ---------- ///
	if (auto it = jointMap.find("mixamorig:Head"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "Head", it->second, -1, {0, 0.12f, 0}, 0.12f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:Neck"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "Neck", it->second, -1, {0, 0.05f, 0}, 0.1f * scaleFactor, 0.0f });
	}

	/// ---------- 腹・胸 ---------- ///
	if (jointMap.contains("mixamorig:Spine") && jointMap.contains("mixamorig:Spine1")) {
		bodyPartColliders_.push_back({ "SpineLower", jointMap.at("mixamorig:Spine"), jointMap.at("mixamorig:Spine1"), {}, 0.15f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:Spine1") && jointMap.contains("mixamorig:Spine2")) {
		bodyPartColliders_.push_back({ "SpineUpper", jointMap.at("mixamorig:Spine1"), jointMap.at("mixamorig:Spine2"), {0.0f,0.06f,0.0f}, 0.18f * scaleFactor, 0.0f });
	}

	/// ---------- 腰 ---------- ///
	if (auto it = jointMap.find("mixamorig:Hips"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "Pelvis", it->second, -1, {}, 0.16f * scaleFactor, 0.0f });
	}

	/// ---------- 左上腕・左前腕 ---------- ///
	if (jointMap.contains("mixamorig:LeftArm") && jointMap.contains("mixamorig:LeftForeArm")) {
		bodyPartColliders_.push_back({ "LeftUpperArm", jointMap.at("mixamorig:LeftArm"), jointMap.at("mixamorig:LeftForeArm"), {}, 0.1f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:LeftForeArm") && jointMap.contains("mixamorig:LeftHand")) {
		bodyPartColliders_.push_back({ "LeftLowerArm", jointMap.at("mixamorig:LeftForeArm"), jointMap.at("mixamorig:LeftHand"), {}, 0.09f * scaleFactor, 0.0f });
	}

	/// ---------- 右上腕・右前腕 ---------- ///
	if (jointMap.contains("mixamorig:RightArm") && jointMap.contains("mixamorig:RightForeArm")) {
		bodyPartColliders_.push_back({ "RightUpperArm", jointMap.at("mixamorig:RightArm"), jointMap.at("mixamorig:RightForeArm"), {}, 0.1f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:RightForeArm") && jointMap.contains("mixamorig:RightHand")) {
		bodyPartColliders_.push_back({ "RightLowerArm", jointMap.at("mixamorig:RightForeArm"), jointMap.at("mixamorig:RightHand"), {}, 0.09f * scaleFactor, 0.0f });
	}

	/// ---------- 左大腿・左下腿 ---------- ///
	if (jointMap.contains("mixamorig:LeftUpLeg") && jointMap.contains("mixamorig:LeftLeg")) {
		bodyPartColliders_.push_back({ "LeftUpperLeg", jointMap.at("mixamorig:LeftUpLeg"), jointMap.at("mixamorig:LeftLeg"), {}, 0.12f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:LeftLeg") && jointMap.contains("mixamorig:LeftFoot")) {
		bodyPartColliders_.push_back({ "LeftLowerLeg", jointMap.at("mixamorig:LeftLeg"), jointMap.at("mixamorig:LeftFoot"), {}, 0.1f * scaleFactor, 0.0f });
	}

	/// ---------- 右大腿・右下腿 ---------- ///
	if (jointMap.contains("mixamorig:RightUpLeg") && jointMap.contains("mixamorig:RightLeg")) {
		bodyPartColliders_.push_back({ "RightUpperLeg", jointMap.at("mixamorig:RightUpLeg"), jointMap.at("mixamorig:RightLeg"), {}, 0.12f * scaleFactor, 0.0f });
	}
	if (jointMap.contains("mixamorig:RightLeg") && jointMap.contains("mixamorig:RightFoot")) {
		bodyPartColliders_.push_back({ "RightLowerLeg", jointMap.at("mixamorig:RightLeg"), jointMap.at("mixamorig:RightFoot"), {}, 0.1f * scaleFactor, 0.0f });
	}

	/// ---------- 左足 ---------- ///
	if (jointMap.contains("mixamorig:LeftFoot") && jointMap.contains("mixamorig:LeftToeBase")) {
		bodyPartColliders_.push_back({ "LeftToe", jointMap.at("mixamorig:LeftFoot"), jointMap.at("mixamorig:LeftToeBase"), {}, 0.07f * scaleFactor, 0.0f });
	}

	/// ---------- 右足 ---------- ///
	if (jointMap.contains("mixamorig:RightFoot") && jointMap.contains("mixamorig:RightToeBase")) {
		bodyPartColliders_.push_back({ "RightToe", jointMap.at("mixamorig:RightFoot"), jointMap.at("mixamorig:RightToeBase"), {}, 0.07f * scaleFactor, 0.0f });
	}

	/// ---------- 左肩と右肩 ---------- ///
	if (auto it = jointMap.find("mixamorig:LeftShoulder"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "LeftShoulder",it->second, -1, {-0.08f, 0.0f, 0.0f}, 0.11f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:RightShoulder"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "RightShoulder",	it->second, -1, {0.08f, 0.0f, 0.0f}, 0.11f * scaleFactor, 0.0f });
	}

	/// ---------- 左腕・右腕 ---------- ///
	if (auto it = jointMap.find("mixamorig:LeftForeArm"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "LeftElbow", it->second, -1, {}, 0.09f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:RightForeArm"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "RightElbow", it->second, -1, {}, 0.09f * scaleFactor, 0.0f });
	}

	/// ---------- 左膝・右膝 ---------- ///
	if (auto it = jointMap.find("mixamorig:LeftLeg"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "LeftKnee", it->second, -1, {}, 0.10f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:RightLeg"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "RightKnee",	it->second, -1, {}, 0.10f * scaleFactor, 0.0f });
	}

	/// ---------- 左手首・右手首 ---------- ///
	if (auto it = jointMap.find("mixamorig:LeftHand"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "LeftWrist",	it->second, -1, {}, 0.08f * scaleFactor, 0.0f });
	}
	if (auto it = jointMap.find("mixamorig:RightHand"); it != jointMap.end()) {
		bodyPartColliders_.push_back({ "RightWrist", it->second, -1, {}, 0.08f * scaleFactor, 0.0f });
	}
}
