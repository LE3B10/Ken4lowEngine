#pragma once
#include <string>
#include <string_view>
#include <json.hpp>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///

	// ActorComponentが所有者Actorを参照するための前方宣言
	class Actor;

	/// -------------------------------------------------------------
	/// 		  Actorに追加できる機能単位の基底クラス
	/// -------------------------------------------------------------
	class ActorComponent
	{
	public: /// ---------- コンストラクタ / デストラクタ ---------- ///

		/// <summary>
		/// ActorComponentのデフォルトコンストラクタ。Actorに追加する際に呼ばれる。
		/// </summary>
		ActorComponent() = default;

		/// <summary>
		/// 派生ActorComponentを安全に破棄するための仮想デストラクタ。
		/// </summary>
		virtual ~ActorComponent() = default;

	public: /// ---------- 仮想関数 ---------- ///

		/// <summary>
		/// ActorComponent生成後の初期化処理
		/// </summary>
		virtual void Initialize() {}

		/// <summary>
		/// ActorComponentの1フレーム更新処理
		/// </summary>
		/// <param name="deltaTime">ゲーム時間</param>
		virtual void Update([[maybe_unused]] float deltaTime) {}

		/// <summary>
		/// PhysicsWorld更新後に呼ばれる後処理
		virtual void PostPhysicsUpdate([[maybe_unused]] float deltaTime) {}

		/// <summary>
		/// ゲーム画面に表示する描画処理
		/// </summary>
		virtual void Draw() {}

		/// <summary>
		/// シャドウを描画する処理
		/// </summary>
		virtual void DrawShadow() {}

		/// <summary>
		/// EditorやDebug用のImGui描画処理
		/// </summary>
		virtual void DrawImGui() {}

		/// <summary>
		/// Detailsウィンドウに表示するComponent詳細を描画する。
		/// </summary>
		virtual void DrawInspectorImGui()
		{
			DrawImGui(); // 既存のImGui描画をDetails表示に流用する
		}

		/// <summary>
		/// ActorComponent破棄前の終了処理
		/// </summary>
		virtual void Finalize() {};

	public: /// ---------- 所有者Actorの取得 / 設定 ---------- ///

		/// <summary>
		/// このActorComponentを所有するActorを設定する
		/// </summary>
		/// <param name="owner"></param>
		void SetOwner(Actor* owner)
		{
			owner_ = owner; // ActorComponentはActorを所有せず、参照だけ保持する
		}

		/// <summary>
		/// このActorComponentを所有するActorを取得する
		/// </summary>
		/// <returns></returns>
		Actor* GetOwner() const
		{
			return owner_; // ActorComponentはActorを所有せず、参照だけ保持する
		}

	public: /// ---------- 名前設定 ---------- ///

		/// <summary>
		/// Componentの識別名を設定する。
		/// </summary>
		void SetName(std::string_view name)
		{
			name_ = std::string(name); // string_viewは保持せず、Component側で文字列を所有する
		}

		/// <summary>
		/// Componentの識別名を取得する。
		/// </summary>
		const std::string& GetName() const
		{
			return name_; // Editor表示やComponent検索に使う名前を返すc 
		}

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentクラス名を取得する。
		/// </summary>
		virtual std::string GetClassTypeName() const
		{
			return "ActorComponent"; // Componentの型名を返す。派生クラスでオーバーライドする
		}

		/// <summary>
		/// Component共通情報をJSONへ保存する。
		/// </summary>
		virtual void ToJson(nlohmann::json& outJson)const
		{
			outJson["Name"] = GetName();			// Component名を保存する
			outJson["Class"] = GetClassTypeName();  // Componentの種類を保存する
			outJson["Type"] = "ActorComponent";		// ActorComponent系であることを保存する
		}

	protected: /// ---------- メンバ変数 ---------- ///

		// 所有権はActor側にあり、ActorComponent側ではdeleteしない
		Actor* owner_ = nullptr;

		// Editor上でComponentを識別するための名前
		std::string name_ = "ActorComponent";
	};
}