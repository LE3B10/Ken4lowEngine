#include "CollisionManager.h"
#include "ParameterManager.h"
#include "Collider.h"
#include <CollisionUtility.h>


void CollisionManager::Initialize()
{
	isCollider_ = true;
	ParameterManager::GetInstance()->CreateGroup("Collider");
	ParameterManager::GetInstance()->AddItem("Collider", "isCollider", isCollider_);

	RegisterCollisionFuncsions();
}

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
///						コライダー追加処理
/// -------------------------------------------------------------
void CollisionManager::AddCollider(Collider* other)
{
	colliders_.push_back(other);
}


/// -------------------------------------------------------------
///						コライダー削除処理
/// -------------------------------------------------------------
void CollisionManager::RemoveCollider(Collider* other)
{
	colliders_.remove(other);
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

void CollisionManager::RegisterCollisionFuncsions()
{
	// OBB vs OBB 判定
	collisionTable_[{3, 4}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	collisionTable_[{4, 3}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	collisionTable_[{1, 5}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	collisionTable_[{5, 1}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	//// OBB vs OBB 判定　どっちか
	//auto obb_vs_obb = [](Collider* a, Collider* b) {
	//	return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
	//	};

	//// 共通（Default = 0）やプレイヤー = 1 を仮定してる場合
	//collisionTable_[{3, 4}] = obb_vs_obb; // Enemy vs Bullet
	//collisionTable_[{4, 3}] = obb_vs_obb; // Bullet vs Enemy
}
