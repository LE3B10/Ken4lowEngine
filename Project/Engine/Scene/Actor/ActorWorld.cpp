#include "ActorWorld.h"

namespace Ken4lowEngine
{
	void ActorWorld::Initialize()
	{
		for (auto& actor : actors_)
		{
			// 初期化処理は各Actorに委譲する
			actor->Initialize();
		}
	}

	void ActorWorld::Update(float deltaTime)
	{
		for (auto& actor : actors_)
		{
			// ActorWorldは更新順だけ管理し、処理内容はActor/Component側に任せる
			actor->Update(deltaTime);
		}
	}

	void ActorWorld::Draw()
	{
		for (auto& actor : actors_)
		{
			// 通常描画を持つActorだけが内部Component経由で描画される
			actor->Draw();
		}
	}

	void ActorWorld::DrawShadow()
	{
		for (auto& actor : actors_)
		{
			// 影を落とすActorだけが内部Component経由でShadow描画される
			actor->DrawShadow();
		}
	}

	void ActorWorld::DrawImGui()
	{
		for (auto& actor : actors_)
		{
			// Actor単位でEditor表示を呼び出し、Component編集へつなげる
			actor->DrawImGui();
		}
	}

	void ActorWorld::Finalize()
	{
		for (auto& actor : actors_)
		{
			// Actor破棄前にComponent側のFinalizeまで流す
			actor->Finalize();
		}

		actors_.clear(); // Finalize後にActorを破棄し、古い状態が残らないようにする
	}
}