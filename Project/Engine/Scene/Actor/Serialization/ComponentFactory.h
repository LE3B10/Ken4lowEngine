#pragma once
#include <string_view>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class Actor;
	class ActorComponent;
	class SceneComponent;

	/// -------------------------------------------------------------
	///		  JSONのClass文字列からComponentを生成するFactory
	/// -------------------------------------------------------------
	class ComponentFactory
	{
	public: /// ---------- 静的メンバ関数 ---------- ///

		/// <summary>
		/// 指定されたClass名に対応するComponentをActorへ追加する
		/// </summary>
		static ActorComponent* CreateComponent(Actor* owner, std::string_view className);

		/// <summary>
		/// 指定されたClass名に対応するSceneComponentをRootComponentとして生成する
		/// </summary>
		static SceneComponent* CreateRootSceneComponent(Actor* owner, std::string_view className);
	};
}