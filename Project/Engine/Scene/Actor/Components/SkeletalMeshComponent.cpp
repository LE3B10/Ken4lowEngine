#define NOMINMAX
#include "SkeletalMeshComponent.h"
#include "AssetPathSelector.h"
#include "DirectXCommon.h"

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
		hasInitialized_ = true;
		ReloadSkeletalModel();

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
		ProcessReloadRequest();

		if (!animationModel_)
		{
			return;
		}

		SyncTransformToAnimationModel();
		ApplyPlaybackSettings();
		animationModel_->Update(); // 既存のAnimationModel更新処理へ再生状態とTransformを反映する
		isPlaying_ = IsPlaying();
	}

	void SkeletalMeshComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		ProcessReloadRequest();

		if (!animationModel_)
		{
			return;
		}

		SyncTransformToAnimationModel();
		const bool wasPlaying = animationModel_->IsAnimationPlaying();
		animationModel_->SetAnimationPlaying(false);
		animationModel_->Update(); // 物理更新後のTransformを再生時間を進めずに描画へ反映する
		animationModel_->SetAnimationPlaying(wasPlaying);
		isPlaying_ = IsPlaying();
	}

	void SkeletalMeshComponent::Draw()
	{
		if (!visible_ || !animationModel_ || !hasMesh_)
		{
			return;
		}

		SyncTransformToAnimationModel();
		animationModel_->RefreshWorldTransform();
		animationModel_->Draw(); // 既存のスキニング描画パスで描画する
	}

	void SkeletalMeshComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();

#ifdef USE_IMGUI
		ImGui::SeparatorText("スケルタルメッシュコンポーネント");

		ImGui::SeparatorText("モデル");
		char modelPathBuffer[256]{};
		std::snprintf(modelPathBuffer, sizeof(modelPathBuffer), "%s", modelPath_.c_str());
		if (ImGui::InputText("モデルパス", modelPathBuffer, sizeof(modelPathBuffer)))
		{
			SetModelPath(modelPathBuffer);
		}

		std::string selectedModelPath = modelPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##SkeletalMeshComponentModelPath", selectedModelPath, AssetType::SkeletalMesh))
		{
			SetModelPath(selectedModelPath);
		}

		ImGui::SeparatorText("アニメーション");
		if (animationModel_ && !animationModel_->GetAnimationClips().empty())
		{
			const auto& clips = animationModel_->GetAnimationClips();
			if (ImGui::BeginCombo("アニメーション名", animationName_.empty() ? animationModel_->GetCurrentAnimationName().c_str() : animationName_.c_str()))
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
			char animationNameBuffer[128]{};
			std::snprintf(animationNameBuffer, sizeof(animationNameBuffer), "%s", animationName_.c_str());
			if (ImGui::InputText("アニメーション名", animationNameBuffer, sizeof(animationNameBuffer)))
			{
				SetAnimationName(animationNameBuffer);
			}
		}

		ImGui::SeparatorText("再生");
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

		const size_t clipCount = animationModel_ ? animationModel_->GetAnimationClips().size() : 0;
		const Vector3 componentWorldPosition = GetWorldPosition();
		const Vector3 componentWorldRotation = GetWorldRotation();
		const Vector3 componentWorldScale = GetWorldScale();
		const Vector3 animationModelPosition = animationModel_ ? animationModel_->GetTranslate() : Vector3{};
		const Vector3 animationModelRotation = animationModel_ ? animationModel_->GetRotate() : Vector3{};
		const Vector3 animationModelScale = animationModel_ ? animationModel_->GetScale() : Vector3{};

		ImGui::Text("現在時刻: %.3f", animationModel_ ? animationModel_->GetAnimationTime() : 0.0f);
		ImGui::Text("再生状態: %s", clipCount == 0 ? "Clipなし" : (IsPlaying() ? "再生中" : "停止中"));
		ImGui::Text("モデル状態: %s", animationModel_ ? "読み込み済み" : "未読み込み");
		ImGui::Text("ロード状態: %s", modelStatus_.c_str());
		ImGui::Text("メッシュ: %s", hasMesh_ ? "あり" : "なし");
		ImGui::Text("Skeleton: %s", hasSkeleton_ ? "あり" : "なし");
		ImGui::Text("Clip数: %zu", clipCount);
		ImGui::Text("選択中Clip: %s", animationModel_ ? animationModel_->GetCurrentAnimationName().c_str() : "");
		ImGui::Text("Draw可能: %s", (visible_ && animationModel_ && hasMesh_) ? "はい" : "いいえ");
		ImGui::Text("Component World Position: %.3f, %.3f, %.3f", componentWorldPosition.x, componentWorldPosition.y, componentWorldPosition.z);
		ImGui::Text("Component World Rotation: %.3f, %.3f, %.3f", componentWorldRotation.x, componentWorldRotation.y, componentWorldRotation.z);
		ImGui::Text("Component World Scale: %.3f, %.3f, %.3f", componentWorldScale.x, componentWorldScale.y, componentWorldScale.z);
		ImGui::Text("AnimationModel Position: %.3f, %.3f, %.3f", animationModelPosition.x, animationModelPosition.y, animationModelPosition.z);
		ImGui::Text("AnimationModel Rotation: %.3f, %.3f, %.3f", animationModelRotation.x, animationModelRotation.y, animationModelRotation.z);
		ImGui::Text("AnimationModel Scale: %.3f, %.3f, %.3f", animationModelScale.x, animationModelScale.y, animationModelScale.z);

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
		ReleaseAnimationModel();
		isPlaying_ = false;
		isPaused_ = false;
		hasInitialized_ = false;
		reloadRequested_ = false;
		resumeAfterReload_ = false;
		modelStatus_ = modelPath_.empty() ? "Empty" : "Finalized";
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

		bool hasRestoredModelPath = false;
		std::string restoredModelPath;
		if (inJson.contains("ModelPath") && inJson["ModelPath"].is_string())
		{
			restoredModelPath = inJson["ModelPath"].get<std::string>();
			hasRestoredModelPath = !restoredModelPath.empty();
		}
		if (!hasRestoredModelPath && inJson.contains("AnimationPath") && inJson["AnimationPath"].is_string())
		{
			restoredModelPath = inJson["AnimationPath"].get<std::string>();
			hasRestoredModelPath = !restoredModelPath.empty();
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

		if (hasRestoredModelPath)
		{
			SetModelPath(restoredModelPath);
		}
		else if (animationModel_)
		{
			EnsureAnimationSelection(true);
			ApplyPlaybackSettings();
		}
	}

	void SkeletalMeshComponent::SetModelPath(std::string_view modelPath)
	{
		const std::string newModelPath(modelPath);
		if (modelPath_ == newModelPath)
		{
			if (!animationModel_ && hasInitialized_)
			{
				resumeAfterReload_ = resumeAfterReload_ || playOnStart_;
				RequestReload();
			}
			return;
		}

		modelPath_ = newModelPath;
		resumeAfterReload_ = resumeAfterReload_ || playOnStart_ || isPlaying_ || (animationModel_ && animationModel_->IsAnimationPlaying());
		RequestReload();
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
			ReloadSkeletalModel();
		}
		if (!animationModel_)
		{
			isPlaying_ = false;
			isPaused_ = false;
			return;
		}

		if (!EnsureAnimationSelection(false))
		{
			animationModel_->SetAnimationPlaying(false);
			isPlaying_ = false;
			isPaused_ = false;
			return;
		}
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
		if (!animationModel_->GetAnimationClips().empty())
		{
			animationModel_->ResetAnimationTime();
		}
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
			ReloadSkeletalModel();
		}
		if (!animationModel_)
		{
			isPlaying_ = false;
			isPaused_ = false;
			return;
		}

		if (!EnsureAnimationSelection(true))
		{
			animationModel_->SetAnimationPlaying(false);
			isPlaying_ = false;
			isPaused_ = false;
			return;
		}
		animationModel_->ResetAnimationTime();
		Play();
	}

	bool SkeletalMeshComponent::IsPlaying() const
	{
		return animationModel_ && !animationModel_->GetAnimationClips().empty() && animationModel_->IsAnimationPlaying();
	}

	bool SkeletalMeshComponent::ReloadSkeletalModel()
	{
		ReleaseAnimationModel();
		isPlaying_ = false;
		isPaused_ = false;
		hasMesh_ = false;
		hasSkeleton_ = false;

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
			animationModel_ = std::make_unique<AnimationModel>();
			animationModel_->Initialize(modelPath_, true);
			SyncTransformToAnimationModel();
			hasMesh_ = animationModel_->HasMesh();
			hasSkeleton_ = animationModel_->HasSkeleton();
			if (!hasMesh_)
			{
				modelStatus_ = "No Mesh";
				animationModel_.reset();
				return false;
			}
			EnsureAnimationSelection(true);
			ApplyPlaybackSettings();
			animationModel_->SetAnimationPlaying(false);
			modelStatus_ = "Loaded";
			return true;
		} catch (const std::exception& e)
		{
			animationModel_.reset();
			modelStatus_ = std::string("Failed: ") + e.what();
		} catch (...)
		{
			animationModel_.reset();
			modelStatus_ = "Failed";
		}

		return false;
	}

	void SkeletalMeshComponent::RequestReload()
	{
		reloadRequested_ = true;
		modelStatus_ = modelPath_.empty() ? "Empty" : "Reload Pending";
	}

	void SkeletalMeshComponent::ProcessReloadRequest()
	{
		if (!reloadRequested_ || !hasInitialized_)
		{
			return;
		}

		const bool shouldResume = resumeAfterReload_;
		reloadRequested_ = false;
		resumeAfterReload_ = false;

		if (ReloadSkeletalModel() && shouldResume)
		{
			Play();
		}
	}

	void SkeletalMeshComponent::ReleaseAnimationModel()
	{
		if (!animationModel_)
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
				fenceManager->Wait(); // GPUが参照中のSkinClusterリソースを破棄しないようにする
			}
		}

		animationModel_.reset();
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

	bool SkeletalMeshComponent::EnsureAnimationSelection(bool resetTime)
	{
		if (!animationModel_)
		{
			return false;
		}

		if (SelectConfiguredAnimation(resetTime))
		{
			return true;
		}

		const auto& clips = animationModel_->GetAnimationClips();
		if (clips.empty())
		{
			animationName_.clear();
			return false;
		}

		animationName_ = clips.front().name;
		return animationModel_->PlayAnimationByIndex(0, resetTime);
	}
}
