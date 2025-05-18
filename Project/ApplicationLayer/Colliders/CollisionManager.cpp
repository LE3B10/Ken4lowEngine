#include "CollisionManager.h"
#include "ParameterManager.h"
#include "Collider.h"


void CollisionManager::Initialize()
{
	isCollider_ = true;
	ParameterManager::GetInstance()->CreateGroup("Collider");
	ParameterManager::GetInstance()->AddItem("Collider", "isCollider", isCollider_);

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

	// 異なるタイプの組み合わせのみ衝突判定する（任意）
	if (colliderA->GetTypeID() == colliderB->GetTypeID()) return;

	// ← ここを修正！
	const OBB obbA = colliderA->GetOBB();
	const OBB obbB = colliderB->GetOBB();

	// 全15軸で分離軸テスト
	for (int i = 0; i < 3; ++i)
	{
		if (IsSeparatingAxis(obbA.orientations[i], obbA, obbB)) return;
		if (IsSeparatingAxis(obbB.orientations[i], obbA, obbB)) return;
	}
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			Vector3 axis = Vector3::Cross(obbA.orientations[i], obbB.orientations[j]);
			if (Vector3::Length(axis) > 0.0001f && IsSeparatingAxis(Vector3::Normalize(axis), obbA, obbB)) return;
		}
	}

	colliderA->OnCollision(colliderB);
	colliderB->OnCollision(colliderA);
}

bool CollisionManager::IsSeparatingAxis(const Vector3& axis, const OBB& obbA, const OBB& obbB)
{
	float projectionA = 0.0f;
	float projectionB = 0.0f;
	float distance = std::abs(Vector3::Dot(axis, obbB.center - obbA.center));

	for (int i = 0; i < 3; ++i)
	{
		projectionA += std::abs(Vector3::Dot(axis, obbA.orientations[i]) * obbA.size[i]);
		projectionB += std::abs(Vector3::Dot(axis, obbB.orientations[i]) * obbB.size[i]);
	}
	return distance > (projectionA + projectionB);
}
