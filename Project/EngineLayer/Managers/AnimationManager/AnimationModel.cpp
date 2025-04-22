#include "AnimationModel.h"
#include "ModelManager.h"
#include <TextureManager.h>
#include <DirectXCommon.h>
#include <Object3DCommon.h>
#include <SRVManager.h>
#include <ResourceManager.h>
#include <Wireframe.h>


/// -------------------------------------------------------------
///				　		 初期化処理
/// -------------------------------------------------------------
void AnimationModel::Initialize(const std::string& fileName, bool isAnimation, bool hasSkeleton)
{
	dxCommon_ = DirectXCommon::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();
	wireframe_ = Wireframe::GetInstance();

	isAnimation_ = isAnimation;
	hasSkeleton_ = hasSkeleton;

	// モデル読み込み
	modelData = ModelManager::GetInstance()->LoadModelFile("Resources/AnimatedCube", fileName);

	// アニメーションの読み込み
	if (isAnimation_)
	{
		animation = LoadAnimationFile("Resources/AnimatedCube", fileName);
	}

	// 骨の読み込み
	if (hasSkeleton_)
	{
		skeleton = CreateSkelton(modelData.rootNode);
		skinCluster = CreateSkinCluster(skeleton, modelData, SRVManager::GetInstance()->GetDescriptorSize());
	}

	// .objファイルの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);

	// 読み込んだテクスチャ番号を取得
	modelData.material.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureFilePath);

	// マテリアルデータの初期化処理
	material_.Initialize();



#pragma region 頂点バッファデータの開始位置サイズおよび各頂点のデータ構造を指定
	// 頂点バッファビューを作成する
	vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * (modelData.vertices.size()));
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();									 // リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * (modelData.vertices.size()));						 // 使用するリソースのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);														 // 1頂点あたりのサイズ

	VertexData* vertexData = nullptr;																			 // 頂点リソースにデータを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));										 // 書き込むためのアドレスを取得

	// モデルデータの頂点データをコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	// アンマップ
	vertexResource->Unmap(0, nullptr);
#pragma endregion


	// カメラ用のリソースを作る
	cameraResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU));
	// 書き込むためのアドレスを取得
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// カメラの初期位置
	cameraData->worldPosition = camera_->GetTranslate();

	// インデックスデータを作成
	indexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	uint32_t* indexData = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	std::memcpy(indexData, modelData.indices.data(), sizeof(uint32_t) * modelData.indices.size());
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void AnimationModel::Update()
{
	// FPSの取得 deltaTimeの計算
	float deltaTime = 1.0f / dxCommon_->GetFPSCounter().GetFPS();

	if (isAnimation_ && !hasSkeleton_)
	{
		//UpdateAnimation(deltaTime);
	}

	if (isAnimation_ && hasSkeleton_)
	{
		animationTime_ += deltaTime;
		animationTime_ = std::fmod(animationTime_, animation.duration);
		ApplyAnimation(animationTime_);
		UpdateSkeleton(skeleton);
		UpdateSkinCluster(skinCluster, skeleton);
	}

	// 骨のワイヤーフレームを更新
	if (hasSkeleton_ && wireframe_)
	{
		for (const Joint& joint : skeleton.joints)
		{
			if (joint.parent)
			{
				Vector3 parentPos = skeleton.joints[*joint.parent].skeletonSpaceMatrix.GetTranslation();
				Vector3 jointPos = joint.skeletonSpaceMatrix.GetTranslation();
				wireframe_->DrawLine(parentPos, jointPos, { 1.0f, 1.0f, 0.0f, 1.0f });
			}
		}
	}

	material_.Update();
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void AnimationModel::Draw()
{
	auto commandList = dxCommon_->GetCommandList();

	D3D12_VERTEX_BUFFER_VIEW vbvs[2] =
	{
		vertexBufferView,				// VertexDataのVBV
		skinCluster.influenceBufferView // インフルエンスのVBV
	};
	// 配列を渡す（開始Slot番号、使用Slot数、VBVへのポインタ）
	commandList->IASetVertexBuffers(0, 2, vbvs); // モデル用VBV
	
	commandList->IASetIndexBuffer(&indexBufferView);

	material_.SetPipeline();

	commandList->SetGraphicsRootDescriptorTable(2, modelData.material.gpuHandle);
	commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());

	// モデルの描画
	commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
}


/// -------------------------------------------------------------
///				　	アニメーションの更新処理
/// -------------------------------------------------------------
void AnimationModel::UpdateAnimation(float deltaTime)
{
	animationTime_ += deltaTime;
	animationTime_ = std::fmod(animationTime_, animation.duration);
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

	wvpData->WVP = localMatrix * worldViewProjectionMatrix;
	wvpData->World = localMatrix * worldMatrix;
	wvpData->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(localMatrix * worldMatrix));
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

		// 位置（transform）のキーフレームを追加
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


/// -------------------------------------------------------------
///				Nodeの階層構造からSkeletonを作成
/// -------------------------------------------------------------
Skeleton AnimationModel::CreateSkelton(const Node& rootNode)
{
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

	// 名前とindexのマッピングを行いアクセスしやすくする
	for (const Joint& joint : skeleton.joints)
	{
		skeleton.jointMap.emplace(joint.name, joint.index);
	}

	UpdateSkeleton(skeleton);

	return skeleton;
}


/// -------------------------------------------------------------
///						NodeからJointを作成
/// -------------------------------------------------------------
int32_t AnimationModel::CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints)
{
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = Matrix4x4::MakeIdentity();
	joint.transform = node.transform;
	joint.index = int32_t(joints.size()); // 現在登録されている数をIndexに代入
	joint.parent = parent;
	joints.push_back(joint); // SkeletonのJoint列に追加
	for (const Node& child : node.children)
	{
		// 子Jointを作成し、そのIndexを登録
		int32_t childIndex = CreateJoint(child, joint.index, joints);
		joints[joint.index].children.push_back(childIndex);
	}

	// 自身のIndexを返す
	return joint.index;
}


/// -------------------------------------------------------------
///						Skeletonの更新処理
/// -------------------------------------------------------------
void AnimationModel::UpdateSkeleton(Skeleton& skeleton)
{
	// 全てのJointを更新。親が若いので通常ループで処理可能になっている
	for (Joint& joint : skeleton.joints)
	{
		// **スケールを適用**
		joint.localMatrix = Matrix4x4::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);

		// 親がいれば親の行列を掛ける
		if (joint.parent)
		{
			joint.skeletonSpaceMatrix = Matrix4x4::Multiply(joint.localMatrix, skeleton.joints[*joint.parent].skeletonSpaceMatrix);
		}
		else
		{
			// 親がいないのでlocalMatrixとskeletonSpaceMatrixは一致する
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}
}


/// -------------------------------------------------------------
///				　アニメーションを適用させる処理
/// -------------------------------------------------------------
void AnimationModel::ApplyAnimation(float animationTime)
{
	for (Joint& joint : skeleton.joints)
	{
		if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end())
		{
			const NodeAnimation& rootNodeAnimation = (*it).second;

			// 位置データ
			if (!rootNodeAnimation.translate.empty())
			{
				joint.transform.translate = CalculateValue(rootNodeAnimation.translate, animationTime);
			}
			else
			{
				joint.transform.translate = Vector3(0.0f, 0.0f, 0.0f); // ★デフォルト値
			}

			// 回転データ
			if (!rootNodeAnimation.rotate.empty())
			{
				joint.transform.rotate = CalculateValue(rootNodeAnimation.rotate, animationTime);
			}
			else
			{
				joint.transform.rotate = Quaternion(0.0f, 0.0f, 0.0f, 1.0f); // ★デフォルト値
			}

			// スケールデータ
			if (!rootNodeAnimation.scale.empty())
			{
				joint.transform.scale = CalculateValue(rootNodeAnimation.scale, animationTime);
			}
			else
			{
				joint.transform.scale = Vector3(1.0f, 1.0f, 1.0f); // ★デフォルト値
			}
		}
	}
}

SkinCluster AnimationModel::CreateSkinCluster(const Skeleton& skeleton, const ModelData& modelData, uint32_t descriptorSize)
{
	SkinCluster skinCluster{};
	// palette用のResourceを確保
	skinCluster.paletteResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(WellForGPU) * skeleton.joints.size());
	WellForGPU* mappedPalette = nullptr;
	skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	skinCluster.mappedPalette = { mappedPalette, skeleton.joints.size() }; // spanを使ってアクセスするようにする
	skinCluster.paletteSrvHandle.first = SRVManager::GetInstance()->GetCPUDescriptorHandle(descriptorSize);
	skinCluster.paletteSrvHandle.second = SRVManager::GetInstance()->GetGPUDescriptorHandle(descriptorSize);

	// palette用のSRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
	paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	paletteSrvDesc.Buffer.FirstElement = 0;
	paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	paletteSrvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
	paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
	dxCommon_->GetDevice()->CreateShaderResourceView(skinCluster.paletteResource.Get(), &paletteSrvDesc, skinCluster.paletteSrvHandle.first);

	// influence用のResourceを確保
	skinCluster.influenceResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexInfluence) * modelData.vertices.size());
	VertexInfluence* mappedinfluence = nullptr;
	skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedinfluence));
	std::memset(mappedinfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size());
	skinCluster.mappedInfluence = { mappedinfluence,modelData.vertices.size() };

	// Influence用のVBVを作成
	skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
	skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
	skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

	// InverseBindPoseMatrixの保存領域を作成
	skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
	//std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), Matrix4x4::MakeIdentity());

	// ModelDataのSkinCluster情報を解析してInfluenceの中身を埋める
	for (const auto& jointWeight : modelData.skinClusterData) // ModelのSkinClusterの情報を解析
	{
		auto it = skeleton.jointMap.find(jointWeight.first); // jointWeight.firstはjoint名なので、skeletonに対象となるjointが含まれているか判断
		if (it == skeleton.jointMap.end()) continue; // そんな名前のJointは存在しない、なので次にまわす

		// (*it).secondにはjointのindexが入っているので、該当のindexのinverseBindPoseMatrixを代入
		skinCluster.inverseBindPoseMatrices[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
		for (const auto& vertexWeight : jointWeight.second.vertexWeights)
		{
			auto& currentInfluence = skinCluster.mappedInfluence[vertexWeight.vertexIndex]; // 外套のvertexIndexのinfluence情報を参照しておく
			for (uint32_t index = 0; index < kNumMaxInfluence; ++index) // 空いているところに入れる
			{
				if (currentInfluence.weights[index] == 0.0f) // weight == 0が空いている状態なので、その場所にweightとjointのindexを代入
				{
					currentInfluence.weights[index] = vertexWeight.weight;
					currentInfluence.jointIndices[index] = (*it).second;
					break;
				}
			}
		}
	}

	return skinCluster;
}

void AnimationModel::UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton)
{
	for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex)
	{
		assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());
		skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix =
			skinCluster.inverseBindPoseMatrices[jointIndex] * skeleton.joints[jointIndex].skeletonSpaceMatrix;
		skinCluster.mappedPalette[jointIndex].skeletonSpaceInverceTransposeMatrix =
			Matrix4x4::Transpose(Matrix4x4::Inverse(skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix));
	}
}
