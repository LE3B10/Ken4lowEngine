#pragma once
#include <DX12Include.h>
#include <ModelData.h>
#include <Emitter.h>
#include <ParticleMaterial.h>
#include <Particle.h>
#include <ParticleMesh.h>
#include "ParticleFactory.h" // 追加

#include <unordered_map>
#include <list>
#include <random>
#include <numbers>
#include <AABB.h>

/// ---------- 前方宣言 ----------///
class DirectXCommon;
class SRVManager;
class Camera;

// Δt を定義。とりあえず60fps固定してあるが、実時間を計測して可変fpsで動かせるようにする
const float kDeltaTime = 1.0f / 60.0f;


/// -------------------------------------------------------------
///				パーティクルマネージャークラス
/// -------------------------------------------------------------
class ParticleManager
{
public: /// ---------- 構造体 ---------- ///

	/// ---------- 風のエフェクト ---------- ///
	struct WindZone
	{
		AABB area;		  // 風が吹くエリア
		Vector3 strength; // 風の強さ
	};

	struct AccelerationField
	{
		Vector3 acceleration; // !< 加速度
		AABB area;			  // !< 範囲
	};

	struct ParticleForGPU
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
	};

	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct ParticleGroup
	{
		// マテリアルデータ(テクスチャファイルとテクスチャ用SRVインデックス)
		ParticleMaterial materialData;
		// パーティクルのリスト(std::list<Particle>型)
		uint32_t srvIndex;
		// インスタンシングデータ用SRVインデックス
		ComPtr<ID3D12Resource> instancebuffer;
		// インスタンシングリソース
		ParticleForGPU* mappedData = nullptr;
		// インスタンス数
		uint32_t numParticles = 0;
		// インスタンシングデータを書き込むためのポインタ
		std::list<Particle> particles;
		// パーティクルの種別
		ParticleEffectType type = ParticleEffectType::Default;
	};

public: /// ---------- メンバ関数 ---------- ///

	//  シングルトンインスタンス
	static ParticleManager* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, Camera* camera);

	// パーティクルグループの生成
	void CreateParticleGroup(const std::string& name, const std::string& textureFilePath, ParticleEffectType effectType);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 終了処理
	void Finalize();

	// パーティクルの発生
	void Emit(const std::string name, const Vector3 position, uint32_t count, ParticleEffectType type);

	std::unordered_map<std::string, ParticleManager::ParticleGroup> GetParticleGroups() { return particleGroups; }

	// ImGuiの描画
	void DrawImGui();

	// デバッグカメラの有無
	void SetDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

	// デバッグカメラの有無を取得
	bool GetDebugCamera() { return isDebugCamera_; }

	// ビルボードを有効にするかを設定
	void SetBillboard(bool isBillboard) { useBillboard = isBillboard; }

	// ビルボードを有効にするかを取得
	bool GetBillboard() { return useBillboard; }

	// パーティクルエフェクトの種類を取得
	ParticleEffectType GetGroupType(const std::string& name)
	{
		auto it = particleGroups.find(name);
		if (it != particleGroups.end())
		{
			return it->second.type;
		}

		return ParticleEffectType::Default;
	}

	// パーティクルグループを取得
	ParticleGroup& GetGroup(const std::string& name)
	{
		auto it = particleGroups.find(name);
		if (it != particleGroups.end())
		{
			return it->second;
		}
		throw std::runtime_error("Particle group not found: " + name);
	}

private: /// ---------- ヘルパー関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// PSOを生成
	void CreatePSO();

	std::list<Particle> Emit(const Emitter& emitter, std::mt19937& randomEngine, ParticleEffectType type);

	bool IsCollision(const AABB& aabb, const Vector3& point);

private: /// ---------- メンバ変数 ---------- ///

	ParticleTransform transform;
	ParticleMaterial material_;

	std::unordered_map<ParticleEffectType, ParticleMesh> meshMap_;

	BlendMode cuurenttype = BlendMode::kBlendModeAdd;

	DirectXCommon* dxCommon_ = nullptr;
	SRVManager* srvManager_ = nullptr;
	Camera* camera_ = nullptr;

	Matrix4x4 worldViewProjectionMatrix;
	Matrix4x4 debugViewProjectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;

	ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;

	// モデルの読み込み
	ModelData modelData;

	// パーティクルグループコンテナ
	std::unordered_map<std::string, ParticleGroup> particleGroups;

	// ランダムエンジン
	std::random_device seedGeneral;
	std::mt19937 randomEngin;

	// 描画数
	const uint32_t kNumMaxInstance = 1024;

	bool useBillboard = true;

	bool isWind = false;

	bool isDebugCamera_ = false;

	// Fieldを作る
	AccelerationField accelerationField;

private: /// ---------- コピー禁止 ---------- ///

	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;
};

