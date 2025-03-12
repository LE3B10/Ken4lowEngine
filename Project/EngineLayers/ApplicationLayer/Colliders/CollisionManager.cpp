#include "CollisionManager.h"
#include "Collider.h"
#include "ParameterManager.h"


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void CollisionManager::Initialize()
{
	isCollider_ = false;
	ParameterManager::GetInstance()->CreateGroup("Collider");
	ParameterManager::GetInstance()->AddItem("Collider", "isCollider", isCollider_);
}


/// -------------------------------------------------------------
///							更新処理
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
///							描画処理
/// -------------------------------------------------------------
void CollisionManager::Draw()
{
	// 非表示なら抜ける
	if (!isCollider_) return;

	// 更新処理
	for (Collider* collider : colliders_)
	{
		collider->Draw();
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
void CollisionManager::RemoveCollider(Collider* collider)
{
	colliders_.remove(collider);
}

bool CollisionManager::IsColliding(Collider* colliderA, Collider* colliderB)
{
	return false;
}


/// -------------------------------------------------------------
///				コライダー２つの衝突判定と応答処理
/// -------------------------------------------------------------
void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB)
{
	// 球体同士の衝突判定
	if (CheckSphereCollisitons(colliderA, colliderB))
	{
		// コライダーAの衝突時コールバックを呼び出す
		colliderA->OnCollision(colliderB);
		// コライダーBの衝突時コールバックを呼び出す
		colliderB->OnCollision(colliderA);

		// 当たっていたら赤に
		colliderA->SetColor({ 1.0f,0.0f,0.0f,1.0f });
		colliderB->SetColor({ 1.0f,0.0f,0.0f,1.0f });
	}
	else
	{
		// 当たっていなかったら白に
		colliderA->SetColor({ 1.0f,1.0f,1.0f,1.0f });
		colliderB->SetColor({ 1.0f,1.0f,1.0f,1.0f });
	}
}


/// -------------------------------------------------------------
///					球体同士の衝突判定処理
/// -------------------------------------------------------------
bool CollisionManager::CheckSphereCollisitons(Collider* colliderA, Collider* colliderB)
{
	// コライダーAの座標を取得
	Vector3 positionA = colliderA->GetCenterPosition();

	// コライダーBの座標を取得
	Vector3 positionB = colliderB->GetCenterPosition();

	// 座標の差分ベクトル
	Vector3 subtract = positionB - positionA;

	// AとBの距離を求める
	float distance = Vector3::Length(subtract);

	// コライダーAとコライダーBの半径の加算
	float radiusSum = colliderA->GetRadius() + colliderB->GetRadius();

	return distance <= radiusSum;
}
