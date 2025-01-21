#pragma once
#include <DX12Include.h>
#include <ModelData.h>
#include <Material.h>
#include <TransformationMatrix.h>
#include <VertexData.h>
#include <DirectionalLight.h>
#include <Emitter.h>
#include <Particle.h>
#include "BlendModeType.h"

#include <unordered_map>
#include <list>
#include <random>
#include <numbers>



/// ---------- 前方宣言 ----------///
class DirectXCommon;
class SRVManager;
class Camera;
class ShaderManager;

// Δt を定義。とりあえず60fps固定してあるが、実時間を計測して可変fpsで動かせるようにする
const float kDeltaTime = 1.0f / 60.0f;


/// -------------------------------------------------------------
///				パーティクルマネージャークラス
/// -------------------------------------------------------------
class ParticleManager
{
public: /// ---------- 構造体 ---------- ///

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

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, Camera* camera);

	// パーティクルグループの生成
	void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 終了処理
	void Finalize();

	// パーティクルの発生
	void Emit(const std::string name, const Vector3 position, uint32_t count);

	std::unordered_map<std::string, ParticleManager::ParticleGroup> GetParticleGroups() { return particleGroups; }

private: /// ---------- ヘルパー関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// PSOを生成
	void CreatePSO();

	// 頂点データの初期化
	void InitializeVertexData();

	// マテリアルデータの初期化
	void InitializeMaterialData();

	// パーティクル生成器
	Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate);

private: /// ---------- メンバ変数 ---------- ///
	
	BlendMode cuurenttype = BlendMode::kBlendModeAdd;

	DirectXCommon* dxCommon_ = nullptr;
	SRVManager* srvManager_ = nullptr;
	Camera* camera_ = nullptr;
	ShaderManager* shaderManager = nullptr;

	ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	ComPtr <ID3D12Resource> materialResource;
	ComPtr <ID3D12Resource> vertexResource;

	// モデルの読み込み
	ModelData modelData;

	VertexData* vertexData = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	// パーティクルグループコンテナ
	std::unordered_map<std::string, ParticleGroup> particleGroups;

	// ランダムエンジン
	std::random_device seedGeneral;
	std::mt19937 randomEngin;

	// 描画数
	const uint32_t kNumMaxInstance = 128;

	bool useBillboard = true;

	// 分割数
	uint32_t kSubdivision = 32;

	// 緯度・経度の分割数に応じた角度の計算
	float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);
	float kLonEvery = 2.0f * std::numbers::pi_v<float> / float(kSubdivision);

	// 球体の頂点数の計算
	uint32_t TotalVertexCount = kSubdivision * kSubdivision * 6;

private: /// ---------- コピー禁止 ---------- ///
	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;
};

