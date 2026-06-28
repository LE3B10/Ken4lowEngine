#include "ActorWorld.h"
#include "ColliderComponent.h"
#include "RigidbodyComponent.h"
#include "ActorJsonSerializer.h"
#include "CameraComponent.h"

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
	}

	void ActorWorld::PostPhysicsUpdate(float deltaTime)
	{
		for (auto& actor : actors_)
		{
			// PhysicsWorld更新後の処理をActorへ流す。
			actor->PostPhysicsUpdate(deltaTime);
		}
	}

	void ActorWorld::Draw()
	{
		for (auto& actor : actors_)
		{
			// 通常描画を持つActorだけが内部Component経由で描画される
			actor->Draw();
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
				ImGui::Separator();

				selectedComponent_->DrawInspectorImGui(); // 選択中Componentの詳細を描画する。
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

		if (ImGui::Button("Use TestGround"))
		{
			actorPrefabPath_ = "Resources/ActorPrefabs/TestGround.json"; // Prefab PathをTestGroundに設定する
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

}