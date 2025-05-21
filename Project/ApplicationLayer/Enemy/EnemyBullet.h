#pragma once
#include <Object3D.h>
#include <Vector3.h>
#include <Collider.h>

class EnemyBullet : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();


	void SetPosition(const Vector3& pos) { position_ = pos; }
	
	void SetVelocity(const Vector3& vel) { velocity_ = vel; }
	
	bool IsDead() const { return lifeTime_ <= 0.0f || isDead_; }

	Vector3 GetCenterPosition() const override { return position_; }

	void OnCollision(Collider* other) override;

private:
	std::unique_ptr<Object3D> model_;
	Vector3 position_ = {};
	Vector3 velocity_ = {};
	float lifeTime_ = 3.0f;
	bool isDead_ = false;
};

