#pragma once
#include "Actor.h"
#include "PhysicsWorld.h"
#include "ActorSpawnOptions.h"

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

		/// <summary>
		/// ActorWorldが所有する全Actorを初期化する
		/// </summary>
		void Initialize();

		/// <summary>
		/// ActorWorldが所有する全Actorを更新する
		/// </summary>
		void Update(float deltaTime);

		/// <summary>
		/// PhysicsWorld更新後に全Actorへ後処理を流す。
		/// </summary>
		void PostPhysicsUpdate(float deltaTime);

		/// <summary>
		/// ActorWorldが所有する全Actorの通常描画を行う
		/// </summary>
		void Draw();

		/// <summary>
		/// ActorWorldが所有するScreen Space Spriteをまとめて描画する
		/// </summary>
		void DrawScreenSpaceSprites();

		/// <summary>
		/// ActorWorldが所有する全Actorのシャドウ描画を行う
		/// </summary>
		void DrawShadow();

		/// <summary>
		/// ActorWorldが所有する全ActorのEditor表示を行う
		/// </summary>
		void DrawImGui();

		/// <summary>
		/// 選択中ActorまたはComponentのDetailsウィンドウを描画する。
		/// </summary>
		void DrawDetailsImGui();

		/// <summary>
		/// ActorWorldが所有する全Actorの終了処理を行う
		/// </summary>
		void Finalize();

		/// <summary>
		/// 名前からActorを検索する
		/// </summary>
		/// <param name="name">検索するActorの名前</param>
		Actor* FindActorByName(std::string_view name);

		/// <summary>
		/// JSONファイルからActorを生成してActorWorldに追加する
		/// </summary>
		Actor* SpawnActorFromJson(std::string_view filePath, const ActorSpawnOptions& options = {});

	public: /// ---------- Actor一覧取得 ---------- ///

		/// <summary>
		/// ActorWorldが所有するActor一覧を取得する。
		/// </summary>
		const std::vector<std::unique_ptr<Actor>>& GetActors() const
		{
			return actors_; // Editor表示やDebug確認のために読み取り専用でActor一覧を返す
		}

		/// <summary>
		/// 選択中ActorまたはComponentの注視位置を取得する。
		/// </summary>
		bool GetSelectedFocusPosition(Vector3& outPosition) const;

	public: /// ---------- PhysicsWorld設定 ---------- ///

		/// <summary>
		/// ActorWorldで使用するPhysicsWorldを設定する
		/// </summary>
		void SetPhysicsWorld(PhysicsWorld* physicsWorld)
		{
			physicsWorld_ = physicsWorld;  // ActorWorldは所有せず参照だけ保持する
			SetupDefaultPhysicsSettings(); // PhysicsWorldの初期設定を行う
		}

		/// <summary>
		/// ActorWorld用physicsWorldの初期設定を行う
		/// </summary>
		void SetupDefaultPhysicsSettings();

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// ActorWorldで使用するPhysicsWorldを設定する。
		/// </summary>
		void RegisterPhysicsComponents(Actor& actor);

		/// <summary>
		/// Actorが持つPhysics系ComponentをPhysicsWorldから登録解除する
		/// </summary>
		void UnregisterPhysicsComponents(Actor& actor);

		/// <summary>
		/// Actorが持つLightComponentを描画用ライトへ反映する
		/// </summary>
		void SyncLightComponentsToLightManager();

		/// <summary>
		/// Actor JSON読込予約を次フレームの安全なタイミングで処理する
		/// </summary>
		void ProcessPendingActorReload();

		/// <summary>
		/// 指定ActorへJSONを読み込み、PhysicsWorld登録も安全に更新する。
		/// </summary>
		bool ReloadActorFromJson(Actor& actor, const std::string_view filePath);

		/// <summary>
		/// Actor JSON Spawn予約を次フレームの安全なタイミングで処理する
		/// </summary>
		void ProcessPendingActorSpawn();

		/// <summary>
		///	Actor削除予約を次フレームの安全なタイミングで処理する
		/// </summary>
		void ProcessPendingActorDelete();

		/// <summary>
		/// Actor Prefab JSONからSpawnするためのImGuiを描画する
		/// </summary>
		void DrawActorPrefabSpawnImGui();

		/// <summary>
		/// Actor Prefabフォルダ内のJSONファイル一覧を更新する
		/// </summary>
		void RefreshActorPrefabFileList();

		/// <summary>
		/// Actor Prefabフォルダ内のJSONファイル一覧をImGuiで表示する
		/// </summary>
		void DrawActorPrefabBrowserImGui();

		/// <summary>
		/// 選択中ActorをPrefabとして保存するためのImGuiを描画する
		/// </summary>
		void DrawActorPrefabSaveImGui();

		/// <summary>
		/// 選択中ActorへComponentを追加するためのImGuiを描画する
		/// </summary>
		void DrawAddComponentImGui();

		/// <summary>
		/// 選択中Actorへ指定ClassのComponentを追加する
		/// </summary>
		void AddComponentToSelectedActor(std::string_view componentClassName);

		/// <summary>
		/// Actor内で重複しないComponent名を作成する
		/// </summary>
		std::string MakeUniqueComponentName(const Actor& actor, const std::string& baseName) const;

		/// <summary>
		/// ActorWorld内で重複しないActor名を作成する
		/// </summary>
		std::string MakeUniqueActorName(const std::string& baseName) const;

		/// <summary>
		/// 現在選択中のPrefab JSONファイルを削除する
		/// </summary>
		void DeleteSelectedActorPrefabFile();

		/// <summary>
		/// 指定パスがActor Preafabフォルダ内のJSONか確認する
		/// </summary>
		bool IsValidActorPrefabJsonPath(const std::string& filePath) const;

		/// <summary>
		/// Component削除予約を次フレームの安全なタイミングで処理する
		/// </summary>
		void ProcessPendingComponentDelete();

		/// <summary>
		/// 選択中Componentを削除する
		/// </summary>
		void DeleteSelectedComponent();

		/// <summary>
		/// 選択中Componentを複製する
		/// </summary>
		void DuplicateSelectedComponent();

	private: /// ---------- メンバ変数 ---------- ///

		// ActorWorldがActorの寿命を管理する
		std::vector<std::unique_ptr<Actor>> actors_;

		std::string lastActorJsonSaveMessage_; // ActorWorldの最後のJSON保存メッセージを保持する

		// 次フレームSpawn時に使用するオプション
		ActorSpawnOptions pendingSpawnOptions_;

		// Actor World上で選択中のActor
		Actor* selectedActor_ = nullptr;

		// JSON読込を次フレームUpdateで実行するための予約Actor
		Actor* pendingReloadActor_ = nullptr;

		// JSON読込予約中のファイルパス
		std::string pendingReloadFilePath_;

		// JSON生成予約中のファイルパス
		std::string pendingSpawnFilePath_;

		// JSON読込予約中があるかどうか
		bool hasPendingReloadActor_ = false;

		// JSON生成予約中があるかどうか
		bool hasPendingSpawnActor_ = false;

		// Actor削除を次フレームUpdateで実行するための予約Actor
		Actor* pendingDeleteActor_ = nullptr;

		// Component削除を次フレームUpdateで実行するための予約Component
		ActorComponent* pendingDeleteComponent_ = nullptr;

		// Component削除予約中があるかどうか
		bool hasPendingDeleteComponent_ = false;

		// Actor削除予約中があるかどうか
		bool hasPendingDeleteActor_ = false;

		// Actor World上で選択中のActorComponent
		ActorComponent* selectedComponent_ = nullptr;

		// Actor Detailsウィンドウへフォーカスを移す要求
		bool requestFocusActorDetails_ = false;

		// ActorWorldが所有するPhysicsWorldへの参照
		PhysicsWorld* physicsWorld_ = nullptr;

		// ActorWorldが初期化済みかどうかのフラグ
		bool isInitialized_ = false;

		Vector3 actorPrefabSpawnOffset_ = { 3.0f, 0.0f, 0.0f }; // Actor Prefab Spawn時の位置オフセット

		// Editorから追加するComponentの種類選択用
		int selectedAddComponentTypeIndex_ = 0;

		// Actor Prefabフォルダ内で見つかったJSONファイル一覧
		std::vector<std::string> actorPrefabFiles_;

		// Actor Prefabフォルダのパス
		std::string actorPrefabDirectory_ = "Resources/ActorPrefabs";

		// Actor PrefabのJSONパス入力用バッファ
		std::string actorPrefabPath_ = "Resources/ActorPrefabs/TestActor.json";

		// 選択中ActorをPrefabとして保存する際のデフォルトパス
		std::string actorPrefabSavePath_ = "Resources/ActorPrefabs/NewActorPrefab.json";
	};
}
