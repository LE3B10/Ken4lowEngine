#include "TestActor.h"
#include <SceneComponent.h>
#include <ModelComponent.h>
#include <CameraComponent.h>
#include <ColliderComponent.h>
#include <RigidbodyComponent.h>

void TestActor::Initialize()
{
	if (!GetComponents().empty())
	{
		Actor::Initialize();
		return; // JSON復元済み構成へ既定Componentを重複追加しない。
	}

	auto& root = CreateRootComponent<Ken4lowEngine::ColliderComponent>();
	root.SetName("Root Collider Component");
	root.SetLocalPosition({ 0.0f, 0.0f, 0.0f });
	root.SetShapeType(Ken4lowEngine::ECollisionShapeType::AABB);
	root.SetHalfSize({ 1.0f, 1.0f, 1.0f });
	root.SetCollisionLayer(Ken4lowEngine::PhysicsCollisionLayer::DynamicActor);
	root.SetUpdateOrder(-100); // TransformとColliderを描画Componentより先に同期する。

	auto& model = AddComponent<Ken4lowEngine::ModelComponent>();
	model.SetName("Model Component");
	model.SetModelPath("Sample/cube.gltf");
	model.SetUpdateOrder(0);
	model.SetDrawOrder(0);
	model.AttachTo(&root);

	auto& camera = AddComponent<Ken4lowEngine::CameraComponent>();
	camera.SetName("Camera Component");
	camera.SetLocalPosition({ 0.0f, 2.0f, -8.0f });
	camera.SetLocalRotation({ 0.2f, 0.0f, 0.0f });
	camera.SetInheritParentRotation(false);
	camera.SetAutoRegisterMainCamera(true);
	camera.SetUpdateOrder(100); // 親Transform確定後にCameraを更新する。
	camera.AttachTo(&root); // 位置だけActorへ追従し、Actorの回転とスケールはCameraへ継承しない。

	auto& rigidbody = AddComponent<Ken4lowEngine::RigidbodyComponent>();
	rigidbody.SetName("Rigidbody Component");
	rigidbody.SetMass(1.0f);
	rigidbody.SetUseGravity(true);
	rigidbody.SetUpdateOrder(-50); // Collider同期後、通常描画Componentより前に物理状態を読む。

	Actor::Initialize();
}
