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
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void AnimationModel::Update()
{
	// FPSの取得 deltaTimeの計算
	float deltaTime = 1.0f / dxCommon_->GetFPSCounter().GetFPS();
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
