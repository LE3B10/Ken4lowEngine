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
public: /// ---------- メンバ関数 ---------- ///

	ParticleManager() = default;
	~ParticleManager() = default;

	//  シングルトンインスタンス
	static ParticleManager* GetInstance();

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

private: /// ---------- 構造体 ---------- ///

	// GPUで使用するパーティクルの情報を格納する構造体
	struct ParticleForGPU
	{
		Matrix4x4 WVP;   // ワールド・ビュー・プロジェクション行列
		Matrix4x4 World; // ワールド行列
		Vector4 color;   // パーティクルの色
	};

	// パーティクルグループを管理する構造体
	struct ParticleGroup
	{
		MaterialData materialData;				  // マテリアルデータ(テクスチャ情報など)
		std::list<Particle> particles;			  // パーティクルのリスト
		uint32_t srvIndex;						  // SRVインデックス
		ComPtr<ID3D12Resource> instancebuffer;	  // インスタンシング用バッファ
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU; // CPU用ハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU; // GPU用ハンドル
		ParticleForGPU* mappedData;				  // バッファにマッピングされたデータ
		uint32_t numParticles = 0;				  // パーティクル数
	};

private: /// ---------- メンバ関数 ---------- ///

	// 頂点データの初期化
	void InitializeVertexData();
	
	// パーティクルを更新
	void UpdateParticles(ParticleGroup& group);
	
	// パーティクルグループを描画
	void DrawParticleGroup(const ParticleGroup& group);


private: /// ---------- メンバ変数 ---------- ///

	// メンバ変数
	DirectXCommon* dxCommon_ = nullptr; // DirectXの共通クラス
	SRVManager* srvManager_ = nullptr;  // SRVマネージャー
	Camera* camera_ = nullptr;			// カメラクラス

	ModelData modelData;					 // モデルデータ
	TransformationMatrix* wvpData = nullptr; // WVP用の行列データ

	ComPtr<ID3D12Resource> vertexResource;	   // 頂点リソース
	ComPtr<ID3D12Resource> materialResource;   // マテリアルリソース
	ComPtr<ID3D12Resource> wvpResource;		   // WVPリソース
	ComPtr<ID3D12Resource> instancingResource; // インスタンシングリソース

	std::unique_ptr<PipelineStateManager> pipelineManager_; // パイプライン管理クラス

	std::unordered_map<std::string, ParticleGroup> particleGroups; // パーティクルグループを格納するマップ
	std::mt19937 randomEngin; // ランダムエンジン

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{}; // 頂点バッファビュー

	uint32_t numInstance = 0; // インスタンス数
	
	Matrix4x4 backToFrontMatrix; // ビルボード行列（カメラ正面に常に向く）
};

