#pragma once
#include "Vector3.h"
#include "Object3D.h"
#include "Collider.h"
#include "ItemType.h"
#include "Vector4.h"

#include <memory>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///						アイテムクラス
/// -------------------------------------------------------------
class Item : public K4E::Collider
{
public: /// ---------- メンバ関数 --------- ///

	void Initialize(ItemType type, const K4E::Vector3& pos, int healAmount = 25, int ammoAmount = 30, float pickupRadius = 2.0f);
	void Update(float deltaTime);
	void Draw();

	bool CheckCollisionWithPlayer(const K4E::Vector3& playerPos) const;
	bool OnPickup(class Player& player);
	void ApplyTo(class Player* player);
	void MarkCollected()
	{
		active_ = false;
		SetEnabled(false);
	}
	void SetVisualAnimationSettings(float floatHeight, float floatSpeed, float rotationSpeed);
	void SetVisualColor(const K4E::Vector4& color);

	bool IsCollected() const { return !active_; }
	bool IsExpired() const { return lifetime_ >= maxLifetime_; }
	bool IsActive() const { return active_; }

	ItemType GetType() const { return type_; }
	const K4E::Vector3& GetPosition() const { return position_; }
	float GetPickupRadius() const { return pickupRadius_; }
	int GetHealAmount() const { return healAmount_; }
	int GetAmmoAmount() const { return ammoAmount_; }

public: /// ---------- オーバーライド ---------- ///

	void OnCollision(K4E::Collider* other) override;
	void OnOverlapBegin(const K4E::CollisionHit& hit) override;
	K4E::Vector3 GetCenterPosition() const override { return position_; }
	void SetCenterPosition(const K4E::Vector3& pos) override { position_ = pos; }
	K4E::Vector3 GetOBBHalfSize() const override { return scale_; }
	void SetOBBHalfSize(const K4E::Vector3& halfSize) override { scale_ = halfSize; }
	K4E::Vector3 GetOrientation() const override { return rotation_; }
	void SetOrientation(const K4E::Vector3& rot) override { rotation_ = rot; }

private: /// ---------- メンバ変数 ---------- ///

	ItemType type_ = ItemType::None;
	K4E::Vector3 position_ = {};
	bool active_ = false;
	float pickupRadius_ = 2.0f;
	int healAmount_ = 25;
	int ammoAmount_ = 30;

	std::unique_ptr<K4E::Object3D> object3d_;
	K4E::Vector3 scale_ = { 0.4f, 0.4f, 0.4f };
	float floatTimer_ = 0.0f;
	float floatAmplitude_ = 0.6f;
	float floatSpeed_ = 4.0f;
	K4E::Vector3 basePosition_ = {};
	K4E::Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
	float rotationSpeed_ = 0.01f;
	float lifetime_ = 0.0f;
	const float maxLifetime_ = 999.0f;
};
