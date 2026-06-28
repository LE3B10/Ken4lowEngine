#pragma once
#include <string>
#include <string_view>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///

	// ActorJsonSerializerがActorを参照するための前方宣言
	class Actor;

	/// -------------------------------------------------------------
	///		Actor・ActorComponentのJSONシリアライズを行うクラス
	/// -------------------------------------------------------------
	class ActorJsonSerializer
	{
	public: /// ---------- 静的メンバ関数 ---------- ///

		/// <summary>
		/// Actor構成をJSONファイルへ保存する。
		/// </summary>
		static bool SaveActorToFile(const Actor& actor, std::string_view filePath);

		/// <summary>
		/// JSONファイルからActorへComponent構成を読み込む
		/// </summary>
		static bool LoadActorFromFile(Actor& actor, std::string_view filePath);
	};
}