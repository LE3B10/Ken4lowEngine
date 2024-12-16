#pragma once
#include <DX12Include.h>
#include <ModelData.h>
#include <Material.h>
#include <TransformationMatrix.h>
#include <VertexData.h>
#include <DirectionalLight.h>
#include <Emitter.h>
#include <Particle.h>
#include "CameraManager.h"

#include "PipelineStateManager.h"

#include <unordered_map>
#include <list>
#include <random>

/// ---------- 前方宣言 ----------///
class DirectXCommon;
class SRVManager;

// 円周率
#define pi 3.141592653589793238462643383279502884197169399375105820974944f

// 描画数
const uint32_t kNumMaxInstance = 100;


/// -------------------------------------------------------------
///				パーティクルマネージャークラス
/// -------------------------------------------------------------
class ParticleManager
{
private: /// ---------- 構造体 ---------- ///

	struct ParticleForGPU
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
	};

	struct ParticleGroup
	{
		// マテリアルデータ(テクスチャファイルとテクスチャ用SRVインデックス)
		MaterialData materialData;
		// パーティクルのリスト(std::list<Particle>型)
		uint32_t srvIndex;
		// インスタンシングデータ用SRVインデックス
		ComPtr<ID3D12Resource> instancebuffer;
		// インスタンシングリソース
		ParticleForGPU* mappedData;
		// インスタンス数
		uint32_t numParticles = 0;
		// インスタンシングデータを書き込むためのポインタ
		std::list<Particle> particles;
	};

public: /// ---------- メンバ関数 ---------- ///

	//  シングルトンインスタンス
	static ParticleManager* GetInstance();

	ParticleManager() = default;

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, SRVManager* srvManager, Camera* camera);

	// パーティクルグループの生成
	void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// パーティクル生成関数
	Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate);

	// パーティクルを射出する関数
	void Emit(const Emitter& emitter, std::mt19937& randomEngine);

private: /// ---------- メンバ関数 ---------- ///

	// 頂点データの初期化
	void InitializeVertexData();

	void UpdateParticles(ParticleGroup& group);
	void DrawParticleGroup(const ParticleGroup& group);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	SRVManager* srvManager_ = nullptr;
	Camera* camera_;

	ModelData modelData;

	ComPtr <ID3D12Resource> vertexResource;

	std::unique_ptr<PipelineStateManager> pipelineManager_;

	// パーティクルグループコンテナ
	std::unordered_map<std::string, ParticleGroup> particleGroups;
	std::mt19937 randomEngin;

};

