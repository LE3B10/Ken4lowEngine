#pragma once
#include "ActorComponent.h"

#include <memory>
#include <type_traits>
#include <vector>
#include <utility>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// 			ゲーム内オブジェクトの基底クラス
	/// -------------------------------------------------------------
	class Actor
	{
	public: /// ---------- テンプレート関数 ---------- ///

		/// <summary>
		/// ActorにComponentを追加する。
		/// </summary>
		template<class T, class... Args>
		T& AddComponent(Args&&... args)
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");

			auto component = std::make_unique<T>(std::forward<Args>(args)...);

			auto& ref = *component;	   // push後も呼び出し側が追加したComponentを扱えるように参照を保持しておく
			component->SetOwner(this); // Componentに所有者Actorを設定する

			components_.push_back(std::move(component));
			return ref;
		}

		/// <summary>
		/// 指定型のComponentを取得する
		/// </summary>
		template<class T>
		T* GetComponent()
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");

			for (auto& component : components_)
			{
				if (auto* result = dynamic_cast<T*>(component.get()))
				{
					return result; // 最初に見つかった指定型Componentを返す。
				}
			}

			return nullptr;
		}

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// Actorが持つ全Componentを初期化する
		/// </summary>
		virtual void Initialize();

		/// <summary>
		/// Actorが持つ全Componentを更新する。
		/// </summary>
		virtual void Update(float deltaTime);

		/// <summary>
		/// Actorが持つ全Componentの描画を行う。
		/// </summary>
		virtual void Draw();

		/// <summary>
		/// Actorが持つ全Componentのシャドウ描画を行う。
		/// </summary>
		virtual void DrawShadow();

		/// <summary>
		/// Actorが持つ全ComponentのEditor表示を行う。
		/// </summary>
		virtual void DrawImGui();

		/// <summary>
		/// Actorが持つ全Componentの終了処理を行う。
		/// </summary>
		virtual void Finalize();

	private: /// ---------- メンバ変数 ---------- ///

		// ActorがComponentの寿命を管理するためのコンテナ
		std::vector<std::unique_ptr<ActorComponent>> components_;
	};

}