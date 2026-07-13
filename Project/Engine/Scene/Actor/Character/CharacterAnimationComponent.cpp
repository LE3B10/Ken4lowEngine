#include "CharacterAnimationComponent.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void CharacterAnimationComponent::Initialize()
	{
		SanitizePlaybackState();
	}

	void CharacterAnimationComponent::Update(float deltaTime)
	{
		if (!isPlaying_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

		playbackTime_ += deltaTime * playbackSpeed_;
		if (loop_)
		{
			playbackTime_ = std::fmod(playbackTime_, duration_);
			return;
		}

		if (playbackTime_ >= duration_)
		{
			playbackTime_ = duration_;
			isPlaying_ = false;
		}
	}

	void CharacterAnimationComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("キャラクターアニメーション");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
		ImGui::Text("正規化再生位置: %.3f", GetNormalizedTime());
#endif
	}

	void CharacterAnimationComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		ComponentPropertyUtility::ToJson(const_cast<CharacterAnimationComponent*>(this)->CreateProperties(), outJson);
	}

	void CharacterAnimationComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
		SanitizePlaybackState();
	}

	void CharacterAnimationComponent::Play(std::string_view animationName, float duration, bool loop)
	{
		animationName_ = animationName.empty() ? "Idle" : std::string(animationName);
		duration_ = std::isfinite(duration) ? std::max(duration, 0.001f) : 1.0f;
		playbackTime_ = 0.0f;
		loop_ = loop;
		isPlaying_ = true;
	}

	void CharacterAnimationComponent::Stop()
	{
		playbackTime_ = 0.0f;
		isPlaying_ = false;
	}

	void CharacterAnimationComponent::Restart()
	{
		playbackTime_ = 0.0f;
		isPlaying_ = true;
	}

	void CharacterAnimationComponent::SetPlaybackSpeed(float playbackSpeed)
	{
		playbackSpeed_ = std::isfinite(playbackSpeed) ? std::max(playbackSpeed, 0.0f) : 1.0f;
	}

	void CharacterAnimationComponent::SetPlaybackTime(float playbackTime)
	{
		playbackTime_ = std::isfinite(playbackTime) ? std::clamp(playbackTime, 0.0f, duration_) : 0.0f;
	}

	float CharacterAnimationComponent::GetNormalizedTime() const
	{
		return duration_ > 0.0f ? playbackTime_ / duration_ : 0.0f;
	}

	std::vector<ComponentProperty> CharacterAnimationComponent::CreateProperties()
	{
		return {
			{ "AnimationName", "アニメーション名", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return animationName_; }, [this](const ComponentPropertyValue& value) { if (const std::string* typedValue = std::get_if<std::string>(&value)) animationName_ = typedValue->empty() ? "Idle" : *typedValue; } },
			{ "Duration", "再生時間", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return duration_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) { duration_ = std::max(*typedValue, 0.001f); SetPlaybackTime(playbackTime_); } }, 0.001f, 3600.0f, 0.01f, true },
			{ "PlaybackTime", "再生位置", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return playbackTime_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetPlaybackTime(*typedValue); }, 0.0f, 3600.0f, 0.01f, true },
			{ "PlaybackSpeed", "再生速度", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return playbackSpeed_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetPlaybackSpeed(*typedValue); }, 0.0f, 100.0f, 0.05f, true },
			{ "Loop", "ループ", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return loop_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) loop_ = *typedValue; } },
			{ "Playing", "再生中", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return isPlaying_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) isPlaying_ = *typedValue; } }
		};
	}

	void CharacterAnimationComponent::SanitizePlaybackState()
	{
		if (animationName_.empty()) animationName_ = "Idle";
		duration_ = std::isfinite(duration_) ? std::max(duration_, 0.001f) : 1.0f;
		SetPlaybackSpeed(playbackSpeed_);
		SetPlaybackTime(playbackTime_);
	}
} // namespace Ken4lowEngine
