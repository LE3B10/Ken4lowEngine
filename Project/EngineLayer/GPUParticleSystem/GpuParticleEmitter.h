#pragma once
#include <string>
#include <Vector3.h>
#include <Vector4.h>

/// ---------- 前方宣言 ---------- ///
class GpuParticleBuffers;

/// ---------- Gpuエミッター構造体 ---------- ///
struct GpuEmitterDesc
{
	uint32_t count = 10;         // 発生数
	float radius = 1.0f;        // 半径
	float lifetime = 5.0f;      // 寿命
	float speed = 1.0f;         // 速度
	float frequency = 0.5f;    // 発生頻度
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 色
	uint32_t type = 0;          // エミッタータイプ（0:球体）
};

/// -------------------------------------------------------------
///			　	GPUパーティクルエミッタークラス
/// -------------------------------------------------------------
class GpuParticleEmitter
{
public: /// ---------- メンバ関数 ---------- ///

	GpuParticleEmitter(const std::string& name, const GpuEmitterDesc& desc)
		: name_(name), desc_(desc) {
	}

	// 指定位置でパーティクルを出したいときに呼ぶ
	void Emit(GpuParticleBuffers* buffers, const Vector3& position) const;

	// エミッター名の取得
	const std::string& GetName() const { return name_; }

private: /// ---------- メンバ変数 ---------- ///

	std::string name_;
	GpuEmitterDesc desc_;
};

