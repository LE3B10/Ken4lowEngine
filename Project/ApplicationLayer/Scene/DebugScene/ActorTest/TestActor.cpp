#include "TestActor.h"
#include <SceneComponent.h>
#include <ModelComponent.h>
#include <CameraComponent.h>
#include <ColliderComponent.h>
#include <RigidbodyComponent.h>

void TestActor::Initialize()
{
	auto& root = CreateRootComponent<Ken4lowEngine::ColliderComponent>();
	root.SetName("Root Collider Component");
	root.SetLocalPosition({ 0.0f, 0.0f, 0.0f });
	root.SetShapeType(Ken4lowEngine::ECollisionShapeType::AABB);
	root.SetHalfSize({ 1.0f, 1.0f, 1.0f });
	root.SetCollisionLayer(Ken4lowEngine::PhysicsCollisionLayer::DynamicActor);

	auto& model = AddComponent<Ken4lowEngine::ModelComponent>();
	model.SetName("Model Component");
	model.SetModelPath("Sample/cube.gltf");
	model.AttachTo(&root);

	auto& camera = AddComponent<Ken4lowEngine::CameraComponent>();
	camera.SetName("Camera Component");
	camera.SetLocalPosition({ 0.0f, 2.0f, -8.0f });
	camera.SetLocalRotation({ 0.2f, 0.0f, 0.0f });
	camera.SetInheritParentRotation(false);
	camera.SetAutoRegisterMainCamera(true);
	camera.AttachTo(&root); // 位置だけActorへ追従し、モデル回転とScaleはCameraへ継承しない。

	auto& rigidbody = AddComponent<Ken4lowEngine::RigidbodyComponent>();
	rigidbody.SetName("Rigidbody Component");
	rigidbody.SetMass(1.0f);
	rigidbody.SetUseGravity(true);

	Actor::Initialize();
}

void TestActor::Update(float deltaTime)
{
	Actor::Update(deltaTime);
}

void TestActor::Draw()
{
	Actor::Draw();
}

void TestActor::DrawShadow()
{
	Actor::DrawShadow();
}

void TestActor::DrawImGui()
{
#ifdef USE_IMGUI
	Actor::DrawImGui();
#endif // USE_IMGUI
}

void TestActor::Finalize()
{
	Actor::Finalize();
}
