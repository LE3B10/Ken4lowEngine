#pragma once
#include <DX12Include.h>
#include <ModelData.h>
#include <Material.h>
#include <TransformationMatrix.h>
#include <VertexData.h>
#include <DirectionalLight.h>
#include "Emitter.h"
#include "Particle.h"

#include <unordered_map>
#include <random>

/// ---------- 前方宣言 ----------///
class DirectXCommon;

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
		// マテリアルデータ
		// パーティクルのリスト
		// インスタンシングデータ用SRVインデックス
		// インスタンシングリソース
		// インスタンス数
		// インスタンシングデータを書き込むためのポインタ
	};

public: /// ---------- メンバ関数 ---------- ///

	//  シングルトンインスタンス
	static ParticleManager* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// パーティクルグループの生成
	void CreateParticleGroup(const std::string name, const std::string textureFilePath);

private: /// ---------- メンバ関数 ---------- ///

	

private: /// ---------- メンバ変数 ---------- ///

	// パーティクルグループコンテナ
	std::unordered_map<std::string, ParticleGroup> particleGroups;
	
};

