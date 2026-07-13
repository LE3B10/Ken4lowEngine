#pragma once
#include <Actor.h>
#include <string>

/// -------------------------------------------------------------
///	 Actor・ActorComponent・ActorWorldの動作確認を行うテスト用Actorクラス
/// -------------------------------------------------------------
class TestActor final : public Ken4lowEngine::Actor
{
public: /// ---------- メンバ関数 ---------- ///
	
	/// <summary>
	/// JSON保存・復元で使用するActorクラス名を取得する。
	/// </summary>
	std::string GetClassTypeName() const override
	{
		return "TestActor"; // TestActorとして保存する。
	}

	/// <summary>
	/// TestActor生成後の初期化処理を行う。
	/// </summary>
	void Initialize() override;

};
