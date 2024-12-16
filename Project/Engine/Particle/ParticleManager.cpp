#include "ParticleManager.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <MatrixMath.h>
#include <LogString.h>
#include <SRVManager.h>
#include <TextureManager.h>

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
void ParticleManager::Initialize(DirectXCommon* dxCommon, SRVManager* srvManager)
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

	// ランダムエンジンの初期化
	std::random_device seed;
	randomEngin.seed(seed());
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
	group.textureFilePath = textureFilePath;

	// テクスチャを読み込む
	TextureManager::GetInstance()->LoadTexture(group.textureFilePath);

	// インスタンシング用リソースの生成
	group.instancebuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kNumMaxInstance);
	group.instancebuffer->Map(0, nullptr, reinterpret_cast<void**>(&group.mappedData));

	// インスタンシング用にSRVを確保してSRVインデックスを記録
	group.srvIndex = srvManager_->Allocate();

	// 初期化
	for (uint32_t i = 0; i < kNumMaxInstance; ++i)
	{
		group.mappedData[i].WVP = MakeIdentity();
		group.mappedData[i].World = MakeIdentity();
	}

	// SRV生成（StructuredBuffer用設定）
	//srvManager_->CreateSRVForStructureBuffer(group.srvIndex, group.instancebuffer.Get(), numElements, structuredByteStride);

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
