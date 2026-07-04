#pragma once
#include "SceneComponent.h"
#include "AudioManager.h"
#include "Vector3.h"

#include <string>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// Actorの3D位置に基づいて距離減衰する音声を再生するComponentクラス
	/// -------------------------------------------------------------
	class WorldAudioComponent : public SceneComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override
		{
			return "WorldAudioComponent";
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void Play();
		void Stop();
		void Restart();
		bool IsPlaying() const;

		void SetSoundPath(const std::string& soundPath);
		void SetVolume(float volume);
		void SetPitch(float pitch);
		void SetLoop(bool loop) { loop_ = loop; }
		void SetPlayOnStart(bool playOnStart) { playOnStart_ = playOnStart; }
		void SetEnabled(bool enabled);
		void SetMinDistance(float minDistance);
		void SetMaxDistance(float maxDistance);
		void SetFollowOwner(bool followOwner);

	private:
		void UpdateRuntimeAudio();
		float CalculateDistanceToListener() const;
		float CalculateAttenuatedVolume(float distance) const;
		void SanitizeDistanceRange();
		Vector3 GetCurrentSourcePosition() const;

	private:
		std::string soundPath_;
		float volume_ = 1.0f;
		float pitch_ = 1.0f;
		bool loop_ = false;
		bool playOnStart_ = false;
		bool enabled_ = true;
		float minDistance_ = 1.0f;
		float maxDistance_ = 30.0f;
		bool followOwner_ = true;

		Vector3 sourcePosition_{ 0.0f, 0.0f, 0.0f };
		float currentDistance_ = 0.0f;
		float currentAttenuatedVolume_ = 0.0f;
		AudioManager::AudioHandle audioHandle_ = AudioManager::InvalidAudioHandle;
	};
}
