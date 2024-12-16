#include "ParticleManager.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <MatrixMath.h>
#include <LogString.h>
#include <SRVManager.h>
#include <TextureManager.h>
#include <BlendModeType.h>

/// -------------------------------------------------------------
///				    シングルトンインスタンス
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
	/*
	* 引数でDirectXCommonとSRVマネージャーのポインタを受け取ってメンバ変数に記録する
	* ランダムエンジンの初期化
	* パイプライン生成
	* 頂点データの初期化（座標など）
	* 頂点リソース生成
	* 頂点バッファビュー（VBV）作成
	* 頂点リソースに頂点データを書き込む　
	*/
	// 引数でDirectXCommonとSRVマネージャーのポインタを受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	camera_ = camera;

	// ランダムエンジンの初期化
	std::random_device seed;
	randomEngin.seed(seed());

	// パイプラインの生成
	pipelineManager_ = std::make_unique<PipelineStateManager>();
	pipelineManager_->Initialize(dxCommon, gParticle, BlendMode::kBlendModeAdd);

	// 頂点データの初期化
	InitializeVertexData();

	// 頂点リソースの生成
	vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * (modelData.vertices.size()));

	// 頂点バッファビューの生成
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};											 // 頂点バッファビューを作成する
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();				 // リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * (modelData.vertices.size()));	 // 使用するリソースのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);									 // 1頂点あたりのサイズ

	// 頂点リソースに頂点データを書き込む
	VertexData* vertexData = nullptr; // 頂点リソースにデータを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	// モデルデータの頂点データをコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
	// アンマップ
	vertexResource->Unmap(0, nullptr);
}


/// -------------------------------------------------------------
///				    パーティクルグループの生成
/// -------------------------------------------------------------
void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
	/*
	* 登録済みの名前かチェックしてassert
	* 新たな空のパーティクルグループを作成し、コンテナに登録
	* 新たなパーティクルグループの
	* ・マテリアルデータにテクスチャファイルパスを設定
	* ・テクスチャを読み込む
	* ・マテリアルデータにテクスチャのSRVインデックスを記録
	* ・インスタンシング用リソースの生成
	* ・インスタンシング用にSRVを確保してSRVインデックスを記録
	* ・SRV生成（StructuredBuffer用設定）
	*/

	// 登録済みの名前かチェック
	assert(particleGroups.find(name) == particleGroups.end() && "Particle group alread exests!");

	// 新たに空のパーティクルグループを作成
	ParticleGroup group{};

	// マテリアルデータにテクスチャファイルパスを設定
	group.materialData.textureFilePath = textureFilePath;

	// テクスチャを読み込む
	TextureManager::GetInstance()->LoadTexture(group.materialData.textureFilePath);

	// マテリアルデータにテクスチャのSRVインデックスを記録
	group.materialData.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(group.materialData.textureFilePath);

	// インスタンシング用リソースの生成
	group.instancebuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kNumMaxInstance);
	group.instancebuffer->Map(0, nullptr, reinterpret_cast<void**>(&group.mappedData));

	// インスタンシング用にSRVを確保してSRVインデックスを記録
	group.srvIndex = srvManager_->Allocate();

	// SRV生成（StructuredBuffer用設定）
	srvManager_->CreateSRVForStructureBuffer(group.srvIndex, group.instancebuffer.Get(), kNumMaxInstance, sizeof(ParticleForGPU));

	// コンテナに登録
	particleGroups[name] = group;
}


/// -------------------------------------------------------------
///				           　更新処理
/// -------------------------------------------------------------
void ParticleManager::Update()
{
	/*
	* ビルボード行列の計算
	* ビュー行列とプロジェクション行列をカメラから取得
	* すべてのパーティクルグループについて処理する
	* ・グループ内のすべてのパーティクルについて処理する
	* -寿命に達していたらグループから葉ずつ
	* -場の影響を計算（加算）
	* -移動処理（速度を座標に加算）
	* -経過時間を加算
	* -ワールド行列を計算
	* -ワールドビュープロジェクション行列を合成
	* -インスタンシング用データ１個分の書き込み
	*/

	for (auto& [name, group] : particleGroups)
	{
		UpdateParticles(group);
	}
}


/// -------------------------------------------------------------
///				           　描画処理
/// -------------------------------------------------------------
void ParticleManager::Draw()
{
	/*
	* コマンド：ルートシグネチャを設定
	* コマンド：PSO（Pipeline State Object）を設定
	* コマンド：プリミティブトポロジー描画形状）を設定
	* コマンド：VBV（Vertex Buffer View）を設定
	* すべてのパーティクルグループについて処理する
	* ・コマンド：テクスチャのSRVのDescriptorTableを設定
	* ・コマンド：インスタンシングデータのSRVのDescriptorTableを設定
	* ・コマンド：DrawCall（インスタンシング描画）
	*/


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
	auto& particles = group.particles;
	auto mappedData = group.mappedData;

	// 寿命処理と物理演算
	uint32_t index = 0;
	for (auto it = particles.begin(); it != particles.end();) {
		it->currentTime += 1.0f / 60.0f;
		if (it->currentTime >= it->lifeTime) {
			it = particles.erase(it);
		}
		else {
			it->transform.translate += it->velocity;
			mappedData[index].World = MakeAffineMatrix(it->transform.scale, it->transform.rotate, it->transform.translate);
			mappedData[index].color = it->color;
			++it;
			++index;
		}
	}
	group.numParticles = index;
}



void ParticleManager::DrawParticleGroup(const ParticleGroup& group)
{
	auto commandList = dxCommon_->GetCommandList();

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	auto srvHandle = srvManager_->GetGPUDescriptorHandle(group.srvIndex);
	commandList->SetGraphicsRootDescriptorTable(1, srvHandle);
	commandList->DrawInstanced(6, group.numParticles, 0, 0);
}
