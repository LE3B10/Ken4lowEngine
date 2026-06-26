#include "TestGroundActor.h"

#include <SceneComponent.h>
#include <InstancedModelComponent.h>
#include <ColliderComponent.h>
#include <RigidbodyComponent.h>

void TestGroundActor::Initialize()
{
	// Actor全体の基準TransformとしてRootComponentを生成する。
	auto& root = CreateRootComponent<Ken4lowEngine::ColliderComponent>();
	root.SetName("Root Collider Component");
	root.SetLocalPosition({ 0.0f, -3.0f, 0.0f });
	root.SetLocalScale({ 1.0f, 1.0f, 1.0f });
	root.SetShapeType(Ken4lowEngine::ECollisionShapeType::AABB);
	root.SetHalfSize({ 10.0f, 1.0f, 10.0f });
	root.SetCollisionLayer(Ken4lowEngine::PhysicsCollisionLayer::WorldStatic); // 床用の静的衝突レイヤーを設定する

	// GPUインスタンシングで床用のキューブを大量描画する。
	auto& instancedModel = AddComponent<Ken4lowEngine::InstancedModelComponent>();
	instancedModel.SetName("Instanced Ground Model");
	instancedModel.SetModelPath("Sample/cube.gltf");
	instancedModel.SetInstanceCount(100);
	instancedModel.SetSpacing(2.0f);
	instancedModel.SetInstanceScale({ 1.0f, 1.0f, 1.0f });
	instancedModel.AttachTo(&root);

	// 床は物理で動かないStatic Bodyにする。
	auto& rigidbody = AddComponent<Ken4lowEngine::RigidbodyComponent>();
	rigidbody.SetName("Ground Rigidbody");
	rigidbody.SetBodyType(Ken4lowEngine::BodyType::Static);
	rigidbody.SetUseGravity(false);

	// Actor基底クラスの初期化処理を実行する。
	Actor::Initialize();
}
