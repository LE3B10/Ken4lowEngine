#include "ActorWorld.h"
#include "ColliderComponent.h"
#include "RigidbodyComponent.h"
#include "ActorJsonSerializer.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "SpriteComponent.h"

#include "ComponentFactory.h"
#include "LightManager.h"
#include "SceneComponent.h"
#include "SpriteManager.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>

#ifdef USE_IMGUI
#include <imgui.h>
#include <cstdio>
#endif

namespace Ken4lowEngine
{
#ifdef USE_IMGUI
	namespace
	{
		constexpr size_t kNameEditBufferSize = 128;

		bool DrawNameInput(const char* label, const std::string& currentName, std::string& editName)
		{
			std::array<char, kNameEditBufferSize> buffer{};
			std::snprintf(buffer.data(), buffer.size(), "%s", currentName.c_str());

			if (ImGui::InputText(label, buffer.data(), buffer.size()))
			{
				editName = std::string(buffer.data());
				return true; // 名前が変更された場合にtrueを返す
			}

			return false; // 名前が変更されなかった場合にfalseを返す
		}
	}
#endif // USE_IMGUI

	void ActorWorld::Initialize()
	{
		if (isInitialized_)
		{
			return; // すでに初期化済みなら何もしない
		}

		for (auto& actor : actors_)
		{
			// 初期化処理は各Actorに委譲する
			actor->Initialize();

			// Actor初期化後に生成されたCollider / RigidbodyをPhysicsWorldへ登録する
			RegisterPhysicsComponents(*actor);
		}

		RefreshActorPrefabFileList(); // Actor PrefabsのJSONファイル一覧を更新する

		isInitialized_ = true; // ActorWorldの初期化が完了したことを示す
	}

	void ActorWorld::Update(float deltaTime)
	{
		ProcessPendingActorReload(); // JSON読込予約があれば次フレームの安全なタイミングで処理する
		ProcessPendingActorSpawn();	 // JSON生成予約があれば次フレームの安全なタイミングで処理する
		ProcessPendingActorDelete(); // Destroy予約があれば次フレームの安全なタイミングで処理する
		ProcessPendingComponentDelete(); // Component削除予約があれば次フレームの安全なタイミングで処理する

		for (auto& actor : actors_)
		{
			// ActorWorldは更新順だけ管理し、処理内容はActor/Component側に任せる
			actor->Update(deltaTime);

			if (actor->IsPendingDestroy())
			{
				// Actor削除前にPhysicsWorldが持つ外部参照を解除する。
				UnregisterPhysicsComponents(*actor);
			}
		}

		std::erase_if(actors_, [](const std::unique_ptr<Actor>& actor)
			{
				return actor->IsPendingDestroy(); // Destroy予約されたActorを更新後にまとめて削除する
			});

		SyncLightComponentsToLightManager(); // ActorのLightComponentを描画用ライトへ反映する
	}

	void ActorWorld::PostPhysicsUpdate(float deltaTime)
	{
		for (auto& actor : actors_)
		{
			// PhysicsWorld更新後の処理をActorへ流す。
			actor->PostPhysicsUpdate(deltaTime);
		}

		SyncLightComponentsToLightManager(); // 物理更新後のWorld位置をライトへ反映する
	}

	void ActorWorld::Draw()
	{
		SyncLightComponentsToLightManager(); // 描画直前のLightComponent設定をLightManagerへ渡す

		for (auto& actor : actors_)
		{
			// 通常描画を持つActorだけが内部Component経由で描画される
			actor->Draw();
		}
	}

	void ActorWorld::DrawScreenSpaceSprites()
	{
		std::vector<SpriteComponent*> spriteComponents;

		for (auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy())
			{
				continue; // 削除予定のActorはSprite描画対象から外す
			}

			const auto components = actor->GetComponents<SpriteComponent>();
			for (SpriteComponent* spriteComponent : components)
			{
				if (!spriteComponent || !spriteComponent->CanDrawScreenSpace())
				{
					continue; // 非表示または無効なSpriteComponentは描画しない
				}

				spriteComponents.push_back(spriteComponent);
			}
		}

		std::stable_sort(spriteComponents.begin(), spriteComponents.end(),
			[](const SpriteComponent* a, const SpriteComponent* b)
			{
				return a->GetDrawOrder() < b->GetDrawOrder(); // DrawOrderが小さいSpriteから先に描画する
			});

		if (spriteComponents.empty())
		{
			return; // 描画対象のSpriteComponentが無い場合は何もしない
		}

		SpriteManager::GetInstance()->SetRenderSetting_UI();

		for (SpriteComponent* spriteComponent : spriteComponents)
		{
			spriteComponent->DrawScreenSpace();
		}
	}

	void ActorWorld::DrawShadow()
	{
		for (auto& actor : actors_)
		{
			// 影を落とすActorだけが内部Component経由でShadow描画される
			actor->DrawShadow();
		}
	}

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

	void ActorWorld::Finalize()
	{
		for (auto& actor : actors_)
		{
			// Actor破棄前にPhysicsWorldが持つ外部参照を解除する
			UnregisterPhysicsComponents(*actor);

			// Actor破棄前にComponent側のFinalizeまで流す
			actor->Finalize();
		}

		actors_.clear(); // Finalize後にActorを破棄し、古い状態が残らないようにする
		LightManager::GetInstance()->SetLightComponentPointLights({}); // ActorWorld破棄時にComponent由来ライトを解除する

		isInitialized_ = false; // 再Initialize時にSpawn済みActorを通常初期化できるように戻す
	}

	Actor* ActorWorld::FindActorByName(std::string_view name)
	{
		for (auto& actor : actors_)
		{
			// Actor名が一致した最初のActorを返す
			if (actor->GetName() == name)
			{
				return actor.get();
			}
		}

		return nullptr; // 名前が一致するActorが見つからなかった場合はnullptrを返す
	}

	Actor* ActorWorld::SpawnActorFromJson(std::string_view filePath, const ActorSpawnOptions& options)
	{
		std::unique_ptr<Actor> actor = ActorJsonSerializer::CreateActorFromJson(filePath, options);
		if (!actor)
		{
			lastActorJsonSaveMessage_ = "Load failed : " + std::string(filePath);
			return nullptr; // JSON読み込みに失敗した場合はnullptrを返す
		}

		Actor* spawnedActor = actor.get(); // Spawn後も呼び出し側が生成したActorを扱えるようにポインタを保持しておく

		// SpawnしたActor名が既存Actorと被らないようにする
		spawnedActor->SetName(MakeUniqueActorName(spawnedActor->GetName()));

		for (CameraComponent* cameraComponent : spawnedActor->GetComponents<CameraComponent>())
		{
			if (!cameraComponent)
			{
				continue; // CameraComponentがnullptrなら登録しない
			}

			cameraComponent->SetAutoRegisterMainCamera(false); // SpawnしたActorのCameraComponentをMainCameraとして登録する
		}

		actors_.push_back(std::move(actor)); // ActorWorldがActorの寿命を管理するため、所有権を移動する

		if (isInitialized_)
		{
			RegisterPhysicsComponents(*spawnedActor); // Collider/RigidbodyをPhysicsWorldへ自動登録する
		}

		selectedActor_ = spawnedActor; // Spawn後は自動的に生成したActorを選択状態にする
		selectedComponent_ = nullptr; // Spawn後はComponent選択を解除する

		lastActorJsonSaveMessage_ = "Spawned : " + std::string(filePath); // Spawn成功メッセージを更新する
		return spawnedActor;
	}

	bool ActorWorld::GetSelectedFocusPosition(Vector3& outPosition) const
	{
		if (selectedComponent_)
		{
			if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(selectedComponent_))
			{
				outPosition = sceneComponent->GetWorldPosition();
				return true;
			}

			if (Actor* owner = selectedComponent_->GetOwner())
			{
				if (SceneComponent* root = owner->GetRootComponent())
				{
					outPosition = root->GetWorldPosition(); // ActorComponentなら所有ActorのRoot位置を使う
					return true;
				}
			}
		}

		if (selectedActor_)
		{
			if (SceneComponent* root = selectedActor_->GetRootComponent())
			{
				outPosition = root->GetWorldPosition(); // Actor選択時はRoot位置を使う
				return true;
			}
		}

		// フォーカスできる対象がない
		return false;
	}

	void ActorWorld::SetupDefaultPhysicsSettings()
	{
		if (!physicsWorld_)
		{
			return; // PhysicsWorldが設定されていない場合は何もしない
		}

		// ActorComponent物理では、まず同一Layer同士をBLockにして衝突確認をしやすくする
		physicsWorld_->GetResponseMatrix().SetResponse(
			PhysicsCollisionLayer::DynamicActor,
			PhysicsCollisionLayer::DynamicActor,
			CollisionResponseType::Block); // 動くActor同士は物理的に衝突させる。

		physicsWorld_->GetResponseMatrix().SetResponse(
			PhysicsCollisionLayer::DynamicActor,
			PhysicsCollisionLayer::WorldStatic,
			CollisionResponseType::Block); // 動くActorと床は物理的に衝突させる。
	}

	void ActorWorld::RegisterPhysicsComponents(Actor& actor)
	{
		if (!physicsWorld_ || actor.IsPhysicsRegistered())
		{
			return; // PhysicsWorldが無い、または登録済みの場合は何もしない
		}

		auto colliders = actor.GetComponents<ColliderComponent>();
		auto* rigidbody = actor.GetComponent<RigidbodyComponent>();
		Rigidbody* physicsRigidbody = rigidbody ? rigidbody->GetRigidbody() : nullptr;

		if (physicsRigidbody)
		{
			// ActorのRigidbodyをPhysicsWorldへ登録する
			physicsWorld_->RegisterRigidbody(physicsRigidbody);
		}

		for (ColliderComponent* collider : colliders)
		{
			if (!collider || !collider->GetCollider())
			{
				continue; // Colliderがnullptrなら登録しない
			}

			if (physicsRigidbody)
			{
				collider->GetCollider()->SetRigidbody(physicsRigidbody); // ColliderにRigidbodyを紐付ける
			}

			// ActorのColliderをPhysicsWorldへ登録する
			physicsWorld_->RegisterCollider(collider->GetCollider());
		}

		actor.SetPhysicsRegistered(true); // 登録済みフラグを立てる
	}

	void ActorWorld::UnregisterPhysicsComponents(Actor& actor)
	{
		if (!physicsWorld_ || !actor.IsPhysicsRegistered())
		{
			return; // 未登録なら二重解除しない。
		}

		auto colliders = actor.GetComponents<ColliderComponent>();
		for (ColliderComponent* collider : colliders)
		{
			if (!collider || !collider->GetCollider())
			{
				continue; // Colliderがnullptrなら解除しない
			}

			// ActorのColliderをPhysicsWorldから登録解除する
			physicsWorld_->UnregisterCollider(collider->GetCollider());
		}

		if (auto* rigidbody = actor.GetComponent<RigidbodyComponent>())
		{
			// ActorのRigidbodyをPhysicsWorldから登録解除する
			physicsWorld_->UnregisterRigidbody(rigidbody->GetRigidbody());
		}

		actor.SetPhysicsRegistered(false); // 登録済みフラグを下ろす
	}

	void ActorWorld::SyncLightComponentsToLightManager()
	{
		std::vector<LightManager::PunctualLightGPU> pointLights;

		for (const auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy())
			{
				continue; // 削除予定のActorはライト反映対象から外す
			}

			const auto lightComponents = actor->GetComponents<LightComponent>();
			for (const LightComponent* lightComponent : lightComponents)
			{
				if (!lightComponent || !lightComponent->IsActiveInHierarchy() || !lightComponent->IsEnabled())
				{
					continue; // 無効なLightComponentは描画用ライトに登録しない
				}

				const Vector3& color = lightComponent->GetColor();

				LightManager::PunctualLightGPU light{};
				light.lightType = 2; // Point
				light.color = { color.x, color.y, color.z, 1.0f };
				light.intensity = lightComponent->GetIntensity();
				light.position = lightComponent->GetWorldPosition();
				light.radius = lightComponent->GetRange();
				light.decay = 1.0f;
				light.enabled = 1u;

				pointLights.push_back(light);
			}
		}

		LightManager::GetInstance()->SetLightComponentPointLights(pointLights);
	}

	void ActorWorld::ProcessPendingActorReload()
	{
		if (!hasPendingReloadActor_ || !pendingReloadActor_)
		{
			return; // JSON読込予約が無い場合は何もしない
		}

		Actor* reloadActor = pendingReloadActor_;
		const std::string reloadFilePath = pendingReloadFilePath_;

		hasPendingReloadActor_ = false; // 次フレームで再度処理されないようにフラグを下ろす
		pendingReloadActor_ = nullptr; // 次フレームで再度処理されないようにポインタをクリアする
		pendingReloadFilePath_.clear(); // 次フレームで再度処理されないようにファイルパスをクリアする

		selectedComponent_ = nullptr; // Componentは作り直されるので選択解除する

		const bool succeeded = ReloadActorFromJson(*reloadActor, reloadFilePath);

		lastActorJsonSaveMessage_ = succeeded
			? "Loaded : " + reloadFilePath
			: "Load failed : " + reloadFilePath;

		selectedActor_ = reloadActor; // 読み込み後もActorを選択状態に戻す
	}

	bool ActorWorld::ReloadActorFromJson(Actor& actor, const std::string_view filePath)
	{
		UnregisterPhysicsComponents(actor); // JSON読み込み前にPhysicsWorld登録を解除する

		const bool succeeded = ActorJsonSerializer::LoadActorFromFile(actor, filePath);
		if (!succeeded)
		{
			RegisterPhysicsComponents(actor); // JSON読み込みに失敗した場合はPhysicsWorld登録を元に戻す
			return false; // JSON読み込みに失敗した場合はfalseを返す
		}

		RegisterPhysicsComponents(actor); // JSON読み込み後にPhysicsWorld登録を再度行う
		return true;
	}

	void ActorWorld::ProcessPendingActorSpawn()
	{
		if (!hasPendingSpawnActor_)
		{
			return; // JSON生成予約が無い場合は何もしない
		}

		const std::string spawnFilePath = pendingSpawnFilePath_;
		const ActorSpawnOptions spawnOptions = pendingSpawnOptions_;

		hasPendingSpawnActor_ = false; // 次フレームで再度処理されないようにフラグを下ろす
		pendingSpawnFilePath_.clear(); //次フレームで再度処理されないようにファイルパスをクリアする
		pendingSpawnOptions_ = ActorSpawnOptions{}; // 次フレームで再度処理されないようにオプションをクリアする

		SpawnActorFromJson(spawnFilePath, spawnOptions); // JSONからActorを生成してActorWorldに追加する
	}

	void ActorWorld::ProcessPendingActorDelete()
	{
		if (!hasPendingDeleteActor_ || !pendingDeleteActor_)
		{
			return; // Destroy予約が無い場合は何もしない
		}

		Actor* deletedActor = pendingDeleteActor_;

		hasPendingDeleteActor_ = false; // 次フレームで再度処理されないようにフラグを下ろす
		pendingDeleteActor_ = nullptr; // 次フレームで再度処理されないようにポインタをクリアする

		const std::string deletedActorName = deletedActor->GetName();

		if (selectedActor_ == deletedActor)
		{
			selectedActor_ = nullptr; // Destroy対象が選択中Actorなら選択解除する
		}

		if (selectedComponent_ && selectedComponent_->GetOwner() == deletedActor)
		{
			selectedComponent_ = nullptr; // Destroy対象のActorが所有するComponentが選択中なら選択解除する
		}

		UnregisterPhysicsComponents(*deletedActor); // Destroy前にPhysicsWorld登録を解除する
		deletedActor->Finalize(); // Destroy前にFinalizeを流す

		std::erase_if(actors_, [deletedActor](const std::unique_ptr<Actor>& actor)
			{
				return actor.get() == deletedActor; // Destroy対象のActorをActorWorldから削除する
			});

		lastActorJsonSaveMessage_ = "Destroyed : " + deletedActorName; // Destroy成功メッセージを更新する
	}

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

	void ActorWorld::AddComponentToSelectedActor(std::string_view componentClassName)
	{
		Actor* targetActor = selectedActor_;

		if (!targetActor && selectedComponent_)
		{
			targetActor = selectedComponent_->GetOwner(); // Component選択中なら所有Actorへ追加する
		}

		if (!targetActor)
		{
			lastActorJsonSaveMessage_ = "Add Component failed : no selected Actor";
			return;
		}

		const std::string className{ componentClassName };

		if (!ComponentFactory::IsAllowMultiple(className) && targetActor->HasComponentClass(className))
		{
			lastActorJsonSaveMessage_ = "Add Component failed : already exists" + className;
			return; // alloMutiple = false のComponentはUI以外の経路からも重複追加させない
		}

		// 既にPhysicsWorldへ登録済みなら、一度解除してからComponentを追加する
		const bool wasPhysicsRegistered = targetActor->IsPhysicsRegistered();
		if (wasPhysicsRegistered)
		{
			UnregisterPhysicsComponents(*targetActor);
		}

		ActorComponent* newComponent = nullptr;

		// RootComponentが無いActorにSceneComponent系を追加する場合はRootとして生成する
		if (!targetActor->GetRootComponent())
		{
			newComponent = ComponentFactory::CreateRootSceneComponent(targetActor, componentClassName);
		}

		if (!newComponent)
		{
			newComponent = ComponentFactory::CreateComponent(targetActor, componentClassName);
		}

		if (!newComponent)
		{
			if (wasPhysicsRegistered)
			{
				RegisterPhysicsComponents(*targetActor); // 追加失敗時は元のPhysics登録状態へ戻す
			}

			lastActorJsonSaveMessage_ = "Add Component failed : " + std::string(componentClassName);
			return;
		}

		// Component名が重複しないようにする
		newComponent->SetName(MakeUniqueComponentName(*targetActor, std::string(componentClassName)));

		// SceneComponent系なら、Rootがある場合はRoot配下へAttachする
		if (SceneComponent* newSceneComponent = dynamic_cast<SceneComponent*>(newComponent))
		{
			SceneComponent* rootComponent = targetActor->GetRootComponent();

			if (rootComponent && rootComponent != newSceneComponent)
			{
				newSceneComponent->AttachTo(rootComponent); // 追加したSceneComponentはRoot配下に置く
			}

			newSceneComponent->RefreshWorldTransform();
		}

		if (isInitialized_)
		{
			newComponent->Initialize(); // 追加したComponentだけ初期化する
		}

		if (wasPhysicsRegistered)
		{
			RegisterPhysicsComponents(*targetActor); // Collider/Rigidbody追加に対応するためPhysics登録を作り直す
		}

		selectedActor_ = nullptr;
		selectedComponent_ = newComponent;
		requestFocusActorDetails_ = true;

		lastActorJsonSaveMessage_ = "Added Component : " + newComponent->GetName();
	}

	std::string ActorWorld::MakeUniqueComponentName(const Actor& actor, const std::string& baseName) const
	{
		const std::string safeBaseName = baseName.empty() ? "Component" : baseName; // 空文字の場合はデフォルト名を使用する

		if (!actor.FindComponentByName(safeBaseName))
		{
			return safeBaseName; // 同名のComponentが存在しない場合はそのまま返す
		}

		for (int index = 1; index < 10000; ++index)
		{
			char nameBuffer[256]{};
			std::snprintf(nameBuffer, sizeof(nameBuffer), "%s_%03d", safeBaseName.c_str(), index);

			const std::string candidateName = nameBuffer;

			if (!actor.FindComponentByName(candidateName))
			{
				return candidateName; // 同名のComponentが存在しない場合はその名前を返す
			}
		}

		return safeBaseName + "_Duplicate"; // 10000件以上同名が存在する場合は末尾に_Duplicateを付けて返す
	}

	std::string ActorWorld::MakeUniqueActorName(const std::string& baseName) const
	{
		const std::string safeBaseName = baseName.empty() ? "Actor" : baseName; // 空文字の場合はデフォルト名を使用する

		bool existsSameName = false;

		for (const auto& actor : actors_)
		{
			if (!actor)
			{
				continue; // Actorがnullptrならスキップする
			}

			if (actor->GetName() == safeBaseName)
			{
				existsSameName = true;
				break; // 同名のActorが存在する場合はループを抜ける
			}
		}

		if (!existsSameName)
		{
			return safeBaseName; // 同名のActorが存在しない場合はそのまま返す
		}

		for (int index = 1; index < 10000; ++index)
		{
			char nameBuffer[256]{};
			std::snprintf(nameBuffer, sizeof(nameBuffer), "%s_%03d", safeBaseName.c_str(), index);

			const std::string candidateName = nameBuffer;

			bool exsits = false;

			for (const auto& actor : actors_)
			{
				if (!actor)
				{
					continue; // Actorがnullptrならスキップする
				}

				if (actor->GetName() == candidateName)
				{
					exsits = true;
					break; // 同名のActorが存在する場合はループを抜ける
				}
			}

			if (!exsits)
			{
				return candidateName; // 同名のActorが存在しない場合はその名前を返す
			}
		}

		return safeBaseName + "_Duplicate"; // 10000件以上同名が存在する場合は末尾に_Duplicateを付けて返す
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

	void ActorWorld::ProcessPendingComponentDelete()
	{
		if (!hasPendingDeleteComponent_ || !pendingDeleteComponent_)
		{
			return; // Component削除予約が無い場合は何もしない。
		}

		ActorComponent* deleteComponent = pendingDeleteComponent_;

		hasPendingDeleteComponent_ = false; // 再処理防止。
		pendingDeleteComponent_ = nullptr;

		Actor* owner = deleteComponent->GetOwner();
		if (!owner)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : no owner";
			return;
		}

		// ActorWorldが所有しているActorか確認する。
		bool ownerExists = false;
		for (const auto& actor : actors_)
		{
			if (actor.get() == owner)
			{
				ownerExists = true;
				break;
			}
		}

		if (!ownerExists)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : owner not found";
			return;
		}

		if (deleteComponent == owner->GetRootComponent())
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : RootComponent cannot be deleted";
			return;
		}

		const std::string deletedComponentName = deleteComponent->GetName();

		const bool wasPhysicsRegistered = owner->IsPhysicsRegistered();
		if (wasPhysicsRegistered)
		{
			UnregisterPhysicsComponents(*owner); // Collider/Rigidbodyを消す可能性があるので一度解除する。
		}

		if (selectedComponent_ == deleteComponent)
		{
			selectedComponent_ = nullptr; // 削除対象を選択していた場合は選択解除する。
			selectedActor_ = owner;       // 削除後は所有Actorを選択状態に戻す。
		}

		const bool removed = owner->RemoveComponent(deleteComponent);

		if (wasPhysicsRegistered)
		{
			RegisterPhysicsComponents(*owner); // 残ったCollider/Rigidbodyを再登録する。
		}

		if (!removed)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : " + deletedComponentName;
			return;
		}

		lastActorJsonSaveMessage_ = "Deleted Component : " + deletedComponentName;
	}

	void ActorWorld::DeleteSelectedComponent()
	{
		if (!selectedComponent_)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : no selected Component";
			return;
		}

		Actor* owner = selectedComponent_->GetOwner();
		if (!owner)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : no owner";
			return;
		}

		if (selectedComponent_ == owner->GetRootComponent())
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : RootComponent cannot be deleted";
			return;
		}

		pendingDeleteComponent_ = selectedComponent_;
		hasPendingDeleteComponent_ = true;

		lastActorJsonSaveMessage_ = "Delete Component requested : " + selectedComponent_->GetName();
	}

	void ActorWorld::DuplicateSelectedComponent()
	{
		if (!selectedComponent_)
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : no selected Component";
			return;
		}

		Actor* owner = selectedComponent_->GetOwner();
		if (!owner)
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : no owner Actor";
			return;
		}

		if (selectedComponent_ == owner->GetRootComponent())
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : RootComponent cannot be duplicated";
			return;
		}

		const bool wasPhysicsRegistered = owner->IsPhysicsRegistered();
		if (wasPhysicsRegistered)
		{
			UnregisterPhysicsComponents(*owner); // Physics登録済みのときだけ一度解除する
		}

		nlohmann::json sourceJson;
		selectedComponent_->ToJson(sourceJson); // Serialize / Deserializeを使ってComponent設定を安全に複製する

		if (!sourceJson.contains("Class") || !sourceJson["Class"].is_string())
		{
			if (wasPhysicsRegistered)
			{
				RegisterPhysicsComponents(*owner);
			}

			lastActorJsonSaveMessage_ = "Duplicate Component failed : invalid Component class";
			return;
		}

		const std::string className = sourceJson["Class"].get<std::string>();

		if (!ComponentFactory::IsAllowMultiple(className) && owner->HasComponentClass(className))
		{
			if (wasPhysicsRegistered)
			{
				RegisterPhysicsComponents(*owner);
			}

			lastActorJsonSaveMessage_ = "Duplicate Component failed : already exists " + className;
			return; // Camera / Rigidbodyなど１つだけのComponentは複製させない
		}

		ActorComponent* duplicatedComponent = ComponentFactory::CreateComponent(owner, className);
		if (!duplicatedComponent)
		{
			if (wasPhysicsRegistered)
			{
				RegisterPhysicsComponents(*owner); // 追加失敗時は元のPhysics登録状態へ戻す
			}

			lastActorJsonSaveMessage_ = "Duplicate Component failed : " + className;
			return;
		}

		duplicatedComponent->FromJson(sourceJson);

		const std::string baseName = selectedComponent_->GetName().empty()
			? className
			: selectedComponent_->GetName();

		duplicatedComponent->SetName(MakeUniqueComponentName(*owner, baseName + "_Copy"));

		if (SceneComponent* duplicatedScene = dynamic_cast<SceneComponent*>(duplicatedComponent))
		{
			SceneComponent* sourceScene = dynamic_cast<SceneComponent*>(selectedComponent_);

			if (sourceScene && sourceScene->GetParent())
			{
				duplicatedScene->AttachTo(sourceScene->GetParent());
			}
			else if (owner->GetRootComponent() && owner->GetRootComponent() != duplicatedScene)
			{
				duplicatedScene->AttachTo(owner->GetRootComponent());
			}

			duplicatedScene->RefreshWorldTransform();
		}

		if (isInitialized_)
		{
			duplicatedComponent->Initialize();
		}

		if (wasPhysicsRegistered)
		{
			RegisterPhysicsComponents(*owner); // 複製後のCollider/Rigidbodyを再登録する
		}

		selectedActor_ = nullptr;
		selectedComponent_ = duplicatedComponent;
		requestFocusActorDetails_ = true;

		lastActorJsonSaveMessage_ = "Duplicated Component : " + duplicatedComponent->GetName();
	}

}
