#include "ParticleEmitter.h"
#include <LogString.h>
#include <DirectXCommon.h>

/// -------------------------------------------------------------
///		   　		パーティクルを生成する関数
/// -------------------------------------------------------------
void ParticleEmitter::Initialize()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// ランダムエンジンの初期化
	std::random_device seedGenerator;
	std::mt19937 randomEngine(seedGenerator());

	// ⊿t を定義。とりあえず60fps固定してあるが、実時間を計測して可変fpsで動かせるようにする
	const float kDeltaTime = 1.0f / 60.0f;

	// 描画するインスタンス数
	uint32_t numInstance = 0;

	// エミッター
	Emitter emitter{};
	emitter.count = 3;
	emitter.frequency = 0.5f;
	emitter.frequencyTime = 0.0f;

	emitter.transform = {
		{1.0f, 1.0f, 1.0f },
		{0.0f, 0.0f, 0.0f },
		{0.0f, 0.0f, 0.0f }
	};

	// パーティクルをリストで管理
	std::list<Particle> particles;
}


/// -------------------------------------------------------------
///		   　		パーティクルを生成する関数
/// -------------------------------------------------------------
Particle ParticleEmitter::MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate)
{
	Particle particle;

	// 一様分布生成器を使って乱数を生成
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

	// 位置と速度を[-1, 1]でランダムに初期化
	particle.transform = {
		{ 1.0f, 1.0f, 1.0 },
		{ 0.0f, 0.0f, 0.0f },
		{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) }
	};

	// 発生場所を計算
	Vector3 randomTranslate{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	particle.transform.translate = translate + randomTranslate;

	// 色を[0, 1]でランダムに初期化
	particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine) };

	// パーティクル生成時にランダムに1秒～3秒の間生存
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0;
	particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };

	return particle;
}


/// -------------------------------------------------------------
///		   　		パーティクルを射出する関数
/// -------------------------------------------------------------
std::list<Particle> ParticleEmitter::Emit(const Emitter& emitter, std::mt19937& randomEngine)
{
	std::list<Particle> particles;

	for (uint32_t count = 0; count < emitter.count; ++count)
	{
		particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
	}

	return particles;
}


/// -------------------------------------------------------------
///		   　		        当たり判定
/// -------------------------------------------------------------
bool ParticleEmitter::IsCollision(const AABB& aabb, const Vector3& point)
{
	return (
		point.x >= aabb.min.x && point.x <= aabb.max.x &&
		point.y >= aabb.min.x && point.y <= aabb.max.y &&
		point.z >= aabb.min.x && point.z <= aabb.max.z
		);
}
