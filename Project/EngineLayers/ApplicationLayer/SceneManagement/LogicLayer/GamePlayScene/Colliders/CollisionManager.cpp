#include "CollisionManager.h"
#include "Collider.h"
#include "Vec3Func.h"

#ifdef _DEBUG
#include"ImGui.h"
#endif

using namespace Vec3;


/// -------------------------------------------------------------
///					初期化処理・可視化処理用
/// -------------------------------------------------------------
void CollisionManager::Initialize()
{
	// 非表示なら抜ける
	if (!isCollider_)
	{
		return;
	}

	// すべてのコライダーについて
	for (Collider* collider : colliders_)
	{
		if (isCollider_)
		{
			// コライダーの初期化処理
			collider->Initialize();
		}
	}
}


/// -------------------------------------------------------------
///					更新処理・可視化処理用
/// -------------------------------------------------------------
void CollisionManager::Update()
{
	// 非表示なら抜ける
	if (!isCollider_)
	{
		return;
	}

	// すべてのコライダーについて
	for (Collider* collider : colliders_)
	{
		if (isCollider_)
		{
			// コライダーの更新処理
			collider->Update();
		}
	}
}


/// -------------------------------------------------------------
///					描画処理・可視化処理用
/// -------------------------------------------------------------
void CollisionManager::Draw()
{
	// 非表示なら抜ける
	if (!isCollider_)
	{
		return;
	}

	// すべてのコライダーについて
	for (Collider* collider : colliders_)
	{
		if (isCollider_)
		{
			// コライダーの描画処理
			collider->Draw();
		}
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

			printf("Checking collision between %d and %d\n", colliderA->GetTypeID(), colliderB->GetTypeID());

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
///						ImGuiの描画
/// -------------------------------------------------------------
void CollisionManager::DrawImGui()
{
#ifdef _DEBUG

	ImGui::Begin("Cillider");
	if (ImGui::Button(isCollider_ ? "Collider is visible" : "Collider is invisible"))
	{
		isCollider_ = !isCollider_;
	}
	ImGui::End();

#endif // _DEBUG
}


/// -------------------------------------------------------------
///				コライダー２つの衝突判定と応答処理
/// -------------------------------------------------------------
void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB)
{
	if (IsAABBCollision(colliderA, colliderB)) // AABB で大まかに判定
	{
		// コライダーAの衝突時コールバックを呼び出す
		colliderA->OnCollision(colliderB);

		// コライダーBの衝突時コールバックを呼び出す
		colliderB->OnCollision(colliderA);
	}
}


/// -------------------------------------------------------------
///			　AABB同士の衝突判定処理（静的衝突判定用）
/// -------------------------------------------------------------
bool CollisionManager::IsAABBCollision(Collider* colliderA, Collider* colliderB)
{
	Vector3 minA = colliderA->GetAABBMin();
	Vector3 maxA = colliderA->GetAABBMax();

	Vector3 minB = colliderB->GetAABBMin();
	Vector3 maxB = colliderB->GetAABBMax();

	printf("AABB A: min(%f, %f, %f), max(%f, %f, %f)\n", minA.x, minA.y, minA.z, maxA.x, maxA.y, maxA.z);
	printf("AABB B: min(%f, %f, %f), max(%f, %f, %f)\n", minB.x, minB.y, minB.z, maxB.x, maxB.y, maxB.z);

	return
		(minA.x < maxB.x && maxA.x > minB.x) &&
		(minA.y < maxB.y && maxA.y > minB.y) &&
		(minA.z < maxB.z && maxA.z > minB.z);
}

