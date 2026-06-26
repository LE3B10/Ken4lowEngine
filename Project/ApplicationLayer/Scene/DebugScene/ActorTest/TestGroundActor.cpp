#include "TestGroundActor.h"

#include <SceneComponent.h>
#include <InstancedModelComponent.h>
#include <ColliderComponent.h>
#include <RigidbodyComponent.h>

void TestGroundActor::Initialize()
{
	// Actor全体の基準TransformとしてRootComponentを生成する。
	auto& root = CreateRootComponent<>();
	root.SetLocalPosition({ 0.0f, -3.0f, 0.0f });
	root.SetLocalScale({ 1.0f, 1.0f, 1.0f });

	// GPUインスタンシングで床用のキューブを大量描画する。
	auto& instancedModel = AddComponent<Ken4lowEngine::InstancedModelComponent>();
	instancedModel.SetName("Instanced Ground Model");
	instancedModel.SetModelPath("Sample/cube.gltf");
	instancedModel.SetInstanceCount(100);
	instancedModel.SetSpacing(2.0f);
	instancedModel.SetInstanceScale({ 1.0f, 1.0f, 1.0f });
	instancedModel.AttachTo(&root);

	// 床全体を受け止めるための簡易Colliderを追加する。
	auto& collider = AddComponent<Ken4lowEngine::ColliderComponent>();
	collider.SetName("Ground Collider");
	collider.SetShapeType(Ken4lowEngine::ECollisionShapeType::AABB);
	collider.SetHalfSize({ 10.0f, 1.0f, 10.0f });
	collider.SetCollisionLayer(1); // CollisionLayerを設定する
	collider.AttachTo(&root);

	// 床は物理で動かないStatic Bodyにする。
	auto& rigidbody = AddComponent<Ken4lowEngine::RigidbodyComponent>();
	rigidbody.SetName("Ground Rigidbody");
	rigidbody.SetBodyType(Ken4lowEngine::BodyType::Static);
	rigidbody.SetUseGravity(false);

	// Actor基底クラスの初期化処理を実行する。
	Actor::Initialize();
}
