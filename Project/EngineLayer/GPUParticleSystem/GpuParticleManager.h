#pragma once
#include "GpuParticlePipeline.h"
#include "GpuParticleBuffers.h"
#include "GpuParticleRenderer.h"
#include "GpuParticleEmitter.h"

#include <unordered_map>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Camera;


/// -------------------------------------------------------------
///			　GPUパーティクルマネージャークラス
/// -------------------------------------------------------------
class GpuParticleManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static GpuParticleManager* GetInstance();

	// 初期化処理
	void Initialize(Camera* camera);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- グループ処理 ---------- ///

	GpuParticleEmitter& CreateEmitter(const std::string& name, const GpuEmitterDesc& desc);

	// 指定位置でパーティクルを出す
	void Emit(const std::string& name, const Vector3& position);

private: /// ---------- メンバ関数 ---------- ///

	// ディスパッチ処理
	void Dispatch();

	// ディスパッチ処理（エミット用）
	void DispatchEmit();

	// ディスパッチ処理（更新用）
	void DispatchUpdate();

private: /// ---------- メンバ変数 ---------- ///

	Camera* camera_ = nullptr; // カメラのポインタ

	bool isDebugCamera_ = false; // デバッグカメラ有効フラグ

	// GPUパーティクルパイプライン
	std::unique_ptr<GpuParticlePipeline> gpuParticlePipeline_;

	// GPUパーティクルバッファ
	std::unique_ptr<GpuParticleBuffers> gpuParticleBuffers_;

	// GPUパーティクルレンダラー
	std::unique_ptr<GpuParticleRenderer> gpuParticleRenderer_;

	// エミッターコンテナ
	std::unordered_map<std::string, std::unique_ptr<GpuParticleEmitter>> emitters_;
};

