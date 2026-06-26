#pragma once
#include "SceneComponent.h"
#include <Collider.h>
#include <PhysicsCollisionLayer.h>

#include <memory>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///	  Actorに当たり判定情報を追加するColliderComponentクラス
	/// -------------------------------------------------------------
	class ColliderComponent : public SceneComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ColliderComponentの初期化処理
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// ColliderComponentの1フレーム更新処理
		/// </summary>
		void Update(float deltaTime) override;

		/// <summary>
		/// PhysicsWorld更新後にColliderのTransformをSceneComponentへ反映する
		/// </summary>
		void PostPhysicsUpdate(float deltaTime) override;

		/// <summary>
		/// ColliderComponentのImGui描画処理
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// ColliderComponentの終了処理
		/// </summary>
		void Finalize() override;

	public: /// ---------- アクセサ ---------- ///

		/// <summary>
		/// Sphere判定の半径を設定する
		/// </summary>
		void SetRadius(float radius) { radius_ = radius; }

		/// <summary>
		/// PhysicWorldへ登録するColliderを取得する
		Collider* GetCollider() { return collider_.get(); }

		/// <summary>
		/// Colliderの形状種別を設定する
		/// </summary>
		void SetShapeType(ECollisionShapeType shapeType) { shapeType_ = shapeType; }

		/// <summary>
		/// Colliderの形状種別を取得する
		/// </summary>
		ECollisionShapeType GetShapeType() const { return shapeType_; }

		/// <summary>
		/// PhysicsWorld用のCollisionLayerを設定する
		/// </summary>
		void SetCollisionLayer(uint32_t layer);

		/// <summary>
		/// PhysicsWorld用のCollisionLayerを名前付きレイヤーで設定する。
		/// </summary>
		void SetCollisionLayer(PhysicsCollisionLayer layer);

		/// <summary>
		/// AABB / OBB 判定に使う半サイズを設定する
		/// </summary>
		void SetHalfSize(const Vector3& halfSize) { halfSize_ = halfSize; }

		/// <summary>
		/// AABB / OBB 判定に使う半サイズを取得する
		/// </summary>
		const Vector3& GetHalfSize() const { return halfSize_; }

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// SceneComponentのWorld位置をCollider中心へ反映する
		/// </summary>
		void SyncFromSceneTransform();

	private: /// ---------- メンバ変数 ---------- ///

		// PhysicWorldへ登録するCollider。SphereColliderとして扱う
		std::unique_ptr<Collider> collider_;

		// Colliderの形状種別。SphereColliderとして扱う
		ECollisionShapeType shapeType_ = ECollisionShapeType::Sphere;

		// AABB / OBBとして扱う半サイズ。デフォルトは1.0fの立方体
		Vector3 halfSize_{ 1.0f, 1.0f, 1.0f };

		// PhysicsWorld用のCollisionLayer
		uint32_t collisionLayer_ = 0;

		// SphereColliderとして扱う半径
		float radius_ = 1.0f;
	};
}