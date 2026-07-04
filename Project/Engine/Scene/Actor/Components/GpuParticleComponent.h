#pragma once
#include "SceneComponent.h"
#include "Vector3.h"
#include "Vector4.h"

#include <string>

namespace Ken4lowEngine
{
	class GpuParticleEmitter;

	/// -------------------------------------------------------------
	///   ActorにGPUパーティクル演出を追加するComponentクラス
	/// -------------------------------------------------------------
	class GpuParticleComponent : public SceneComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		void Initialize() override;
		void Update(float deltaTime) override;
		void DrawImGui() override;
		void Finalize() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		std::string GetClassTypeName() const override
		{
			return "GpuParticleComponent"; // GpuParticleComponentとして保存する。
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 再生制御 ---------- ///

		void Play();
		void Stop();
		void Restart();
		bool IsPlaying() const;

	public: /// ---------- 設定取得 ---------- ///

		const std::string& GetEffectName() const { return effectName_; }
		void SetEffectName(const std::string& effectName) { effectName_ = effectName; }

		const std::string& GetEmitterName() const { return emitterName_; }
		void SetEmitterName(const std::string& emitterName) { emitterName_ = emitterName; }

		bool IsPlayOnStart() const { return playOnStart_; }
		void SetPlayOnStart(bool playOnStart) { playOnStart_ = playOnStart; }

		bool IsLoop() const { return loop_; }
		void SetLoop(bool loop) { loop_ = loop; }

		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible);

		bool IsFollowOwner() const { return followOwner_; }
		void SetFollowOwner(bool followOwner) { followOwner_ = followOwner; }

		const Vector3& GetLocalOffset() const { return localOffset_; }
		void SetLocalOffset(const Vector3& localOffset) { localOffset_ = localOffset; }

		const Vector3& GetScale() const { return scale_; }
		void SetScale(const Vector3& scale) { scale_ = scale; }

	private: /// ---------- 内部処理 ---------- ///

		std::string ResolveEmitterName() const;
		Vector3 CalculateEmitterPosition() const;
		GpuParticleEmitter* EnsureEmitter();
		void SyncEmitterPosition();

	private: /// ---------- メンバ変数 ---------- ///

		std::string effectName_ = "Smoke";
		std::string emitterName_;
		bool playOnStart_ = false;
		bool loop_ = false;
		bool visible_ = true;
		bool followOwner_ = true;
		Vector3 localOffset_{ 0.0f, 0.0f, 0.0f };
		Vector3 scale_{ 1.0f, 1.0f, 1.0f };

		float emissionRate_ = 10.0f;
		float lifeTime_ = 1.0f;
		Vector4 startColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 endColor_{ 1.0f, 1.0f, 1.0f, 0.0f };
		float startSize_ = 1.0f;
		float endSize_ = 1.0f;
		float velocityScale_ = 1.0f;

		bool playing_ = false;
		std::string activeEmitterName_;
		Vector3 lockedPosition_{ 0.0f, 0.0f, 0.0f };
	};
}
