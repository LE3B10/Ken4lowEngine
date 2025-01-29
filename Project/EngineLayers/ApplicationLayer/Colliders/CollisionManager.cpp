#include "CollisionManager.h"
#include "Collider.h"
#include "VectorMath.h"
#include "ParameterManager.h"


void CollisionManager::Initialize()
{
	// グループを追加
	const char* groupName = "Collider";
	ParameterManager::GetInstance()->CreateGroup(groupName);
	ParameterManager::GetInstance()->AddItem(groupName, "isCollider", isCollider_);

}



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
		// 描画
		if (isCollider_)
		{
			collider->Update();
		}
	}
}



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
		// コライダーの更新
		collider->Draw();
	}
}


/// -------------------------------------------------------------
///							リセット処理
/// -------------------------------------------------------------
void CollisionManager::Reset()
{
	colliders_.clear();
	player_ = nullptr;
	floor_.clear();
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

void CollisionManager::DrawParameter()
{
	ParameterManager* globalVariables = ParameterManager::GetInstance();
	isCollider_ = globalVariables->GetValue<bool>("Collider", "isCollider");
}


/// -------------------------------------------------------------
///				コライダー２つの衝突判定と応答処理
/// -------------------------------------------------------------
void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB)
{
	if (IsCollision(colliderA, colliderB))
	{
		colliderA->OnCollision(colliderB);
		colliderB->OnCollision(colliderA);
	}
}


/// -------------------------------------------------------------
///						OBB同士の当たり判定
/// -------------------------------------------------------------
bool CollisionManager::IsCollision(Collider* colliderA, Collider* colliderB)
{
	const float epsilon = 1e-5f; // ゼロベクトルとみなすための閾値

	// Collider A の中心座標、軸、サイズを取得
	Vector3 centerA = colliderA->GetCenterPosition();
	Vector3 orientationsA[3] = {
		colliderA->GetOrientation(0), // X軸方向
		colliderA->GetOrientation(1), // Y軸方向
		colliderA->GetOrientation(2)  // Z軸方向
	};
	Vector3 sizeA = colliderA->GetSize();

	// Collider B の中心座標、軸、サイズを取得
	Vector3 centerB = colliderB->GetCenterPosition();
	Vector3 orientationsB[3] = {
		colliderB->GetOrientation(0), // X軸方向
		colliderB->GetOrientation(1), // Y軸方向
		colliderB->GetOrientation(2)  // Z軸方向
	};
	Vector3 sizeB = colliderB->GetSize();

	// 分離軸のリストを計算
	// 各OBBのローカル軸3本 + 各軸同士の外積9本 = 合計15本
	Vector3 axes[15] = {
		orientationsA[0], orientationsA[1], orientationsA[2], // Collider A の軸
		orientationsB[0], orientationsB[1], orientationsB[2], // Collider B の軸
		Cross(orientationsA[0], orientationsB[0]),            // AのX軸 × BのX軸
		Cross(orientationsA[0], orientationsB[1]),            // AのX軸 × BのY軸
		Cross(orientationsA[0], orientationsB[2]),            // AのX軸 × BのZ軸
		Cross(orientationsA[1], orientationsB[0]),            // AのY軸 × BのX軸
		Cross(orientationsA[1], orientationsB[1]),            // AのY軸 × BのY軸
		Cross(orientationsA[1], orientationsB[2]),            // AのY軸 × BのZ軸
		Cross(orientationsA[2], orientationsB[0]),            // AのZ軸 × BのX軸
		Cross(orientationsA[2], orientationsB[1]),            // AのZ軸 × BのY軸
		Cross(orientationsA[2], orientationsB[2])             // AのZ軸 × BのZ軸
	};

	// 各軸ごとに分離平面の存在を確認
	for (int i = 0; i < 15; ++i)
	{
		// 外積の結果がほぼゼロベクトルの場合、無視（分離軸として有効でない）
		if (Length(axes[i]) < epsilon)
		{
			continue; // 次の軸をチェック
		}

		// 分離軸を正規化（単位ベクトル化）
		Vector3 axis = Normalize(axes[i]);

		// Collider A の投影範囲を計算
		float centerProjectionA = Dot(centerA, axis); // 中心点を軸に投影
		float extentA =
			std::abs(Dot(orientationsA[0] * sizeA.x * 0.5f, axis)) + // X軸方向の半径
			std::abs(Dot(orientationsA[1] * sizeA.y * 0.5f, axis)) + // Y軸方向の半径
			std::abs(Dot(orientationsA[2] * sizeA.z * 0.5f, axis));  // Z軸方向の半径

		// Collider B の投影範囲を計算
		float centerProjectionB = Dot(centerB, axis); // 中心点を軸に投影
		float extentB =
			std::abs(Dot(orientationsB[0] * sizeB.x * 0.5f, axis)) + // X軸方向の半径
			std::abs(Dot(orientationsB[1] * sizeB.y * 0.5f, axis)) + // Y軸方向の半径
			std::abs(Dot(orientationsB[2] * sizeB.z * 0.5f, axis));  // Z軸方向の半径

		// 投影範囲が重なっていない場合、分離軸が存在 → 衝突していない
		if (centerProjectionA + extentA < centerProjectionB - extentB ||
			centerProjectionB + extentB < centerProjectionA - extentA)
		{
			return false; // 分離軸が見つかった → 衝突していない
		}
	}

	// すべての軸で分離が見つからなかった場合、衝突している
	return true;
}
