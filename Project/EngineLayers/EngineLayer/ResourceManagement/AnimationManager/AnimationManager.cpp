#include "AnimationManager.h"
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
void AnimationManager::Initialize(const std::string& fileName)
{
	dxCommon_ = DirectXCommon::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// モデル読み込み
	modelData = ModelManager::LoadModelFile("Resources/AnimatedCube", fileName);

	// アニメーションの読み込み
	animation = LoadAnimationFile("Resources/AnimatedCube", fileName);

	// 骨の読み込み
	skeleton = CreateSkelton(modelData.rootNode);

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
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void AnimationManager::Update()
{
	UpdateAnimation(1.0f / 60.0f);

	//animationTime_ += 1.0f / 60.0f;
	animationTime_ = std::fmod(animationTime_, animation.duration);
	ApplyAnimation(animationTime_);
	UpdateSkeleton();
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


	// ★★★ デバッグ用にJointを可視化 ★★★
	for (const auto& joint : skeleton.joints)
	{
		// ① Jointの位置に球を描画
		Wireframe::GetInstance()->DrawSphere(joint.transform.translate, 0.05f, { 1.0f, 0.0f, 0.0f, 1.0f });

		// ② 親がいる場合、親子関係を線で描画
		if (joint.parent)
		{
			const Joint& parentJoint = skeleton.joints[*joint.parent];
			Wireframe::GetInstance()->DrawLine(
				parentJoint.transform.translate,  // 親の位置
				joint.transform.translate,       // 子の位置
				{ 0.0f, 1.0f, 0.0f, 1.0f }         // 緑色
			);
		}
	}
}


/// -------------------------------------------------------------
///				　	アニメーションの更新処理
/// -------------------------------------------------------------
void AnimationManager::UpdateAnimation(float deltaTime)
{
	animationTime_ += deltaTime;
	animationTime_ = std::fmod(animationTime_, animation.duration);

	// rootNodeAnimationが存在するか確認
	auto it = animation.nodeAnimations.find(modelData.rootNode.name);
	if (it == animation.nodeAnimations.end()) {
		return; // アニメーションデータがなければ処理しない
	}

	NodeAnimation& rootNodeAnimation = it->second;

	// translate, rotate, scale の keyframes が空でないかチェック
	Vector3 translate = rootNodeAnimation.translate.empty() ? Vector3(0.0f, 0.0f, 0.0f)
		: CalculateValue(rootNodeAnimation.translate, animationTime_);
	Quaternion rotate = rootNodeAnimation.rotate.empty() ? Quaternion(0.0f, 0.0f, 0.0f, 1.0f)
		: CalculateValue(rootNodeAnimation.rotate, animationTime_);
	Vector3 scale = rootNodeAnimation.scale.empty() ? Vector3(1.0f, 1.0f, 1.0f)
		: CalculateValue(rootNodeAnimation.scale, animationTime_);

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

	wvpData->WVP = worldViewProjectionMatrix;
	wvpData->World = localMatrix * worldMatrix;
	wvpData->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(localMatrix * worldMatrix));
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


/// -------------------------------------------------------------
///				Nodeの階層構造からSkeletonを作成
/// -------------------------------------------------------------
Skeleton AnimationManager::CreateSkelton(const Node& rootNode)
{
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

	// 名前とindexのマッピングを行いアクセスしやすくする
	for (const Joint& joint : skeleton.joints)
	{
		skeleton.jointMap.emplace(joint.name, joint.index);
	}

	UpdateSkeleton();

	return skeleton;
}


/// -------------------------------------------------------------
///						NodeからJointを作成
/// -------------------------------------------------------------
int32_t AnimationManager::CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints)
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
void AnimationManager::UpdateSkeleton()
{
	// 全てのJointを更新。親が若いので通常ループで処理可能になっている
	for (Joint& joint : skeleton.joints)
	{
		joint.localMatrix = Matrix4x4::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);

		// 親がいれば親の行列を掛ける
		if (joint.parent)
		{
			joint.skeletonSpaceMatrix = skeleton.joints[*joint.parent].skeletonSpaceMatrix * joint.localMatrix;
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
void AnimationManager::ApplyAnimation(float animationTime)
{
	for (Joint& joint : skeleton.joints)
	{
		if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end())
		{
			const NodeAnimation& rootNodeAnimation = (*it).second;

			// 位置データ
			if (!rootNodeAnimation.translate.empty()) {
				joint.transform.translate = CalculateValue(rootNodeAnimation.translate, animationTime);
			}
			else {
				joint.transform.translate = Vector3(0.0f, 0.0f, 0.0f); // ★デフォルト値
			}

			// 回転データ
			if (!rootNodeAnimation.rotate.empty()) {
				joint.transform.rotate = CalculateValue(rootNodeAnimation.rotate, animationTime);
			}
			else {
				joint.transform.rotate = Quaternion(0.0f, 0.0f, 0.0f, 1.0f); // ★デフォルト値
			}

			// スケールデータ
			if (!rootNodeAnimation.scale.empty()) {
				joint.transform.scale = CalculateValue(rootNodeAnimation.scale, animationTime);
			}
			else {
				joint.transform.scale = Vector3(1.0f, 1.0f, 1.0f); // ★デフォルト値
			}
		}
	}
}
