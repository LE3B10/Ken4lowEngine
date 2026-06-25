#pragma once
#include "Actor.h"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// Actorを生成・所有・更新するActor用Worldクラス。
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

			auto& ref = *actor; // 登録後も生成したActorを呼び出し側で設定できるように参照を保持する。
			actors_.push_back(std::move(actor));

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
		/// ActorWorldが所有する全Actorの終了処理を行う
		/// </summary>
		void Finalize();

	private: /// ---------- メンバ変数 ---------- ///

		// ActorWorldがActorの寿命を管理する
		std::vector<std::unique_ptr<Actor>> actors_;
	};
}