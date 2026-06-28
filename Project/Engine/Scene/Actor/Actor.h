#pragma once
#include "ActorComponent.h"
#include "SceneComponent.h"

#include <memory>
#include <type_traits>
#include <vector>
#include <utility>
#include <string>
#include <string_view>
#include <json.hpp>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// 			ゲーム内オブジェクトの基底クラス
	/// -------------------------------------------------------------
	class Actor
	{
	public: /// ---------- テンプレート関数 ---------- ///

		/// <summary>
		/// ActorにComponentを追加する
		/// </summary>
		template<class T, class... Args>
		T& AddComponent(Args&&... args)
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");

			auto component = std::make_unique<T>(std::forward<Args>(args)...);
			component->SetName(component->GetClassTypeName()); // Componentの型名をデフォルト名として設定する

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
					return result; // 最初に見つかった指定型Componentを返す
				}
			}

			return nullptr;
		}

		/// <summary>
		/// 指定型のComponentをすべて取得する
		/// </summary>
		template<class T>
		std::vector<T*> GetComponents()
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");

			std::vector<T*> results;
			for (auto& component : components_)
			{
				if (auto* result = dynamic_cast<T*>(component.get()))
				{
					results.push_back(result); // 指定型に一致したComponentを一覧へ追加する
				}
			}
			return results;
		}

		/// <summary>
		/// 指定型のComponentをすべて取得する
		/// </summary>
		template<class T>
		std::vector<const T*> GetComponents() const
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");

			std::vector<const T*> results;
			for (const auto& component : components_)
			{
				if (const auto* result = dynamic_cast<T*>(component.get()))
				{
					results.push_back(result); // 指定型に一致したComponentを一覧へ追加する
				}
			}
			return results;
		}

		/// <summary>
		/// Actorの基準となるRootComponentを生成して設定する
		/// </summary>
		template<class T = SceneComponent, class... Args>
		T& CreateRootComponent(Args&&... args)
		{
			static_assert(std::is_base_of_v<SceneComponent, T>, "T must inherit from SceneComponent.");

			auto& root = AddComponent<T>(std::forward<Args>(args)...);
			root.SetName(root.GetClassTypeName()); // RootComponentの型名をデフォルト名として設定する

			if (!rootComponent_)
			{
				rootComponent_ = &root;  // Actor全体の基準Transformとして保持する
			}

			return root;
		}

	public: /// ---------- 仮想関数 ---------- ///

		/// <summary>
		/// 派生Actorを安全に破棄するための仮想デストラクタ
		/// </summary>
		virtual ~Actor() = default;

		/// <summary>
		/// Actorが持つ全Componentを初期化する
		/// </summary>
		virtual void Initialize();

		/// <summary>
		/// Actorが所有するComponentのみを初期化する
		/// </summary>
		void InitializeComponents();

		/// <summary>
		/// Actorが持つ全Componentを更新する
		/// </summary>
		virtual void Update(float deltaTime);

		/// <summary>
		/// PhysicsWorld更新後に全Componentへ後処理を流す。
		/// </summary>
		virtual void PostPhysicsUpdate(float deltaTime);

		/// <summary>
		/// Actorが持つ全Componentの描画を行う
		/// </summary>
		virtual void Draw();

		/// <summary>
		/// Actorが持つ全Componentのシャドウ描画を行う
		/// </summary>
		virtual void DrawShadow();

		/// <summary>
		/// Actorが持つ全ComponentのEditor表示を行う
		/// </summary>
		virtual void DrawImGui();

#ifdef USE_IMGUI
		/// <summary>
		/// ActorWorld上にActorとComponent階層を表示する。
		/// </summary>
		void DrawHierarchyImGui(Actor*& selectedActor, ActorComponent*& selectedComponent);

		/// <summary>
		/// Actorが所有するComponentを階層表示する。
		/// </summary>
		void DrawComponentHierarchyImGui(Actor*& selectedActor, ActorComponent*& selectedComponent);

		/// <summary>
		/// Detailsウィンドウに表示するActor詳細を描画する。
		/// </summary>
		void DrawInspectorImGui();
#endif // USE_IMGUI

		/// <summary>
		/// Actorが持つ全Componentの終了処理を行う
		/// </summary>
		virtual void Finalize();

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するActorクラス名を取得する
		/// </summary>
		virtual std::string GetClassTypeName() const
		{
			return "Actor"; // 基底Actorの型名を返す。派生Actorでオーバーライドする。
		}

		/// <summary>
		/// Actorと所有Component情報をJSONへ保存する。
		/// </summary>
		virtual void ToJson(nlohmann::json& outJson) const;

		/// <summary>
		/// JSONからActor共通情報を復元する
		/// </summary>
		virtual void FromJson(const nlohmann::json& inJson);

		/// <summary>
		/// Actorが所有するComponentをすべて破棄する。
		/// </summary>
		void ClearComponents();

	public: /// ---------- Actor破棄フラグ設定 ---------- ///

		/// <summary>
		/// Actorを削除予約状態にする。
		/// </summary>
		void Destroy()
		{
			isPendingDestroy_ = true; // ActorWorldの安全なタイミングで削除されるように予約する
		}

		/// <summary>
		/// Actorが削除予約中華取得する
		/// </summary>
		bool IsPendingDestroy() const
		{
			return isPendingDestroy_; // ActorWorld側の削除判定に使う
		}

	public: /// ---------- Component一覧取得 ---------- ///

		/// <summary>
		/// Actorが所有するComponent一覧を取得する
		/// </summary>
		const std::vector<std::unique_ptr<ActorComponent>>& GetComponents() const
		{
			return components_; // Editor表示用に読み取り専用でComponent一覧を返す
		}

	public: /// ---------- 名前設定 ---------- ///

		/// <summary>
		/// Actorの識別名を設定する
		/// </summary>
		void SetName(std::string_view name)
		{
			name_ = std::string(name); // string_viewは保持せず、Actor側で文字列を所有する
		}

		/// <summary>
		/// Actorの識別名を取得する
		/// </summary>
		const std::string& GetName() const
		{
			return name_; // Actor検索に使う名前を返す
		}

		/// <summary>
		/// 名前からComponentを検索する。
		/// </summary>
		ActorComponent* FindComponentByName(std::string_view name);

		/// <summary>
		/// 名前からComponentを検索する。
		/// </summary>
		const ActorComponent* FindComponentByName(std::string_view name) const;

	public: /// ---------- RootComponent ---------- ///

		/// <summary>
		/// Actorの基準となるRootComponentを設定する。
		/// </summary>
		void SetRootComponent(SceneComponent* rootComponent)
		{
			rootComponent_ = rootComponent; // ActorのRootComponentを設定する
		}

		/// <summary>
		/// Actorの基準となるRootComponentを取得する。
		/// </summary>
		SceneComponent* GetRootComponent() const
		{
			return rootComponent_; // Modelや子Componentが参照する基準Transformを返す
		}

	public: /// ---------- PhysicsWorld登録状態 ---------- ///

		/// <summary>
		/// PhysicsWorldへ登録済みか設定する。
		/// </summary>
		void SetPhysicsRegistered(bool isRegistered)
		{
			isPhysicsRegistered_ = isRegistered; // PhysicsWorldへの登録状態を設定する
		}

		/// <summary>
		/// PhysicsWorldへ登録済みか取得する。
		/// </summary>
		bool IsPhysicsRegistered() const
		{
			return isPhysicsRegistered_; // PhysicsWorldへの登録状態を返す
		}

	private: /// ---------- メンバ変数 ---------- ///

		// Actor全体の基準Transform。所有権はcomponents_側が持つ
		SceneComponent* rootComponent_ = nullptr;

		// ActorがComponentの寿命を管理するためのコンテナ
		std::vector<std::unique_ptr<ActorComponent>> components_;

		// ActorWorldやEditor上で識別するための名前
		std::string name_ = "Actor";

		// 更新中に即削除せず、ActorWorld側で安全に破棄するためのフラグ
		bool isPendingDestroy_ = false;

		// PhysicsWorldへ登録済みかどうか
		bool isPhysicsRegistered_ = false;
	};

}