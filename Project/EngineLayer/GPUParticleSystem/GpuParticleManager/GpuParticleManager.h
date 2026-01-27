#pragma once
#include "GpuParticlePipeline.h"
#include "GpuParticleBuffers.h"
#include "GpuParticleRenderer.h"
#include "GpuParticleEmitter.h"

#include "GpuParticleEmitterData.h"

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

	/// <summary>
	/// GpuParticleManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>GpuParticleManager の唯一のインスタンス。</returns>
	static GpuParticleManager* GetInstance();

	/// <summary>
	/// GPU パーティクルシステムの初期化処理。<br/>
	/// ・カメラポインタの保持<br/>
	/// ・GpuParticlePipeline の生成と初期化<br/>
	/// ・GpuParticleBuffers の生成と初期化（カメラ情報などを反映）<br/>
	/// ・GpuParticleRenderer の生成と初期化<br/>
	/// ・初期状態のパーティクルバッファをセットアップする Dispatch() の呼び出し<br/>
	/// を行います。
	/// </summary>
	/// <param name="camera">ビュー射影行列などを取得するためのカメラ。</param>
	void Initialize(Camera* camera);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 毎フレームの更新処理。<br/>
	/// ・GpuParticleBuffers::Update() で Δt やビュー情報などを更新<br/>
	/// ・DispatchUpdate() で GPU 上の全パーティクルを一括更新（位置・寿命など）<br/>
	/// ・全エミッターの BuildCB() を呼び出してエミット要求を CB に積む<br/>
	/// ・Emit が必要なエミッターがあれば DispatchEmit() を呼び出す<br/>
	/// という流れで GPU パーティクルのシミュレーションを進めます。
	/// </summary>
	/// <param name="deltaTime">前フレームからの経過時間（秒）。</param>
	void Update(float deltaTime);

	/// <summary>
	/// GPU パーティクルの描画処理。<br/>
	/// GpuParticleRenderer を通して、GpuParticleBuffers が持つパーティクルバッファを<br/>
	/// インスタンシング描画します。<br/>
	/// 現在は GpuParticleBuffers::GetMaxParticles() をそのまま描画数として渡しています。
	/// </summary>
	void Draw();

	///
	void DrawImGui();

public: /// ---------- エミッター関連 ---------- ///

	/// <summary>
	/// 新しい GPU パーティクルエミッターを作成します。<br/>
	/// name をキーとして GpuParticleEmitter を生成し、内部のコンテナに登録します。<br/>
	/// すでに同名のエミッターが存在する場合は nullptr を返します。
	/// </summary>
	/// <param name="name">エミッター名（識別用のキー）。</param>
	/// <param name="info">エミッターの基本設定（初期速度・寿命・発生レートなど）。</param>
	/// <returns>作成された GpuParticleEmitter へのポインタ。失敗時は nullptr。</returns>
	GpuParticleEmitter* CreateEmitter(const std::string& name, const GpuParticleEmitter::EmitterInfo& info);

	/// <summary>
	/// 指定された名前の GPU パーティクルエミッターを取得します。<br/>
	/// 見つかった場合は内部で管理しているインスタンスへのポインタを返し、<br/>
	/// 見つからない場合は nullptr を返します。
	/// </summary>
	/// <param name="name">取得したいエミッター名。</param>
	/// <returns>GpuParticleEmitter へのポインタ。存在しない場合は nullptr。</returns>
	GpuParticleEmitter* GetEmitter(const std::string& name);

	/// <summary>
	/// 指定されたエミッターに対して「一度だけ count 個ぶんバースト発生させる」リクエストを行います。<br/>
	/// 内部的には GpuParticleEmitter::RequestEmit() を呼び出し、<br/>
	/// 次回 Update() 時に BuildCB() → DispatchEmit() を経由して GPU へ反映されます。
	/// </summary>
	/// <param name="name">バーストさせたいエミッター名。</param>
	/// <param name="count">発生させるパーティクルの個数。</param>
	void BurstEmitter(const std::string& name, uint32_t count);

	// デバッグ用：デバッグカメラの有効／無効を切り替え
	void SetDebugCameraEnabled(bool enabled) { gpuParticleBuffers_->SetDebugCameraEnabled(enabled); }

private: /// ---------- ディスパッチ関数 ---------- ///

	/// <summary>
	/// シミュレーション初期化用ディスパッチ。<br/>
	/// GpuParticleBuffers が持つパーティクルバッファに対して、<br/>
	/// 「初期状態のパーティクル」を書き込む Compute シェーダを実行します。<br/>
	/// ・UAV 用の ResourceTransition<br/>
	/// ・UAVManager::PreDispatch() による UAV ヒープのセット<br/>
	/// ・GpuParticlePipeline::GetCsPSO() を使った Dispatch<br/>
	/// ・NON_PIXEL_SHADER_RESOURCE へのバリア戻し<br/>
	/// などの処理をまとめて行います。
	/// </summary>
	void Dispatch();

	/// <summary>
	/// エミット専用のディスパッチ処理。<br/>
	/// ・UAV に遷移したパーティクルバッファに対して<br/>
	/// ・GpuParticlePipeline::GetCsEmitPSO() を設定して Dispatch<br/>
	/// ・Emitter CB / PerFrame CB を RootConstantBufferView でセット<br/>
	/// することで、エミット部分だけを GPU 上で実行します。
	/// </summary>
	void DispatchEmit(D3D12_GPU_VIRTUAL_ADDRESS emitterCbAddr);

	/// <summary>
	/// 毎フレームの更新専用ディスパッチ処理。<br/>
	/// ・GpuParticlePipeline::GetCsUpdatePSO() を設定して Dispatch<br/>
	/// ・パーティクルバッファ UAV と PerFrame CB をバインド<br/>
	/// ・maxParticles / numthreads からスレッドグループ数を計算して Dispatch<br/>
	/// することで、全パーティクルの寿命・位置・速度などを更新します。
	/// </summary>
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

