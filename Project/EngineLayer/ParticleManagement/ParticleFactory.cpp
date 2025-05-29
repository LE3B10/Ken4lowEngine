#include "ParticleFactory.h"

Particle ParticleFactory::Create(std::mt19937& randomEngine, const Vector3& position, ParticleEffectType effectType)
{
	Particle particle;

	switch (effectType)
	{
	case ParticleEffectType::Default: {
		std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
		std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
		std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

		Vector3 randomTranslate{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
		particle.transform.translate_ = position + randomTranslate;
		particle.transform.scale_ = { 1.0f, 1.0f, 1.0f };
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };
		particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
		particle.lifeTime = distTime(randomEngine);
		particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
		break;
	}

	case ParticleEffectType::Slash: {
		std::uniform_real_distribution<float> distScale(0.8f, 3.0f);
		std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);

		particle.transform.scale_ = { 0.1f, distScale(randomEngine) * 2.0f, 2.0f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 0.0f, 0.0f, 0.0f };
		particle.transform.rotate_ = { 0.0f, 0.0f, distRotate(randomEngine) };
		particle.transform.translate_ = position;
		particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		particle.lifeTime = 1.0f;
		particle.velocity = { 0.0f, 0.0f, 0.0f };
		break;
	}
	case ParticleEffectType::Ring: {
		// ランダム処理なしの固定値
		particle.transform.translate_ = position; // 位置
		particle.transform.scale_ = { 1.0f, 1.0f, 1.0f }; // 大きさ
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f }; // Z軸回転（45度）

		particle.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 色
		particle.lifeTime = 999.0f; // 半永久的に表示
		particle.velocity = { 0.0f, 0.0f, 0.0f }; // 動かさない

		particle.startScale = particle.transform.scale_;
		particle.endScale = particle.transform.scale_;
		break;
	}
	case ParticleEffectType::Cylinder: {
		std::uniform_real_distribution<float> distColor(0.0f, 1.0f);

		particle.transform.translate_ = position;
		particle.transform.scale_ = { 1.0f, 1.0f, 1.0f }; // 高さ方向にスケール
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };

		particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
		particle.lifeTime = 999.0f; // 一時的にずっと表示

		particle.startScale = particle.transform.scale_;
		particle.endScale = particle.transform.scale_;
		break;
	}
	case ParticleEffectType::Star: {
		std::uniform_real_distribution<float> distColor(0.8f, 1.0f);
		particle.transform.translate_ = position;
		particle.transform.scale_ = { 0.5f, 0.5f, 0.5f };
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };
		particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
		particle.lifeTime = 0.3f;
		particle.velocity = { 0.0f, 0.0f, 0.0f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 0.0f, 0.0f, 0.0f };
		break;
	}
	case ParticleEffectType::Smoke: {
		std::uniform_real_distribution<float> distOffset(-0.3f, 0.3f);
		std::uniform_real_distribution<float> distScale(0.3f, 0.6f);
		std::uniform_real_distribution<float> distTime(0.6f, 1.2f);
		std::uniform_real_distribution<float> distGray(0.3f, 0.6f);
		std::uniform_real_distribution<float> distRot(0.0f, std::numbers::pi_v<float> *2.0f);
		std::uniform_real_distribution<float> distDir(-0.1f, 0.1f);

		Vector3 offset = {
			distOffset(randomEngine),
			0.0f,
			distOffset(randomEngine)
		};

		float gray = distGray(randomEngine);
		particle.transform.translate_ = position + offset;
		particle.transform.rotate_.z = distRot(randomEngine);
		particle.color = { gray, gray, gray, 0.5f };
		particle.lifeTime = distTime(randomEngine);

		float scale = distScale(randomEngine);
		particle.startScale = { scale * 0.3f, scale * 0.3f, scale * 0.3f };
		particle.endScale = { scale, scale, scale };

		particle.velocity = { distDir(randomEngine), 0.3f, distDir(randomEngine) };
		break;
	}
	}

	particle.currentTime = 0.0f;
	return particle;
}
