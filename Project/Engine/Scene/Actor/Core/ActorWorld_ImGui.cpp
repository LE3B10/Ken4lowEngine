#include "ActorWorld.h"

#include "ActorJsonSerializer.h"
#include "ComponentFactory.h"

#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void ActorWorld::DrawImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::Begin("Actor World"))
		{
			ImGui::Text("Actor Count: %zu", actors_.size());

			DrawActorPrefabSpawnImGui(); // Actor PrefabsのSpawnボタンを描画する

			ImGui::Separator();

			for (size_t actorIndex = 0; actorIndex < actors_.size(); ++actorIndex)
			{
				Actor* actor = actors_[actorIndex].get();
				if (!actor)
				{
					continue; // Actorがnullptrなら表示しない
				}

				Actor* beforeSelectedActor = selectedActor_;
				ActorComponent* beforeSelectedComponent = selectedComponent_;

				ImGui::PushID(static_cast<int>(actorIndex));
				actor->DrawHierarchyImGui(selectedActor_, selectedComponent_); // Actor World上にActor/Component階層を表示する
				ImGui::PopID();

				if (beforeSelectedActor != selectedActor_ || beforeSelectedComponent != selectedComponent_)
				{
					requestFocusActorDetails_ = true; // 選択中ActorまたはComponentが変化した場合にDetailsウィンドウを更新する
				}
			}
		}
		ImGui::End();

		DrawDetailsImGui(); // 選択中ActorまたはComponentのDetailsウィンドウを描画する
		SyncLightComponentsToLightManager(); // ImGuiで編集したライト設定を次の描画へ反映する
#endif // USE_IMGUI
	}

	void ActorWorld::DrawDetailsImGui()
	{
#ifdef USE_IMGUI
		if (requestFocusActorDetails_)
		{
			ImGui::SetNextWindowFocus(); // 選択が変わったらActor Detailsを前面へ出す
			requestFocusActorDetails_ = false;
		}

		if (ImGui::Begin("Actor Details"))
		{
			Actor* saveTargetActor = selectedActor_;

			if (!saveTargetActor && selectedComponent_)
			{
				saveTargetActor = selectedComponent_->GetOwner(); // Componentが選択中なら所有Actorを取得する
			}

			if (saveTargetActor)
			{
				if (ImGui::Button("Save Selected Actor JSON"))
				{
					const std::string actorName = saveTargetActor->GetName().empty()
						? saveTargetActor->GetClassTypeName()
						: saveTargetActor->GetName();

					const std::string filePath = "Resources/ActorPrefabs/" + actorName + ".json";

					const bool succeeded = ActorJsonSerializer::SaveActorToFile(*saveTargetActor, filePath);

					lastActorJsonSaveMessage_ = succeeded
						? "Saved : " + filePath
						: "Save failed : " + filePath;
				}

				ImGui::SameLine();

				if (ImGui::Button("Load Selected Actor JSON"))
				{
					const std::string actorName = saveTargetActor->GetName().empty()
						? saveTargetActor->GetClassTypeName()
						: saveTargetActor->GetName();

					const std::string filePath = "Resources/ActorPrefabs/" + actorName + ".json";

					selectedComponent_ = nullptr; // Componentは作り直されるので選択解除する

					pendingReloadActor_ = saveTargetActor; // JSON読込予約を次フレームの安全なタイミングで処理する
					pendingReloadFilePath_ = filePath;
					hasPendingReloadActor_ = true;

					lastActorJsonSaveMessage_ = "Reload requested : " + filePath;
				}

				ImGui::SameLine();

				if (ImGui::Button("Delete Selected Actor"))
				{
					pendingDeleteActor_ = saveTargetActor; // Destroy予約を次フレームの安全なタイミングで処理する
					hasPendingDeleteActor_ = true;

					selectedActor_ = nullptr; // Actorは削除されるので選択解除する
					selectedComponent_ = nullptr; // Componentは削除されるので選択解除する

					lastActorJsonSaveMessage_ = "Delete requested : " + saveTargetActor->GetName();
				}

				if (!lastActorJsonSaveMessage_.empty())
				{
					ImGui::Text("%s", lastActorJsonSaveMessage_.c_str());
				}

				ImGui::Separator();
			}

			if (selectedComponent_)
			{
				const std::string componentName = selectedComponent_->GetName().empty()
					? "Unnamed Component"
					: selectedComponent_->GetName();

				ImGui::Text("Selected Component: %s", componentName.c_str());

				Actor* owner = selectedComponent_->GetOwner();
				const bool isRootComponent = owner && selectedComponent_ == owner->GetRootComponent();

				if (isRootComponent)
				{
					ImGui::TextDisabled("RootComponent cannot be deleted or duplicated.");
				}
				else
				{
					if (ImGui::Button("Duplicate Selected Component"))
					{
						DuplicateSelectedComponent();
					}

					ImGui::SameLine();

					if (ImGui::Button("Delete Selected Component"))
					{
						ImGui::OpenPopup("Delete Component?");
					}
				}

				if (ImGui::BeginPopupModal("Delete Component?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Delete this Component?");
					ImGui::Text("%s", componentName.c_str());

					ImGui::Separator();

					if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
					{
						DeleteSelectedComponent(); // Draw中には消さず、次フレームUpdateで削除する。
						ImGui::CloseCurrentPopup();
					}

					ImGui::SameLine();

					if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
					{
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}

				ImGui::Separator();

				selectedComponent_->DrawInspectorImGui();
			}
			else if (selectedActor_)
			{
				const std::string actorName = selectedActor_->GetName().empty()
					? "Unnamed Actor"
					: selectedActor_->GetName();

				ImGui::Text("Selected Actor: %s", actorName.c_str());
				ImGui::Separator();

				selectedActor_->DrawInspectorImGui(); // 選択中Actorの詳細を描画する。
			}
			else
			{
				ImGui::Text("No selection.");
			}

			DrawAddComponentImGui(); // 選択中ActorにComponentを追加するUIを描画する
		}

		ImGui::End();
#endif // USE_IMGUI
	}

	void ActorWorld::DrawAddComponentImGui()
	{
#ifdef USE_IMGUI
		Actor* targetActor = selectedActor_;

		if (!targetActor && selectedComponent_)
		{
			targetActor = selectedComponent_->GetOwner(); // Component選択中なら所有Actorを対象にする。
		}

		ImGui::SeparatorText("コンポーネント追加");

		if (!targetActor)
		{
			ImGui::TextDisabled("Actorが選択されていません。");
			return;
		}

		const auto& componentTypes = ComponentFactory::GetRegisteredComponentTypes();

		if (componentTypes.empty())
		{
			ImGui::TextDisabled("登録済みのComponentがありません。");
			return;
		}

		if (selectedAddComponentTypeIndex_ < 0 ||
		selectedAddComponentTypeIndex_ >= static_cast<int>(componentTypes.size()))
		{
			selectedAddComponentTypeIndex_ = 0;
		}

		const ComponentFactory::ComponentTypeInfo& selectedType = componentTypes[selectedAddComponentTypeIndex_];

		if (ImGui::BeginCombo("種類", selectedType.displayName.c_str()))
		{
			std::string currentCategory;

			for (int index = 0; index < static_cast<int>(componentTypes.size()); ++index)
			{
				const ComponentFactory::ComponentTypeInfo& typeInfo = componentTypes[index];

				if (currentCategory != typeInfo.category)
				{
					currentCategory = typeInfo.category;
					ImGui::TextDisabled("%s", currentCategory.c_str());
				}

				const bool alreadyExists = targetActor->HasComponentClass(typeInfo.className);
				const bool disabled = !typeInfo.allowMultiple && alreadyExists;

				if (disabled)
				{
					ImGui::BeginDisabled();
				}

				const bool isSelected = selectedAddComponentTypeIndex_ == index;
				const std::string selectableLabel = typeInfo.displayName + "##" + typeInfo.className;

				if (ImGui::Selectable(selectableLabel.c_str(), isSelected))
				{
					selectedAddComponentTypeIndex_ = index;
				}

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("%s", typeInfo.description.c_str());
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}

				if (disabled)
				{
					ImGui::EndDisabled();
				}
			}

			ImGui::EndCombo();
		}

		const bool alreadyExists = targetActor->HasComponentClass(selectedType.className);
		const bool canAdd = selectedType.allowMultiple || !alreadyExists;

		ImGui::Text("カテゴリ: %s", selectedType.category.c_str());
		ImGui::TextWrapped("%s", selectedType.description.c_str());

		if (!canAdd)
		{
			ImGui::TextDisabled("このComponentは1つだけ追加できます。");
		}

		if (!canAdd)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("追加##AddComponent"))
		{
			AddComponentToSelectedActor(selectedType.className);
		}

		if (!canAdd)
		{
			ImGui::EndDisabled();
		}
#endif // USE_IMGUI
	}
}
