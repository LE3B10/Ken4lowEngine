#pragma once
#include "Actor.h"
#include "PhysicsWorld.h"
#include "ActorSpawnOptions.h"
#include "ActorJsonSerializer.h"

#include <json.hpp>
#include <memory>
#include <type_traits>
#include <utility>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///		  Actorを生成・所有・更新するActor用Worldクラス
	/// -------------------------------------------------------------
	class ActorWorld
	{
	public: /// ---------- テンプレート関数 ---------- ///

		/// <summary>
		/// ActorWorld内にActorを生成して登録する。
		/// </summary>
		template<class T, class... Args>
		T& SpawnActor(Args&&... args)
		{
			static_assert(std::is_base_of_v<Actor, T>, "T must inherit from Actor.");

			auto actor = std::make_unique<T>(std::forward<Args>(args)...);
			actor->SetName(actor->GetClassTypeName()); // Actorの型名をデフォルトの名前として設定する。

			auto& ref = *actor; // 登録後も生成したActorを呼び出し側で設定できるように参照を保持する。
			actors_.push_back(std::move(actor));

			if (isInitialized_)
			{
				ref.Initialize(); // 実行中にSpawnされたActorを即初期化する
				RegisterPhysicsComponents(ref); // Collider/RigidbodyをPhysicsWorldへ自動登録する
			}

			return ref;
		}

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>ActorWorldが所有する全Actorを初期化する</summary>
		void Initialize();

		/// <summary>ActorWorldが所有する全Actorを更新する</summary>
		void Update(float deltaTime);

		/// <summary>PhysicsWorld更新後に全Actorへ後処理を流す。</summary>
		void PostPhysicsUpdate(float deltaTime);

		/// <summary>ActorWorldが所有する全Actorの通常描画を行う</summary>
		void Draw();

		/// <summary>ActorWorldが所有するScreen Space UI Componentをまとめて描画する</summary>
		void DrawScreenSpaceUI();

		/// <summary>既存呼び出しとの互換用にScreen Space UI描画へ転送する</summary>
		void DrawScreenSpaceSprites();

		/// <summary>ActorWorldが所有する全Actorのシャドウ描画を行う</summary>
		void DrawShadow();

		/// <summary>
		/// ActorWorldが所有する全ActorのEditor表示を行う。
		/// Phase 3以降はWorld Outliner / Detailsへ統合し、既定では旧ウィンドウを表示しない。
		/// </summary>
		void DrawImGui();

		/// <summary>選択中ActorまたはComponentのDetailsウィンドウを描画する。</summary>
		void DrawDetailsImGui();

		/// <summary>統合Detailsパネル内へ選択中ActorまたはComponentの編集UIだけを描画する。</summary>
		void DrawSelectedInspectorContent();

		/// <summary>World Outlinerから選択されたActor / ComponentをActorWorld側へ同期する。</summary>
		void SetSelectedEditorObject(Actor* actor, ActorComponent* component)
		{
			selectedActor_ = actor;
			selectedComponent_ = component;
		}

		/// <summary>旧Actor World / Actor Detailsウィンドウを互換表示するか設定する。</summary>
		void SetLegacyEditorWindowsEnabled(bool enabled) { legacyEditorWindowsEnabled_ = enabled; }
		bool IsLegacyEditorWindowsEnabled() const { return legacyEditorWindowsEnabled_; }

		/// <summary>ActorWorldが所有する全Actorの終了処理を行う</summary>
		void Finalize();

		/// <summary>名前からActorを検索する</summary>
		Actor* FindActorByName(std::string_view name, bool includeInactive = true);

		/// <summary>指定Tagを持つActorを検索する</summary>
		std::vector<Actor*> FindActorsWithTag(std::string_view tag, bool includeInactive = true);

		/// <summary>指定Layerに所属するActorを検索する</summary>
		std::vector<Actor*> FindActorsByLayer(std::string_view layer, bool includeInactive = true);

		/// <summary>JSONファイルからActorを生成してActorWorldに追加する</summary>
		Actor* SpawnActorFromJson(std::string_view filePath, const ActorSpawnOptions& options = {});

		/// <summary>Level JSON内へ埋め込まれたActor JSONからActorを生成する。</summary>
		Actor* SpawnActorFromJson(const nlohmann::json& actorJson, const ActorSpawnOptions& options = {})
		{
			std::unique_ptr<Actor> actor = ActorJsonSerializer::CreateActorFromJson(actorJson, options);
			if (!actor)
			{
				lastActorJsonSaveMessage_ = "Load failed : inline Actor JSON";
				return nullptr;
			}

			Actor* spawnedActor = actor.get();
			spawnedActor->SetName(MakeUniqueActorName(spawnedActor->GetName())); // Level内でもActor名の一意性を既存Spawn規則へ統一する。
			actors_.push_back(std::move(actor));

			if (isInitialized_)
			{
				RegisterPhysicsComponents(*spawnedActor); // Serializer初期化済みComponentを現在のPhysicsWorldへ登録する。
			}

			selectedActor_ = spawnedActor;
			selectedComponent_ = nullptr;
			lastActorJsonSaveMessage_ = "Spawned : inline Actor JSON";
			return spawnedActor;
		}

	public: /// ---------- Actor一覧取得 ---------- ///

		/// <summary>ActorWorldが所有するActor一覧を取得する。</summary>
		const std::vector<std::unique_ptr<Actor>>& GetActors() const
		{
			return actors_; // Editor表示やDebug確認のために読み取り専用でActor一覧を返す
		}

		/// <summary>選択中ActorまたはComponentの注視位置を取得する。</summary>
		bool GetSelectedFocusPosition(Vector3& outPosition) const;

	public: /// ---------- PhysicsWorld設定 ---------- ///

		/// <summary>ActorWorldで使用するPhysicsWorldを設定する</summary>
		void SetPhysicsWorld(PhysicsWorld* physicsWorld)
		{
			physicsWorld_ = physicsWorld;  // ActorWorldは所有せず参照だけ保持する
			SetupDefaultPhysicsSettings(); // PhysicsWorldの初期設定を行う
		}

		/// <summary>ActorWorld用physicsWorldの初期設定を行う。</summary>
		void SetupDefaultPhysicsSettings();

	private: /// ---------- 内部処理 ---------- ///

		void RegisterPhysicsComponents(Actor& actor);
		void UnregisterPhysicsComponents(Actor& actor);
		void SyncLightComponentsToLightManager();
		void ProcessPendingActorReload();
		bool ReloadActorFromJson(Actor& actor, const std::string_view filePath);
		void ProcessPendingActorSpawn();
		void ProcessPendingActorDelete();
		void DrawActorPrefabSpawnImGui();
		void RefreshActorPrefabFileList();
		void DrawActorPrefabBrowserImGui();
		void ProcessPendingComponentDelete();
		void DuplicateSelectedActor();
		void DuplicateSelectedComponent();
		void DeleteSelectedActor();
		void DeleteSelectedComponent();
		std::string MakeUniqueActorName(const std::string& baseName) const;

	private: /// ---------- メンバ変数 ---------- ///
		std::vector<std::unique_ptr<Actor>> actors_;
		PhysicsWorld* physicsWorld_ = nullptr;
		Actor* selectedActor_ = nullptr;
		ActorComponent* selectedComponent_ = nullptr;
		bool legacyEditorWindowsEnabled_ = false;
		bool isInitialized_ = false;
		bool hasPendingReloadActor_ = false;
		Actor* pendingReloadActor_ = nullptr;
		std::string pendingReloadFilePath_;
		bool hasPendingSpawnActor_ = false;
		std::string pendingSpawnActorFilePath_;
		Vector3 pendingSpawnOffset_{};
		Actor* pendingDeleteActor_ = nullptr;
		ActorComponent* pendingDeleteComponent_ = nullptr;
		std::vector<std::string> actorPrefabFilePaths_;
		int selectedActorPrefabIndex_ = -1;
		std::string lastActorJsonSaveMessage_;
	};
} // namespace Ken4lowEngine
