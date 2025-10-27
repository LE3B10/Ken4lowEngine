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

	// 衝突判定関数の登録
	RegisterCollisionFuncsions();
}


/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void CollisionManager::Update()
{
	isCollider_ = ParameterManager::GetInstance()->GetValue<bool>("Collider", "isCollider");

	// 更新処理
	for (Collider* collider : all_) collider->Update();
}


/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
void CollisionManager::Draw()
{
	// 非表示なら抜ける
	if (!isCollider_) return;

	// 描画処理
	for (Collider* collider : all_)
		if (isCollider_) collider->Draw();
}


/// -------------------------------------------------------------
///							リセット処理
/// -------------------------------------------------------------
void CollisionManager::Reset()
{
	all_.clear();
	for (auto& v : buckets_) v.clear(); // 型ごとのバケットも空にする
}


/// -------------------------------------------------------------
///				すべての当たり判定を確認する処理
/// -------------------------------------------------------------
void CollisionManager::CheckAllCollisions()
{
	using CId = uint32_t;
	const CId kPlayer = static_cast<CId>(CollisionTypeIdDef::kPlayer);
	const CId kEnemy = static_cast<CId>(CollisionTypeIdDef::kEnemy);
	const CId kBoss = static_cast<CId>(CollisionTypeIdDef::kBoss);
	const CId kBullet = static_cast<CId>(CollisionTypeIdDef::kBullet);
	const CId kBossBullet = static_cast<CId>(CollisionTypeIdDef::kBossBullet);
	const CId kItem = static_cast<CId>(CollisionTypeIdDef::kItem);
	const CId kWorld = static_cast<CId>(CollisionTypeIdDef::kWorld);

	auto pairLoop = [&](CId aId, CId bId) {
		auto& A = buckets_[aId];
		auto& B = buckets_[bId];
		if (A.empty() || B.empty()) return;
		for (Collider* a : A) for (Collider* b : B) {
			CheckCollisionPair(a, b);
		}
		};

	// 片方向だけにする（OnCollisionはCheckCollisionPair内で両者に通知済み）
	pairLoop(kBoss, kPlayer);
	pairLoop(kEnemy, kPlayer);
	pairLoop(kBullet, kEnemy);
	pairLoop(kEnemy, kBullet);
	pairLoop(kBoss, kBullet);
	pairLoop(kPlayer, kBossBullet);
	pairLoop(kPlayer, kItem);
	pairLoop(kItem, kPlayer);
	pairLoop(kPlayer, kWorld);
	pairLoop(kWorld, kPlayer);
}

/// -------------------------------------------------------------
///						コライダーを追加
/// -------------------------------------------------------------
void CollisionManager::AddCollider(Collider* other)
{
	all_.push_back(other);
	const uint32_t id = other->GetTypeID();
	if (id < kMaxTypes) buckets_[id].push_back(other);
}

/// -------------------------------------------------------------
///						コライダーを削除
/// -------------------------------------------------------------
void CollisionManager::RemoveCollider(Collider* other)
{
	// all から削除
	all_.erase(std::remove(all_.begin(), all_.end(), other), all_.end());

	// バケットから削除
	const uint32_t id = other->GetTypeID();
	if (id < kMaxTypes)
	{
		auto& v = buckets_[id];
		v.erase(std::remove(v.begin(), v.end(), other), v.end());
	}
}

/// -------------------------------------------------------------
///				コライダー２つの衝突判定と応答処理
/// -------------------------------------------------------------
void CollisionManager::CheckCollisionPair(Collider* colliderA, Collider* colliderB)
{
	// 自分同士は無視
	if (colliderA == colliderB) return;

	// 衝突判定関数を取得
	auto key = std::make_pair(colliderA->GetTypeID(), colliderB->GetTypeID());

	// 衝突判定関数を検索
	auto it = collisionTable_.find(key);

	// 衝突判定関数が登録されているか確認
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

	colliderA->OnCollision(colliderB); // Bの衝突応答処理
	colliderB->OnCollision(colliderA); // Aの衝突応答処理
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
	//constexpr CollisionType kEnemyBullet = static_cast<CollisionType>(CollisionTypeIdDef::kEnemyBullet);
	constexpr CollisionType kItem = static_cast<CollisionType>(CollisionTypeIdDef::kItem);
	//constexpr CollisionType kBoss = static_cast<CollisionType>(CollisionTypeIdDef::kBoss);
	//constexpr CollisionType kBossBullet = static_cast<CollisionType>(CollisionTypeIdDef::kBossBullet);
	constexpr CollisionType kWorld = static_cast<CollisionType>(CollisionTypeIdDef::kWorld);

	/// ---------- プレイヤーと敵の衝突判定 ---------- ///
	collisionTable_[{kEnemy, kPlayer}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	collisionTable_[{kPlayer, kEnemy}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	/// ---------- 弾と敵の衝突判定 ---------- ///
	collisionTable_[{kBullet, kEnemy}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(b->GetOBB(), a->GetSegment());
		};

	collisionTable_[{kEnemy, kBullet}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetSegment());
		};

	/// ---------- プレイヤーとアイテムの衝突判定 ---------- ///
	collisionTable_[{kPlayer, kItem}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	collisionTable_[{kItem, kPlayer}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	/// ---------- プレイヤーとワールドの衝突判定 ---------- ///
	collisionTable_[{kPlayer, kWorld}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};
	collisionTable_[{kWorld, kPlayer}] = [](Collider* a, Collider* b) {
		return CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};
}
