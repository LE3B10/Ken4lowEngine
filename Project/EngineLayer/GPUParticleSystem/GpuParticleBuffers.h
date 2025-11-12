#pragma once
#include <DX12Include.h>
#include <Vector3.h>
#include <Vector4.h>
#include <Matrix4x4.h>

#include "BillboardMode.h"
#include "GpuParticleEmitterData.h"

/// ---------- 前方宣言 ---------- ///
class Camera;

/// -------------------------------------------------------------
///			　GPUパーティクルバッファクラス
/// -------------------------------------------------------------
class GpuParticleBuffers
{
	// パーティクルの構造体
	struct ParticleCS
	{
		Vector3 translate; // 位置
		Vector3 scale;	   // スケール
		float lifeTime;	   // 寿命
		Vector3 velocity;  // 速度
		float currentTime; // 経過時間
		uint32_t type;	   // パーティクルの種類
		uint32_t billboardMode; // ビルボードモード
		Vector4 color;	   // 色
	};

	// ビュー行列と射影行列
	struct PerView
	{
		Matrix4x4 viewProjectionMatrix{}; // ビュー射影行列
		Matrix4x4 billboardMatrix{}; // ビルボード用行列
		uint32_t billboardMode{}; // ビルボードモード
		float padding[3]; // パディング
	};

	// 時間計測用
	struct PerFrame
	{
		float time; // 経過時間
		float deltaTime; // 前フレームからの経過時間
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Camera* camera);

	// 更新処理
	void Update(float deltaTime);

public: /// ---------- ゲッター ---------- ///

	// パーティクルバッファの取得
	ID3D12Resource* GetParticleBuffer() const { return particleBuffer_.Get(); }

	// ビュー行列と射影行列バッファの取得
	ID3D12Resource* GetPerViewBuffer() const { return perViewBuffer_.Get(); }

	// エミッターバッファの取得
	ID3D12Resource* GetEmitterBuffer() const { return emitterBuffer_.Get(); }

	// 時間計測用バッファの取得
	ID3D12Resource* GetPerFrameBuffer() const { return perFrameBuffer_.Get(); }

	// フリーカウンターバッファの取得
	ID3D12Resource* GetFreeCounterBuffer() const { return freeListIndexBuffer_.Get(); }

	// 最大パーティクル数の取得
	static uint32_t GetMaxParticles() { return kMaxParticles; }

	// パーティクルバッファのSRVインデックスの取得
	uint32_t GetParticleSrvIndex() const { return particleSrvIndex_; }

	// パーティクルバッファのUAVインデックスの取得
	uint32_t GetParticleUavIndex() const { return particleUavIndex_; }

	// フリーカウンターバッファのUAVインデックスの取得
	uint32_t GetFreeCounterUavIndex() const { return freeListIndexUavIndex_; }

	// パーティクルバッファのマッピング用ポインタの取得
	ParticleCS* GetParticleData() const { return particleData_; }

	// ビュー行列と射影行列バッファのマッピング用ポインタの取得
	PerView* GetPerViewData() const { return perViewData_; }

	// フリーリストバッファの取得
	GpuEmitterCBData* GetEmitterCBData() const { return emitterCBData_; }

private: /// ---------- メンバ関数 ---------- ///

	// パーティクルバッファの生成
	void CreateParticleBuffer();

	// ビュー行列と射影行列バッファの生成
	void CreatePerViewBuffer();

	// エミッターバッファの生成
	void CreateEmitterBuffer();

	// 時間計測用バッファの生成
	void CreatePerFrameBuffer();

	// フリーカウンターバッファの生成
	void CreateFreeListIndexBuffer();

	// フリーリストバッファの生成
	void CreateFreeListBuffer();

private: /// ---------- メンバ変数 ---------- ///

	// 最大パーティクル数
	static const uint32_t kMaxParticles = 131072;

	Camera* camera_ = nullptr; // カメラのポインタ
	bool isDebugCamera_ = false; // デバッグカメラ使用フラグ

	// パーティクルバッファ
	ComPtr<ID3D12Resource> particleBuffer_;
	ParticleCS* particleData_ = nullptr; // マッピング用ポインタ
	uint32_t particleSrvIndex_ = 0;	 // SRVのインデックス
	uint32_t particleUavIndex_ = 0;	 // UAVのインデックス

	// フリーリストインデックスバッファ
	ComPtr<ID3D12Resource> freeListIndexBuffer_;
	uint32_t freeListIndexUavIndex_ = 0;

	// フリーリスト
	ComPtr<ID3D12Resource> freeListBuffer_;
	uint32_t freeListUavIndex_ = 0;

	// ビュー行列と射影行列バッファ
	ComPtr<ID3D12Resource> perViewBuffer_;
	PerView* perViewData_ = nullptr; // マッピング用ポインタ

	// エミッターバッファ
	ComPtr<ID3D12Resource> emitterBuffer_;
	GpuEmitterCBData* emitterCBData_ = nullptr; // マッピング用ポインタ

	// 時間計測用バッファ
	ComPtr<ID3D12Resource> perFrameBuffer_;
	PerFrame* perFrameData_ = nullptr; // マッピング用ポインタ

	// 行列関連
	Matrix4x4 worldViewProjectionMatrix;  // ワールドビュー射影行列
	Matrix4x4 debugViewProjectionMatrix_; // デバッグカメラのビュー射影行列
	Matrix4x4 viewProjectionMatrix_;	  // ビュー射影行列
};

