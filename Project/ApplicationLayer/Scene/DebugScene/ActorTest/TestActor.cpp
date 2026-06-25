#include "TestActor.h"
#include "TestActorComponent.h"

void TestActor::Initialize()
{
	// Actorへテスト用Componentを追加する。
	AddComponent<TestActorComponent>();

	// Actor基底クラスの初期化処理を実行する。
	Actor::Initialize();
}

void TestActor::Update(float deltaTime)
{
	// Actor基底クラスの更新処理を実行する。
	Actor::Update(deltaTime);
}

void TestActor::Draw()
{
	// Actor基底クラスの描画処理を実行する。
	Actor::Draw();
}

void TestActor::DrawShadow()
{
	// Actor基底クラスのシャドウ描画処理を実行する。
	Actor::DrawShadow();
}

void TestActor::DrawImGui()
{
	// Actor基底クラスのImGui描画処理を実行する。
	Actor::DrawImGui();
}

void TestActor::Finalize()
{
	// Actor基底クラスの終了処理を実行する。
	Actor::Finalize();
}