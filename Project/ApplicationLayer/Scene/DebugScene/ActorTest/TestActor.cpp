#include "TestActor.h"
#include "TestActorComponent.h"
#include <SceneComponent.h>
#include <ModelComponent.h>
#include <CameraComponent.h>
#include <ColliderComponent.h>
#include <RigidbodyComponent.h>

void TestActor::Initialize()
{
	// ActorのRootとして扱う SceneComponentを追加する
	auto& root = CreateRootComponent<Ken4lowEngine::ColliderComponent>();
	root.SetName("Root Collider Component");
	root.SetLocalPosition({ 0.0f, 0.0f, 0.0f });
	root.SetShapeType(Ken4lowEngine::ECollisionShapeType::AABB);
	root.SetHalfSize({ 1.0f, 1.0f, 1.0f });
	root.SetCollisionLayer(Ken4lowEngine::PhysicsCollisionLayer::DynamicActor); // 動くActor用の衝突レイヤーを設定する)

	// ActorにModelComponentを追加する
	auto& model = AddComponent<Ken4lowEngine::ModelComponent>();
	model.SetName("Model Component");
	model.SetModelPath("Sample/cube.gltf");
	model.AttachTo(&root); // Root SceneComponentの子として接続する

	// ActorにColliderComponentを追加する。
	auto& camera = AddComponent<Ken4lowEngine::CameraComponent>();
	camera.SetName("Camera Component");
	camera.SetLocalPosition({ 0.0f, 2.0f, -8.0f });
	camera.SetLocalRotation({ 0.2f, 0.0f, 0.0f });
	camera.SetAutoRegisterMainCamera(true); // MainCameraとして登録する
	camera.AttachTo(&root); // Root SceneComponentの子として接続する

	// ActorにRigidbodyComponentを追加する。
	auto& rigidbody = AddComponent<Ken4lowEngine::RigidbodyComponent>();
	rigidbody.SetName("Rigidbody Component");
	rigidbody.SetMass(1.0f);
	rigidbody.SetUseGravity(true);

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
#ifdef USE_IMGUI
	// Actor基底クラスのImGui描画処理を実行する。
	Actor::DrawImGui();
#endif // USE_IMGUI
}

void TestActor::Finalize()
{
	// Actor基底クラスの終了処理を実行する。
	Actor::Finalize();
}