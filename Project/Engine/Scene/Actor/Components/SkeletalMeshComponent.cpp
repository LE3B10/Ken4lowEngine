#define NOMINMAX
#include "SkeletalMeshComponent.h"

#include <algorithm>
#include <cstdio>
#include <exception>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		float ClampPlaybackSpeed(float speed)
		{
			return std::clamp(speed, 0.0f, 10.0f);
		}
	}

	void SkeletalMeshComponent::Initialize()
	{
		SceneComponent::Initialize();
		CreateAnimationModel();

		if (playOnStart_)
		{
			Play(); // Component開始時に設定されたアニメーションを再生する
		}
		else
		{
			Stop();
		}
	}

	void SkeletalMeshComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);

		if (!animationModel_)
		{
			return;
		}

		SyncTransformToAnimationModel();
		ApplyPlaybackSettings();
		animationModel_->Update(); // 既存のAnimationModel更新処理へ再生状態とTransformを反映する
		isPlaying_ = animationModel_->IsAnimationPlaying();
	}

	void SkeletalMeshComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!animationModel_)
		{
			return;
		}

		SyncTransformToAnimationModel();
		const bool wasPlaying = animationModel_->IsAnimationPlaying();
		animationModel_->SetAnimationPlaying(false);
		animationModel_->Update(); // 物理更新後のTransformを再生時間を進めずに描画へ反映する
		animationModel_->SetAnimationPlaying(wasPlaying);
		isPlaying_ = animationModel_->IsAnimationPlaying();
	}

	void SkeletalMeshComponent::Draw()
	{
		if (!visible_ || !animationModel_)
		{
			return;
		}

		animationModel_->Draw(); // 既存のスキニング描画パスで描画する
	}

	void SkeletalMeshComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();

#ifdef USE_IMGUI
		ImGui::SeparatorText("スケルタルメッシュコンポーネント");

		char modelPathBuffer[256]{};
		std::snprintf(modelPathBuffer, sizeof(modelPathBuffer), "%s", modelPath_.c_str());
		if (ImGui::InputText("モデルパス", modelPathBuffer, sizeof(modelPathBuffer)))
		{
			SetModelPath(modelPathBuffer);
		}

		char animationNameBuffer[128]{};
		std::snprintf(animationNameBuffer, sizeof(animationNameBuffer), "%s", animationName_.c_str());
		if (ImGui::InputText("アニメーション名", animationNameBuffer, sizeof(animationNameBuffer)))
		{
			SetAnimationName(animationNameBuffer);
		}

		if (ImGui::Checkbox("表示", &visible_))
		{
			SetVisible(visible_);
		}

		if (ImGui::Checkbox("ループ再生", &loop_))
		{
			SetLoop(loop_);
		}

		if (ImGui::Checkbox("開始時に再生", &playOnStart_))
		{
			SetPlayOnStart(playOnStart_);
		}

		if (ImGui::DragFloat("再生速度", &playbackSpeed_, 0.01f, 0.0f, 10.0f))
		{
			SetPlaybackSpeed(playbackSpeed_);
		}

		ImGui::Text("現在時刻: %.3f", animationModel_ ? animationModel_->GetAnimationTime() : 0.0f);
		ImGui::Text("再生状態: %s", IsPlaying() ? "再生中" : "停止中");
		ImGui::Text("モデル状態: %s", animationModel_ ? "読み込み済み" : "未読み込み");

		if (animationModel_)
		{
			const auto& clips = animationModel_->GetAnimationClips();
			if (!clips.empty() && ImGui::BeginCombo("アニメーション一覧", animationName_.empty() ? animationModel_->GetCurrentAnimationName().c_str() : animationName_.c_str()))
			{
				for (const auto& clip : clips)
				{
					const bool selected = clip.name == animationName_;
					if (ImGui::Selectable(clip.name.c_str(), selected))
					{
						SetAnimationName(clip.name);
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}

		if (ImGui::Button("再生"))
		{
			Play();
		}
		ImGui::SameLine();
		if (ImGui::Button("停止"))
		{
			Stop();
		}
		ImGui::SameLine();
		if (ImGui::Button("一時停止"))
		{
			Pause();
		}
		ImGui::SameLine();
		if (ImGui::Button("リスタート"))
		{
			Restart();
		}
#endif // USE_IMGUI
	}

	void SkeletalMeshComponent::Finalize()
	{
		animationModel_.reset();
		isPlaying_ = false;
		isPaused_ = false;
	}

	void SkeletalMeshComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);

		outJson["Class"] = GetClassTypeName();
		outJson["ModelPath"] = modelPath_;
		outJson["AnimationName"] = animationName_;
		outJson["Visible"] = visible_;
		outJson["Loop"] = loop_;
		outJson["PlayOnStart"] = playOnStart_;
		outJson["PlaybackSpeed"] = playbackSpeed_;
	}

	void SkeletalMeshComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);

		if (inJson.contains("ModelPath") && inJson["ModelPath"].is_string())
		{
			modelPath_ = inJson["ModelPath"].get<std::string>();
		}
		if (inJson.contains("AnimationName") && inJson["AnimationName"].is_string())
		{
			animationName_ = inJson["AnimationName"].get<std::string>();
		}
		if (inJson.contains("Visible") && inJson["Visible"].is_boolean())
		{
			visible_ = inJson["Visible"].get<bool>();
		}
		if (inJson.contains("Loop") && inJson["Loop"].is_boolean())
		{
			loop_ = inJson["Loop"].get<bool>();
		}
		if (inJson.contains("PlayOnStart") && inJson["PlayOnStart"].is_boolean())
		{
			playOnStart_ = inJson["PlayOnStart"].get<bool>();
		}
		if (inJson.contains("PlaybackSpeed") && inJson["PlaybackSpeed"].is_number())
		{
			playbackSpeed_ = ClampPlaybackSpeed(inJson["PlaybackSpeed"].get<float>());
		}
	}

	void SkeletalMeshComponent::SetModelPath(std::string_view modelPath)
	{
		const std::string newModelPath(modelPath);
		if (modelPath_ == newModelPath)
		{
			return;
		}

		modelPath_ = newModelPath;
		CreateAnimationModel();
	}

	void SkeletalMeshComponent::SetAnimationName(std::string_view animationName)
	{
		animationName_ = std::string(animationName);
		SelectConfiguredAnimation(true);
	}

	void SkeletalMeshComponent::SetVisible(bool visible)
	{
		visible_ = visible;
	}

	void SkeletalMeshComponent::SetLoop(bool loop)
	{
		loop_ = loop;
		ApplyPlaybackSettings();
	}

	void SkeletalMeshComponent::SetPlayOnStart(bool playOnStart)
	{
		playOnStart_ = playOnStart;
	}

	void SkeletalMeshComponent::SetPlaybackSpeed(float playbackSpeed)
	{
		playbackSpeed_ = ClampPlaybackSpeed(playbackSpeed);
		ApplyPlaybackSettings();
	}

	void SkeletalMeshComponent::Play()
	{
		if (!animationModel_)
		{
			CreateAnimationModel();
		}
		if (!animationModel_)
		{
			isPlaying_ = false;
			isPaused_ = false;
			return;
		}

		SelectConfiguredAnimation(false);
		ApplyPlaybackSettings();
		animationModel_->SetAnimationPlaying(true);
		isPlaying_ = true;
		isPaused_ = false;
	}

	void SkeletalMeshComponent::Stop()
	{
		isPlaying_ = false;
		isPaused_ = false;
		if (!animationModel_)
		{
			return;
		}

		animationModel_->SetAnimationPlaying(false);
		animationModel_->ResetAnimationTime();
	}

	void SkeletalMeshComponent::Pause()
	{
		isPlaying_ = false;
		isPaused_ = true;
		if (animationModel_)
		{
			animationModel_->SetAnimationPlaying(false);
		}
	}

	void SkeletalMeshComponent::Restart()
	{
		if (!animationModel_)
		{
			CreateAnimationModel();
		}
		if (!animationModel_)
		{
			isPlaying_ = false;
			isPaused_ = false;
			return;
		}

		SelectConfiguredAnimation(true);
		animationModel_->ResetAnimationTime();
		Play();
	}

	bool SkeletalMeshComponent::IsPlaying() const
	{
		return animationModel_ ? animationModel_->IsAnimationPlaying() : isPlaying_;
	}

	void SkeletalMeshComponent::CreateAnimationModel()
	{
		animationModel_.reset();
		isPlaying_ = false;
		isPaused_ = false;

		if (modelPath_.empty())
		{
			return;
		}

		try
		{
			animationModel_ = std::make_unique<AnimationModel>();
			animationModel_->Initialize(modelPath_, true);
			SyncTransformToAnimationModel();
			SelectConfiguredAnimation(true);
			ApplyPlaybackSettings();
			animationModel_->SetAnimationPlaying(false);
		}
		catch (const std::exception&)
		{
			animationModel_.reset();
		}
		catch (...)
		{
			animationModel_.reset();
		}
	}

	void SkeletalMeshComponent::ApplyPlaybackSettings()
	{
		if (!animationModel_)
		{
			return;
		}

		animationModel_->SetAnimationLoop(loop_);
		animationModel_->SetAnimationSpeed(playbackSpeed_);
	}

	void SkeletalMeshComponent::SyncTransformToAnimationModel()
	{
		if (!animationModel_)
		{
			return;
		}

		animationModel_->SetTranslate(GetWorldPosition());
		animationModel_->SetRotate(GetWorldRotation());
		animationModel_->SetScale(GetWorldScale());
	}

	bool SkeletalMeshComponent::SelectConfiguredAnimation(bool resetTime)
	{
		if (!animationModel_ || animationName_.empty())
		{
			return false;
		}

		return animationModel_->PlayAnimationByName(animationName_, resetTime);
	}
}
