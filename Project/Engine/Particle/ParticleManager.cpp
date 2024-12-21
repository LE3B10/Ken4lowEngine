#include "ParticleManager.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <MatrixMath.h>
#include <LogString.h>
#include <SRVManager.h>
#include <TextureManager.h>
#include <BlendModeType.h>

/// -------------------------------------------------------------
///				パーティクルマネージャークラスの実装
/// -------------------------------------------------------------
ParticleManager* ParticleManager::GetInstance()
{
	static ParticleManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///				           初期化処理
/// -------------------------------------------------------------
void ParticleManager::Initialize(DirectXCommon* dxCommon, SRVManager* srvManager, Camera* camera)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	camera_ = camera;

	// ランダムエンジンを初期化
	std::random_device seed;
	randomEngin.seed(seed());

	// パイプラインを初期化
	pipelineManager_ = std::make_unique<PipelineStateManager>();
	pipelineManager_->Initialize(dxCommon, gParticle, BlendMode::kBlendModeAdd);

	// 頂点データを初期化
	InitializeVertexData();

	// 頂点リソースを作成
	vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * modelData.vertices.size());

	// 頂点バッファビューを初期化
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点データをリソースにマッピング
	VertexData* vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
	vertexResource->Unmap(0, nullptr);

	// マテリアル用リソースを作成
	materialResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(Material));
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色
	materialData->enableLighting = true; // ライティングを有効化
	materialData->uvTransform = MakeIdentity(); // 単位行列を設定
	materialResource->Unmap(0, nullptr);

	// WVP用リソースを作成
	wvpResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(TransformationMatrix));
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	wvpData->World = MakeIdentity(); // ワールド行列
	wvpData->WVP = MakeIdentity(); // WVP行列

	// インスタンシング用リソースを作成
	instancingResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(ParticleForGPU) * kNumMaxInstance);
	ParticleForGPU* instancingData = nullptr;
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));
	for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
		instancingData[i].WVP = MakeIdentity(); // WVP行列を単位行列に初期化
		instancingData[i].World = MakeIdentity(); // ワールド行列を単位行列に初期化
	}
}


/// -------------------------------------------------------------
///				    パーティクルグループの生成
/// -------------------------------------------------------------
void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
	// 既存の名前でないことを確認
	assert(particleGroups.find(name) == particleGroups.end() && "Particle group already exists!");

	// 新しいパーティクルグループを作成
	ParticleGroup group;

	// テクスチャ情報を設定
	group.materialData.textureFilePath = textureFilePath;
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	group.materialData.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath);

	// インスタンシング用のリソースを作成
	group.instancebuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kNumMaxInstance);
	group.instancebuffer->Map(0, nullptr, reinterpret_cast<void**>(&group.mappedData));

	// SRVを確保して設定
	group.srvIndex = srvManager_->Allocate();
	group.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(group.srvIndex);
	group.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(group.srvIndex);

	srvManager_->CreateSRVForStructureBuffer(group.srvIndex, group.instancebuffer.Get(), kNumMaxInstance, sizeof(ParticleForGPU));

	// グループを登録
	particleGroups[name] = group;
}


/// -------------------------------------------------------------
///				           　更新処理
/// -------------------------------------------------------------
void ParticleManager::Update()
{
	// カメラ行列とビュー・プロジェクション行列を計算
	Matrix4x4 cameraMatrix = MakeAffineMatrix(camera_->GetScale(), camera_->GetRotation(), camera_->GetTranslate());
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	for (auto& [name, group] : particleGroups) {
		UpdateParticles(group); // パーティクルグループごとに更新
	}
}


/// -------------------------------------------------------------
///				           　描画処理
/// -------------------------------------------------------------
void ParticleManager::Draw()
{
	auto* commandList = dxCommon_->GetCommandList();

	for (const auto& [name, group] : particleGroups)
	{
		DrawParticleGroup(group);
	}
}

Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate)
{
	Particle particle;

	// 一様分布生成器を使って乱数を生成
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

	// 位置と速度を[-1, 1]でランダムに初期化
	particle.transform = {
		{ 1.0f, 1.0f, 1.0 },
		{ 0.0f, 0.0f, 0.0f },
		{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) }
	};

	// 発生場所を計算
	Vector3 randomTranslate{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	particle.transform.translate = translate + randomTranslate;

	// 色を[0, 1]でランダムに初期化
	particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine) };

	// パーティクル生成時にランダムに1秒～3秒の間生存
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0;
	particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };

	return particle;
}



void ParticleManager::Emit(const Emitter& emitter, std::mt19937& randomEngine)
{
	//// パーティクルグループが存在するか確認
	//auto groupIt = particleGroups.find(emitter.groupName);
	//if (groupIt == particleGroups.end()) {
	//	assert(false && "Particle group not found!");
	//	return;
	//}

	//ParticleGroup& group = groupIt->second;

	//// エミッターの設定に従ってパーティクルを生成
	//for (uint32_t i = 0; i < emitter.numParticles; ++i) {
	//	// 新しいパーティクルを生成
	//	Particle newParticle = MakeNewParticle(randomEngine, emitter.spawnPosition);

	//	// パーティクルをグループに追加
	//	group.particles.push_back(newParticle);

	//	// パーティクル数が上限を超えた場合は古いものを削除
	//	if (group.particles.size() > kNumMaxInstance) {
	//		group.particles.pop_front();
	//	}
	//}

	//// GPU用データを更新
	//uint32_t index = 0;
	//for (const auto& particle : group.particles) {
	//	if (index >= kNumMaxInstance) break; // 上限に達したら終了

	//	// インスタンシング用のデータに書き込む
	//	group.mappedData[index].World = MakeAffineMatrix(
	//		particle.transform.scale, particle.transform.rotate, particle.transform.translate);
	//	group.mappedData[index].color = particle.color;
	//	++index;
	//}

	//group.numParticles = index; // 現在のパーティクル数を記録
}


void ParticleManager::InitializeVertexData()
{
	// 6つの頂点を定義して四角形を表現
	modelData.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });  // 左上
	modelData.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelData.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下

	modelData.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下
	modelData.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelData.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右下


	// テクスチャの設定
	modelData.material.textureFilePath = "./Resources/particle.png";

}




void ParticleManager::UpdateParticles(ParticleGroup& group)
{
	// 各パーティクルを更新
	for (auto it = group.particles.begin(); it != group.particles.end();)
	{
		// パーティクルの寿命を更新
		it->currentTime += 1.0f / 60.0f; // フレームレートに応じた時間の加算
		if (it->currentTime >= it->lifeTime)
		{
			// 寿命が尽きたら削除
			it = group.particles.erase(it);
			continue;
		}

		// パーティクルの移動（速度を適用）
		it->transform.translate += it->velocity;
		
		// 必要なrあ色やスケールを徐々に変更
		float lifeRatio = it->currentTime / it->lifeTime; // 残り寿命の割合
		it->color.w = 1.0f - lifeRatio;					  // α値を現象

		// 次のパーティクルに進む
		++it;
	}

	// GPU用データの更新
	uint32_t index = 0;
	for (const auto& particle : group.particles)
	{
		if (index >= kNumMaxInstance)
		{
			// 上限を超えたら終了
			break;
		}

		group.mappedData[index].World = MakeAffineMatrix(particle.transform.scale, particle.transform.rotate, particle.transform.translate);
		group.mappedData[index].color = particle.color;
		++index;
	}

	// 現在のパーティクル数を記録
	group.numParticles = index;
}



void ParticleManager::DrawParticleGroup(const ParticleGroup& group)
{
	// 描画がない場合は描画しない
	if (group.numParticles == 0)
	{
		return;
	}

	// パイプラインステートとルートシグネチャを設定
	auto* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(pipelineManager_->GetRootSignature()->GetRootSignature(gParticle));
	commandList->SetPipelineState(pipelineManager_->GetPipelineState());

	// 頂点バッファを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // トポロジを設定

	// マテリアル（定数バッファビュー）の設定
	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

	// テクスチャのSRV（シェーダーリソースビュー）を設定
	commandList->SetGraphicsRootDescriptorTable(1, group.materialData.gpuHandle);

	// インスタンシングデータのSRVを設定
	commandList->SetGraphicsRootDescriptorTable(2, group.srvHandleGPU);

	// インスタンシング描画コマンドを発行
	commandList->DrawInstanced(6, group.numParticles, 0, 0);
}
