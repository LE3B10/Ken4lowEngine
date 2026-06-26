#pragma once
#include "Actor.h"
#include "PhysicsWorld.h"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include <typeinfo>

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
			actor->SetName(typeid(T).name()); // Actorの型名をデフォルトの名前として設定する。

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

	public: /// ---------- Actor一覧取得 ---------- ///

		/// <summary>
		/// ActorWorldが所有するActor一覧を取得する。
		/// </summary>
		const std::vector<std::unique_ptr<Actor>>& GetActors() const
		{
			return actors_; // Editor表示やDebug確認のために読み取り専用でActor一覧を返す
		}

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

	private: /// ---------- メンバ変数 ---------- ///

		// ActorWorldがActorの寿命を管理する
		std::vector<std::unique_ptr<Actor>> actors_;

		// Actor World上で選択中のActor
		Actor* selectedActor_ = nullptr;

		// Actor World上で選択中のActorComponent
		ActorComponent* selectedComponent_ = nullptr;

		// Actor Detailsウィンドウへフォーカスを移す要求
		bool requestFocusActorDetails_ = false;

		// ActorWorldが所有するPhysicsWorldへの参照
		PhysicsWorld* physicsWorld_ = nullptr;

		// ActorWorldが初期化済みかどうかのフラグ
		bool isInitialized_ = false;
	};
}