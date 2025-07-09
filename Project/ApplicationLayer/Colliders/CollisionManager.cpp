#include "CollisionManager.h"
#include "ParameterManager.h"
#include "Collider.h"
#include <CollisionUtility.h>
#include <CollisionTypeIdDef.h>


/// -------------------------------------------------------------
///				　			　初期化処理
///	-------------------------------------------------------------
void CollisionManager::Initialize()
{
	isCollider_ = true;
	ParameterManager::GetInstance()->CreateGroup("Collider");
	ParameterManager::GetInstance()->AddItem("Collider", "isCollider", isCollider_);

	RegisterCollisionFuncsions();
}


/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void CollisionManager::Update()
{
	isCollider_ = ParameterManager::GetInstance()->GetValue<bool>("Collider", "isCollider");

	// 非表示なら抜ける
	if (!isCollider_) return;

	// 更新処理
	for (Collider* collider : colliders_)
	{
		collider->Update();
	}
}


/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
void CollisionManager::Draw()
{
	// 非表示なら抜ける
	if (!isCollider_) return;

	// 描画処理
	for (Collider* collider : colliders_)
	{
		if (isCollider_) collider->Draw();
	}
}


/// -------------------------------------------------------------
///							リセット処理
/// -------------------------------------------------------------
void CollisionManager::Reset()
{
	colliders_.clear();
}


/// -------------------------------------------------------------
///				すべての当たり判定を確認する処理
/// -------------------------------------------------------------
void CollisionManager::CheckAllCollisions()
{
	// リスト内のペアを総当たり
	std::list<Collider*>::iterator itrA = colliders_.begin();
	for (; itrA != colliders_.end(); ++itrA)
	{
		Collider* colliderA = *itrA;

		// イテレーターBは入れてータAの次の要素からまわす（重複判定を回避）
		std::list<Collider*>::iterator itrB = itrA;
		itrB++;

		for (; itrB != colliders_.end(); ++itrB)
		{
			Collider* colliderB = *itrB;

			// ペアの当たり判定
			CheckCollisionPair(colliderA, colliderB);
		}
	}
}


/// -------------------------------------------------------------
///				コライダー２つの衝突判定と応答処理
/// -------------------------------------------------------------
void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB)
{
	// 自分同士は無視
	if (colliderA == colliderB) return;

	auto key = std::make_pair(colliderA->GetTypeID(), colliderB->GetTypeID());
	auto it = collisionTable_.find(key);

	if (it != collisionTable_.end())
	{
		if (!it->second(colliderA, colliderB))
		{
			return; // 衝突していない
		}
	}
	else
	{
		return; // 登録されていない型は無視
	}

	colliderA->OnCollision(colliderB);
	colliderB->OnCollision(colliderA);
}


/// -------------------------------------------------------------
///				コライダーの衝突判定関数の登録
/// -------------------------------------------------------------
void CollisionManager::RegisterCollisionFuncsions()
{
	using CollisionType = uint32_t;
	constexpr CollisionType kPlayer = static_cast<CollisionType>(CollisionTypeIdDef::kPlayer);
	constexpr CollisionType kEnemy = static_cast<CollisionType>(CollisionTypeIdDef::kEnemy);
	constexpr CollisionType kBullet = static_cast<CollisionType>(CollisionTypeIdDef::kBullet);
	constexpr CollisionType kEnemyBullet = static_cast<CollisionType>(CollisionTypeIdDef::kEnemyBullet);
	constexpr CollisionType kItem = static_cast<CollisionType>(CollisionTypeIdDef::kItem);
	constexpr CollisionType kBoss = static_cast<CollisionType>(CollisionTypeIdDef::kBoss);

	/// ---------- プレイヤーとボスの衝突判定 ---------- ///
	collisionTable_[{kBoss, kPlayer}] =
		[](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetCapsule(), b->GetSphere());
		};

	collisionTable_[{kPlayer, kBoss}] =
		[](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(b->GetCapsule(), a->GetSphere());
		};


	/// ---------- ボスとプレイヤーの弾丸の衝突判定 ---------- ///
	collisionTable_[{kBoss, kBullet}] =
		[](Collider* a, Collider* b) {
		if (a->HasCapsule())       return CollisionUtility::IsCollision(a->GetCapsule(), b->GetSegment());
		else                       return CollisionUtility::IsCollision(a->GetOBB(), b->GetSegment());
		};

	collisionTable_[{kBullet, kBoss}] =
		[](Collider* a, Collider* b) {
		if (b->HasCapsule())       return CollisionUtility::IsCollision(a->GetSegment(), b->GetCapsule());
		else                       return CollisionUtility::IsCollision(a->GetSegment(), b->GetOBB());
		};

	/// ---------- プレイヤーとボスの弾丸の衝突判定 ---------- ///

	collisionTable_[{kPlayer, kEnemyBullet}] =
		[](Collider* a, Collider* b) {
		if (b->HasCapsule())       return CollisionUtility::IsCollision(a->GetSegment(), b->GetCapsule());
		else                       return CollisionUtility::IsCollision(a->GetSegment(), b->GetOBB());
		};

	collisionTable_[{kEnemyBullet, kPlayer}] =
		[](Collider* a, Collider* b) {
		if (a->HasCapsule())       return CollisionUtility::IsCollision(a->GetCapsule(), b->GetSegment());
		else                       return CollisionUtility::IsCollision(a->GetOBB(), b->GetSegment());
		};

	/// ---------- プレイヤーとアイテムの衝突判定 ---------- ///

	collisionTable_[{kPlayer, kItem}] =
		[](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	collisionTable_[{kItem, kPlayer}] =
		[](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};
}
