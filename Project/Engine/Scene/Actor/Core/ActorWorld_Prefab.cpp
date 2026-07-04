#include "ActorWorld.h"

#include "ActorJsonSerializer.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void ActorWorld::DrawActorPrefabSpawnImGui()
	{
#ifdef USE_IMGUI
		constexpr size_t kPathBufferSize = 256;
		std::array<char, kPathBufferSize> buffer{};

		std::snprintf(buffer.data(), buffer.size(), "%s", actorPrefabPath_.c_str());

		if (ImGui::InputText("Prefab Path", buffer.data(), buffer.size()))
		{
			actorPrefabPath_ = buffer.data(); // Prefab Pathを更新する
		}

		if (ImGui::Button("Spawn Actor From JSON"))
		{
			ActorSpawnOptions options;
			options.applySpawnOffset = true; // JSON生成時は位置オフセットを適用しない
			options.spawnOffset = actorPrefabSpawnOffset_; // JSON生成時は位置オフセットを適用しない
			options.disableAutoRegisterMainCamera = true; // JSON生成時はCameraComponentの自動登録を無効化する

			pendingSpawnFilePath_ = actorPrefabPath_; // JSON生成予約を次フレームの安全なタイミングで処理する
			pendingSpawnOptions_ = options;
			hasPendingSpawnActor_ = true;

			lastActorJsonSaveMessage_ = "Spawn requested : " + pendingSpawnFilePath_;
		}

		ImGui::SameLine();

		if (ImGui::Button("Use TestActor"))
		{
			actorPrefabPath_ = "Resources/ActorPrefabs/TestActor.json"; // Prefab PathをTestActorに設定する
		}

		ImGui::SameLine();

		if (ImGui::Button("Use TestGroundActor"))
		{
			actorPrefabPath_ = "Resources/ActorPrefabs/TestGroundActor.json"; // Prefab PathをTestGroundに設定する
		}

		DrawActorPrefabBrowserImGui(); // Actor Prefabsのブラウザを描画する

		DrawActorPrefabSaveImGui(); // Actor Prefabsの保存ウィンドウを描画する

		if (!lastActorJsonSaveMessage_.empty())
		{
			ImGui::Text("%s", lastActorJsonSaveMessage_.c_str());
		}
#endif // USE_IMGUI
	}

	void ActorWorld::RefreshActorPrefabFileList()
	{
		actorPrefabFiles_.clear();

		const std::filesystem::path prefabDirectoryPath{ actorPrefabDirectory_ };

		if (!std::filesystem::exists(prefabDirectoryPath))
		{
			lastActorJsonSaveMessage_ = "Prefab directory does not exist: " + actorPrefabDirectory_;
			return; // ディレクトリが存在しない場合は何もしない
		}

		if (!std::filesystem::is_directory(prefabDirectoryPath))
		{
			lastActorJsonSaveMessage_ = "Prefab path is not a directory: " + actorPrefabDirectory_;
			return; // ディレクトリでない場合は何もしない
		}

		for (const auto& entry : std::filesystem::directory_iterator(prefabDirectoryPath))
		{
			if (!entry.is_regular_file())
			{
				continue; // ファイルでない場合はスキップする
			}

			const std::filesystem::path filePath = entry.path();

			if (filePath.extension() != ".json")
			{
				continue; // JSONファイルでない場合はスキップする
			}

			actorPrefabFiles_.push_back(filePath.string()); // JSONファイルをリストに追加する
		}

		std::sort(actorPrefabFiles_.begin(), actorPrefabFiles_.end()); // ファイル名順にソートする

		lastActorJsonSaveMessage_ = "Prefab list refreshed : " + std::to_string(actorPrefabFiles_.size()) + " files";
	}

	void ActorWorld::DrawActorPrefabBrowserImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::Button("Refresh Prefab List"))
		{
			RefreshActorPrefabFileList(); // Prefabリストを更新する
		}

		if (actorPrefabFiles_.empty())
		{
			ImGui::Text("No prefab json files.");
			return; // Prefabファイルが無い場合は何もしない
		}

		ImGui::SeparatorText("Prefab List");

		for (const std::string& prefabFilePath : actorPrefabFiles_)
		{
			const std::filesystem::path path{ prefabFilePath };
			const std::string fileName = path.filename().generic_string(); // ファイル名のみを取得する

			const bool isSelected = actorPrefabPath_ == prefabFilePath;

			if (ImGui::Selectable(fileName.c_str(), isSelected))
			{
				actorPrefabPath_ = prefabFilePath; // Prefab Pathを選択したファイルに更新する
				lastActorJsonSaveMessage_ = "Selected prefab : " + actorPrefabPath_;
			}
		}

		ImGui::Separator();

		if (ImGui::Button("Delete Selected Prefab JSON"))
		{
			ImGui::OpenPopup("Delete Prefab JSON?");
		}

		if (ImGui::BeginPopupModal("Delete Prefab JSON?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Delete this prefab file?");
			ImGui::Text("%s", actorPrefabPath_.c_str());

			ImGui::Separator();

			if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
			{
				DeleteSelectedActorPrefabFile(); // 選択中Prefab JSONを削除する。
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
#endif // USE_IMGUI
	}

	void ActorWorld::DrawActorPrefabSaveImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("Save Actor Prefab");

		constexpr size_t kPathBufferSize = 256;
		std::array<char, kPathBufferSize> buffer{};

		std::snprintf(buffer.data(), buffer.size(), "%s", actorPrefabSavePath_.c_str());

		if (ImGui::InputText("Save Prefab Path", buffer.data(), buffer.size()))
		{
			actorPrefabSavePath_ = buffer.data(); // Save Prefab Pathを更新する
		}

		Actor* saveTargetActor = selectedActor_;

		if (!saveTargetActor && selectedComponent_)
		{
			saveTargetActor = selectedComponent_->GetOwner(); // Componentが選択中なら所有Actorを取得する
		}

		if (!saveTargetActor)
		{
			ImGui::Text("No selected Actor.");
			return; // 保存対象のActorが無い場合は何もしない
		}

		ImGui::Text("Save Target : %s", saveTargetActor->GetName().c_str());

		if (ImGui::Button("Save Selected As Prefab"))
		{
			const bool result = ActorJsonSerializer::SaveActorToFile(*saveTargetActor, actorPrefabSavePath_);

			if (result)
			{
				lastActorJsonSaveMessage_ = "Saved prefab : " + actorPrefabSavePath_;

				RefreshActorPrefabFileList(); // Prefabリストを更新する
			}
			else
			{
				lastActorJsonSaveMessage_ = "Save Prefab failed : " + actorPrefabSavePath_;
			}
		}
#endif // USE_IMGUI
	}

	void ActorWorld::DeleteSelectedActorPrefabFile()
	{
		if (!IsValidActorPrefabJsonPath(actorPrefabPath_))
		{
			lastActorJsonSaveMessage_ = "Delete prefab failed : invalid path";
			return; // 無効なパスの場合は何もしない
		}

		const std::filesystem::path deletePath{ actorPrefabPath_ };

		if (!std::filesystem::exists(deletePath))
		{
			lastActorJsonSaveMessage_ = "Delete prefab failed : file not found";
			RefreshActorPrefabFileList(); // Prefabリストを更新する
			return; // ファイルが存在しない場合は何もしない
		}

		std::error_code ec;
		const bool removed = std::filesystem::remove(deletePath, ec);

		if (!removed || ec)
		{
			lastActorJsonSaveMessage_ = "Delete prefab failed : " + actorPrefabPath_;
			return;
		}

		lastActorJsonSaveMessage_ = "Deleted prefab : " + actorPrefabPath_;

		RefreshActorPrefabFileList(); // Prefabリストを更新する

		// 削除したパスが入力欄に残り続けると紛らわしいので、残っているPrefabがあれば先頭を選択する
		if (!actorPrefabFiles_.empty())
		{
			actorPrefabSavePath_ = actorPrefabFiles_.front(); // Prefab Save Pathを先頭のファイルに更新する
		}
		else
		{
			actorPrefabPath_.clear(); // Prefab Save Pathを空にする
		}
	}

	bool ActorWorld::IsValidActorPrefabJsonPath(const std::string& filePath) const
	{
		if (filePath.empty())
		{
			return false; // 空パスは無効
		}

		const std::filesystem::path prefabDirectoryPath =
			std::filesystem::weakly_canonical(std::filesystem::path(actorPrefabDirectory_));

		const std::filesystem::path targetPath =
			std::filesystem::weakly_canonical(std::filesystem::path(filePath));

		if (targetPath.extension() != ".json")
		{
			return false; // JSONファイルでない場合は無効
		}

		const std::string prefabDirectoryString = prefabDirectoryPath.generic_string();
		const std::string targetPathString = targetPath.generic_string();

		// targetPath が actorPrefabDirectory_ のサブディレクトリに含まれるかどうかを確認する
		if (targetPathString.find(prefabDirectoryString) != 0)
		{
			return false; // PrefabDirectory外のファイルは無効
		}

		return true;
	}
}