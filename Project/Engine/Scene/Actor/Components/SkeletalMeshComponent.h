#pragma once
#include "SceneComponent.h"
#include "AnimationModel.h"
#include "ComponentProperty.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// Actorにボーン付き3Dモデル描画とアニメーション再生機能を追加するComponentクラス。
	/// -------------------------------------------------------------
	class SkeletalMeshComponent : public SceneComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		void Draw() override;
		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override
		{
			return "SkeletalMeshComponent";
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SetModelPath(std::string_view modelPath);
		void SetAnimationName(std::string_view animationName);
		void SetVisible(bool visible);
		void SetLoop(bool loop);
		void SetPlayOnStart(bool playOnStart);
		void SetPlaybackSpeed(float playbackSpeed);

		void Play();
		void Stop();
		void Pause();
		void Restart();
		bool IsPlaying() const;
		std::vector<ComponentProperty> CreateProperties(bool includeModelPath = true, bool includeAnimationName = true);

	private:
		bool ReloadSkeletalModel();
		void RequestReload();
		void ProcessReloadRequest();
		void ReleaseAnimationModel();
		void ApplyPlaybackSettings();
		void SyncTransformToAnimationModel();
		bool SelectConfiguredAnimation(bool resetTime);
		bool EnsureAnimationSelection(bool resetTime);

	private:
		std::unique_ptr<AnimationModel> animationModel_;
		std::string modelPath_;
		std::string animationName_;
		std::string modelStatus_ = "Empty";
		bool hasMesh_ = false;
		bool hasSkeleton_ = false;
		bool visible_ = true;
		bool loop_ = true;
		bool playOnStart_ = true;
		float playbackSpeed_ = 1.0f;
		bool isPlaying_ = false;
		bool isPaused_ = false;
		bool hasInitialized_ = false;
		bool reloadRequested_ = false;
		bool resumeAfterReload_ = false;
	};
}
