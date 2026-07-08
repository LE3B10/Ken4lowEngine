#include "ActorWorld.h"

#include "ActorJsonSerializer.h"
#include "CameraComponent.h"

#include "LightManager.h"
#include "SceneComponent.h"

#include <cstdio>

namespace Ken4lowEngine
{
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
			if (!actor)
			{
				continue; // nullptrのActorは無視する
			}

			if (!actor->IsActive())
			{
				UnregisterPhysicsComponents(*actor); // 無効なActorは物理登録から外す
				continue;
			}

			if (!actor->IsPhysicsRegistered())
			{
				RegisterPhysicsComponents(*actor); // 再有効化されたActorの物理登録を戻す
			}

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
			if (!actor || !actor->IsActive())
			{
				continue; // 無効なActorは物理後更新対象から外す
			}

			// PhysicsWorld更新後の処理をActorへ流す。
			actor->PostPhysicsUpdate(deltaTime);
		}

		SyncLightComponentsToLightManager(); // 物理更新後のWorld位置をライトへ反映する
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
		LightManager::GetInstance()->SetLightComponentLights({}); // ActorWorld破棄時にComponent由来ライトを解除する

		isInitialized_ = false; // 再Initialize時にSpawn済みActorを通常初期化できるように戻す
	}

	Actor* ActorWorld::FindActorByName(std::string_view name, bool includeInactive)
	{
		for (auto& actor : actors_)
		{
			if (!actor || (!includeInactive && !actor->IsActive()))
			{
				continue; // 無効なActorを除外する指定なら検索対象から外す
			}

			// Actor名が一致した最初のActorを返す
			if (actor->GetName() == name)
			{
				return actor.get();
			}
		}

		return nullptr; // 名前が一致するActorが見つからなかった場合はnullptrを返す
	}

	std::vector<Actor*> ActorWorld::FindActorsWithTag(std::string_view tag, bool includeInactive)
	{
		std::vector<Actor*> results;
		if (tag.empty())
		{
			return results; // 空Tagでは検索しない
		}

		const std::string tagString{ tag };
		for (auto& actor : actors_)
		{
			if (!actor || (!includeInactive && !actor->IsActive()))
			{
				continue; // nullptrや除外対象のInactive Actorは無視する
			}

			if (actor->HasTag(tagString))
			{
				results.push_back(actor.get());
			}
		}

		return results;
	}

	std::vector<Actor*> ActorWorld::FindActorsByLayer(std::string_view layer, bool includeInactive)
	{
		std::vector<Actor*> results;
		const std::string layerString = layer.empty() ? "Default" : std::string(layer);

		for (auto& actor : actors_)
		{
			if (!actor || (!includeInactive && !actor->IsActive()))
			{
				continue; // nullptrや除外対象のInactive Actorは無視する
			}

			if (actor->GetLayer() == layerString)
			{
				results.push_back(actor.get());
			}
		}

		return results;
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
}
