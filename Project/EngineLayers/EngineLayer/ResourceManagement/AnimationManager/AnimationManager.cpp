#include "AnimationManager.h"
#include "ModelManager.h"
#include <TextureManager.h>
#include <DirectXCommon.h>
#include <Object3DCommon.h>
#include <SRVManager.h>
#include <ResourceManager.h>


/// -------------------------------------------------------------
///				　		 初期化処理
/// -------------------------------------------------------------
void AnimationManager::Initialize(const std::string& fileName)
{
	dxCommon_ = DirectXCommon::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// モデル読み込み
	modelData = ModelManager::LoadModelFile("Resources/AnimatedCube", fileName);

	animation = LoadAnimationFile("Resources/AnimatedCube", fileName);

	// .objファイルの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);

	// 読み込んだテクスチャ番号を取得
	modelData.material.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureFilePath);


#pragma region WVP行列データを格納するバッファリソースを生成し初期値として単位行列を設定
	//WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	wvpResource = ResourceManager::CreateBufferResource(DirectXCommon::GetInstance()->GetDevice(), sizeof(TransformationMatrix));

	//書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込んでおく
	wvpData->World = Matrix4x4::MakeIdentity();
	wvpData->WVP = Matrix4x4::MakeIdentity();
	wvpData->WorldInversedTranspose = Matrix4x4::MakeIdentity();
#pragma endregion


#pragma region マテリアル用のリソースを作成しそのリソースにデータを書き込む処理を行う
	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 今回は赤を書き込んでみる
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = true; // 平行光源を有効にする
	materialData->shininess = 1.0f;
	materialData->uvTransform = Matrix4x4::MakeIdentity();
#pragma endregion


#pragma region 頂点バッファデータの開始位置サイズおよび各頂点のデータ構造を指定
	// 頂点バッファビューを作成する
	vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * (modelData.vertices.size() + TotalVertexCount));
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();									 // リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * (modelData.vertices.size() + TotalVertexCount));	 // 使用するリソースのサイズ
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
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void AnimationManager::Update()
{
	animationTime_ += 1.0f / 60.0f;
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
///				　			描画処理
/// -------------------------------------------------------------
void AnimationManager::Draw()
{
	auto commandList = dxCommon_->GetCommandList();

	// 定数バッファビュー (CBV) とディスクリプタテーブルの設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // モデル用VBV
	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, modelData.material.gpuHandle);
	commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());

	// モデルの描画
	commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
}


/// -------------------------------------------------------------
///				　アニメーションファイルを読み込む
/// -------------------------------------------------------------
Animation AnimationManager::LoadAnimationFile(const std::string& directoryPath, const std::string& fileName)
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
