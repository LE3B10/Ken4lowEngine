#define NOMINMAX
#include "AnimatedModelComponent.h"
#include "AssetPathSelector.h"
#include "DirectXCommon.h"

#include <algorithm>
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

	void AnimatedModelComponent::Initialize()
	{
		SceneComponent::Initialize();
		hasInitialized_ = true;
		ReloadAnimatedModel();

		if (playOnStart_)
		{
			Play();
		}
		else
		{
			Stop();
		}
	}

	void AnimatedModelComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);
		ProcessReloadRequest();

		if (!animatedModel_)
		{
			return;
		}

		SyncTransformToAnimatedModel();
		ApplyPlaybackSettings();
		animatedModel_->Update();
		isPlaying_ = IsPlaying();
	}

	void AnimatedModelComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		ProcessReloadRequest();

		if (!animatedModel_)
		{
			return;
		}

		SyncTransformToAnimatedModel();
		const bool wasPlaying = animatedModel_->IsAnimationPlaying();
		animatedModel_->SetAnimationPlaying(false);
		animatedModel_->Update();
		animatedModel_->SetAnimationPlaying(wasPlaying);
		isPlaying_ = IsPlaying();
	}

	void AnimatedModelComponent::Draw()
	{
		if (!visible_ || !animatedModel_ || !hasMesh_)
		{
			return;
		}

		SyncTransformToAnimatedModel();
		animatedModel_->RefreshWorldTransform();
		animatedModel_->Draw();
	}

	void AnimatedModelComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();

#ifdef USE_IMGUI
		ImGui::SeparatorText("アニメーションモデルコンポーネント");

		ImGui::SeparatorText("モデル");
		ImGui::Text("モデルパス: %s", modelPath_.empty() ? "未選択" : modelPath_.c_str());

		std::string selectedModelPath = modelPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##AnimatedModelComponentModelPath", selectedModelPath, AssetType::SkeletalMesh))
		{
			SetModelPath(selectedModelPath);
		}

		ImGui::SeparatorText("アニメーション");
		if (animatedModel_ && !animatedModel_->GetAnimationClips().empty())
		{
			const auto& clips = animatedModel_->GetAnimationClips();
			const char* preview = animationName_.empty() ? animatedModel_->GetCurrentAnimationName().c_str() : animationName_.c_str();
			if (ImGui::BeginCombo("アニメーション名", preview))
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
		else
		{
			ImGui::Text("アニメーション名: %s", animationName_.empty() ? "Clipなし" : animationName_.c_str());
		}

		ImGui::SeparatorText("再生");
		ComponentPropertyUtility::DrawImGui(CreateProperties(false, false));

		const size_t clipCount = animatedModel_ ? animatedModel_->GetAnimationClips().size() : 0;
		ImGui::Text("現在時刻: %.3f", animatedModel_ ? animatedModel_->GetAnimationTime() : 0.0f);
		ImGui::Text("再生状態: %s", clipCount == 0 ? "Clipなし" : (IsPlaying() ? "再生中" : "停止中"));
		ImGui::Text("読み込み状態: %s", modelStatus_.c_str());
		ImGui::Text("モデル状態: %s", animatedModel_ ? "読み込み済み" : "未読み込み");
		ImGui::Text("メッシュ: %s", hasMesh_ ? "あり" : "なし");
		ImGui::Text("Clip数: %zu", clipCount);
		ImGui::Text("選択中Clip: %s", animatedModel_ ? animatedModel_->GetCurrentAnimationName().c_str() : "");
		ImGui::Text("Draw可能: %s", (visible_ && animatedModel_ && hasMesh_) ? "はい" : "いいえ");

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
		if (ImGui::Button("リスタート"))
		{
			Restart();
		}
#endif // USE_IMGUI
	}

	void AnimatedModelComponent::Finalize()
	{
		ReleaseAnimatedModel();
		isPlaying_ = false;
		hasInitialized_ = false;
		reloadRequested_ = false;
		resumeAfterReload_ = false;
		modelStatus_ = modelPath_.empty() ? "Empty" : "Finalized";
	}

	void AnimatedModelComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);

		outJson["Class"] = GetClassTypeName();
		ComponentPropertyUtility::ToJson(const_cast<AnimatedModelComponent*>(this)->CreateProperties(), outJson);
	}

	void AnimatedModelComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);

		ComponentPropertyUtility::FromJson(CreateProperties(false, true), inJson);
		if (inJson.contains("ModelPath") && inJson["ModelPath"].is_string())
		{
			SetModelPath(inJson["ModelPath"].get<std::string>());
		}
	}

	void AnimatedModelComponent::SetModelPath(std::string_view modelPath)
	{
		const std::string newModelPath(modelPath);
		if (modelPath_ == newModelPath)
		{
			if (!animatedModel_ && hasInitialized_)
			{
				resumeAfterReload_ = resumeAfterReload_ || playOnStart_;
				RequestReload();
			}
			return;
		}

		modelPath_ = newModelPath;
		resumeAfterReload_ = resumeAfterReload_ || playOnStart_ || isPlaying_ || (animatedModel_ && animatedModel_->IsAnimationPlaying());
		RequestReload();
	}

	void AnimatedModelComponent::SetAnimationName(std::string_view animationName)
	{
		animationName_ = std::string(animationName);
		SelectConfiguredAnimation(true);
	}

	void AnimatedModelComponent::SetLoop(bool loop)
	{
		loop_ = loop;
		ApplyPlaybackSettings();
	}

	void AnimatedModelComponent::SetPlaybackSpeed(float playbackSpeed)
	{
		playbackSpeed_ = ClampPlaybackSpeed(playbackSpeed);
		ApplyPlaybackSettings();
	}

	void AnimatedModelComponent::Play()
	{
		if (!animatedModel_)
		{
			ReloadAnimatedModel();
		}
		if (!animatedModel_)
		{
			isPlaying_ = false;
			return;
		}

		if (!EnsureAnimationSelection(false))
		{
			animatedModel_->SetAnimationPlaying(false);
			isPlaying_ = false;
			return;
		}

		ApplyPlaybackSettings();
		animatedModel_->SetAnimationPlaying(true);
		isPlaying_ = true;
	}

	void AnimatedModelComponent::Stop()
	{
		isPlaying_ = false;
		if (!animatedModel_)
		{
			return;
		}

		animatedModel_->SetAnimationPlaying(false);
		if (!animatedModel_->GetAnimationClips().empty())
		{
			animatedModel_->ResetAnimationTime();
		}
	}

	void AnimatedModelComponent::Restart()
	{
		if (!animatedModel_)
		{
			ReloadAnimatedModel();
		}
		if (!animatedModel_)
		{
			isPlaying_ = false;
			return;
		}

		if (!EnsureAnimationSelection(true))
		{
			animatedModel_->SetAnimationPlaying(false);
			isPlaying_ = false;
			return;
		}

		animatedModel_->ResetAnimationTime();
		Play();
	}

	bool AnimatedModelComponent::IsPlaying() const
	{
		return animatedModel_ && !animatedModel_->GetAnimationClips().empty() && animatedModel_->IsAnimationPlaying();
	}

	bool AnimatedModelComponent::ReloadAnimatedModel()
	{
		ReleaseAnimatedModel();
		isPlaying_ = false;
		hasMesh_ = false;

		if (modelPath_.empty())
		{
			modelStatus_ = "Empty";
			return false;
		}

		if (!hasInitialized_)
		{
			modelStatus_ = "Waiting Initialize";
			return false;
		}

		try
		{
			animatedModel_ = std::make_unique<AnimationModel>();
			animatedModel_->Initialize(modelPath_, false);
			SyncTransformToAnimatedModel();
			hasMesh_ = animatedModel_->HasMesh();
			if (!hasMesh_)
			{
				modelStatus_ = "No Mesh";
				animatedModel_.reset();
				return false;
			}

			EnsureAnimationSelection(true);
			ApplyPlaybackSettings();
			animatedModel_->SetAnimationPlaying(false);
			modelStatus_ = "Loaded";
			return true;
		}
		catch (const std::exception& e)
		{
			animatedModel_.reset();
			modelStatus_ = std::string("Failed: ") + e.what();
		}
		catch (...)
		{
			animatedModel_.reset();
			modelStatus_ = "Failed";
		}

		return false;
	}

	void AnimatedModelComponent::RequestReload()
	{
		reloadRequested_ = true;
		modelStatus_ = modelPath_.empty() ? "Empty" : "Reload Pending";
	}

	void AnimatedModelComponent::ProcessReloadRequest()
	{
		if (!reloadRequested_ || !hasInitialized_)
		{
			return;
		}

		const bool shouldResume = resumeAfterReload_;
		reloadRequested_ = false;
		resumeAfterReload_ = false;

		if (ReloadAnimatedModel() && shouldResume)
		{
			Play();
		}
	}

	void AnimatedModelComponent::ReleaseAnimatedModel()
	{
		if (!animatedModel_)
		{
			return;
		}

		if (DirectXCommon* dxCommon = DirectXCommon::GetInstance())
		{
			auto* fenceManager = dxCommon->GetFenceManager();
			auto* commandManager = dxCommon->GetCommandManager();
			if (fenceManager && commandManager && commandManager->GetCommandQueue())
			{
				fenceManager->Signal(commandManager->GetCommandQueue());
				fenceManager->Wait(); // GPUが参照中のモデルリソースを破棄しないようにする
			}
		}

		animatedModel_.reset();
	}

	void AnimatedModelComponent::ApplyPlaybackSettings()
	{
		if (!animatedModel_)
		{
			return;
		}

		animatedModel_->SetAnimationLoop(loop_);
		animatedModel_->SetAnimationSpeed(playbackSpeed_);
	}

	void AnimatedModelComponent::SyncTransformToAnimatedModel()
	{
		if (!animatedModel_)
		{
			return;
		}

		animatedModel_->SetTranslate(GetWorldPosition());
		animatedModel_->SetRotate(GetWorldRotation());
		animatedModel_->SetScale(GetWorldScale());
	}

	bool AnimatedModelComponent::SelectConfiguredAnimation(bool resetTime)
	{
		if (!animatedModel_ || animationName_.empty())
		{
			return false;
		}

		return animatedModel_->PlayAnimationByName(animationName_, resetTime);
	}

	bool AnimatedModelComponent::EnsureAnimationSelection(bool resetTime)
	{
		if (!animatedModel_)
		{
			return false;
		}

		if (SelectConfiguredAnimation(resetTime))
		{
			return true;
		}

		const auto& clips = animatedModel_->GetAnimationClips();
		if (clips.empty())
		{
			animationName_.clear();
			return false;
		}

		animationName_ = clips.front().name;
		return animatedModel_->PlayAnimationByIndex(0, resetTime);
	}

	std::vector<ComponentProperty> AnimatedModelComponent::CreateProperties(bool includeModelPath, bool includeAnimationName)
	{
		std::vector<ComponentProperty> properties = {
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetVisible(*typedValue); } } },
			{ "Loop", "ループ再生", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return loop_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetLoop(*typedValue); } } },
			{ "PlayOnStart", "開始時に再生", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return playOnStart_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetPlayOnStart(*typedValue); } } },
			{ "PlaybackSpeed", "再生速度", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return playbackSpeed_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetPlaybackSpeed(*typedValue); } }, 0.0f, 10.0f, 0.01f, true }
		};

		if (includeAnimationName)
		{
			properties.insert(properties.begin(),
				{ "AnimationName", "アニメーション名", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return animationName_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetAnimationName(*typedValue); } } });
		}

		if (includeModelPath)
		{
			properties.insert(properties.begin(),
				{ "ModelPath", "モデルパス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return modelPath_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetModelPath(*typedValue); } } });
		}

		return properties;
	}
}
