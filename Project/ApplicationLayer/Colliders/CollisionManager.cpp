#include "CollisionManager.h"
#include "ParameterManager.h"
#include "Collider.h"
#include <CollisionUtility.h>
#include <CollisionTypeIdDef.h>

#include <algorithm>
#include <limits>
#include <cmath>
#include <unordered_set>
#include <unordered_map>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///                         初期化処理
/// -------------------------------------------------------------
void CollisionManager::Initialize()
{
	isCollider_ = true;
	K4E::ParameterManager::GetInstance()->CreateGroup("K4E::Collider");
	K4E::ParameterManager::GetInstance()->AddItem("K4E::Collider", "isCollider", isCollider_);

	// 衝突判定関数の登録
	RegisterCollisionFuncsions();
}

/// -------------------------------------------------------------
///                         更新処理
/// -------------------------------------------------------------
void CollisionManager::Update()
{
	isCollider_ = K4E::ParameterManager::GetInstance()->GetValue<bool>("K4E::Collider", "isCollider");

	// Collider 本体の Update はデバッグ用（Wireframeなど）
	for (K4E::Collider* collider : all_) collider->Update();
}

/// -------------------------------------------------------------
///                         描画処理
/// -------------------------------------------------------------
void CollisionManager::Draw(bool drawEnemyColliders)
{
	if (!isCollider_) return;

	const uint32_t enemyType = static_cast<uint32_t>(CollisionTypeIdDef::kEnemy);
	for (K4E::Collider* collider : all_)
	{
		if (!collider || (!drawEnemyColliders && collider->GetTypeID() == enemyType)) { continue; }
		collider->Draw();
	}
}


void CollisionManager::DrawImGui()
{
#ifdef USE_IMGUI
	// Collision DebugではCollider表示フラグと登録状況を通常Dockウィンドウ内に表示する。
	bool showCollider = isCollider_;
	if (ImGui::Checkbox("Show Collider", &showCollider))
	{
		isCollider_ = showCollider;
		K4E::ParameterManager::GetInstance()->SetValue("K4E::Collider", "isCollider", isCollider_);
	}
	ImGui::Text("All Colliders: %d", static_cast<int>(all_.size()));
	if (ImGui::TreeNode("Collider Buckets"))
	{
		for (uint32_t i = 0; i < kMaxTypes; ++i)
		{
			if (!buckets_[i].empty())
			{
				ImGui::Text("Type %u: %d", i, static_cast<int>(buckets_[i].size()));
			}
		}
		ImGui::TreePop();
	}
#endif
}

/// -------------------------------------------------------------
///                         リセット処理
/// -------------------------------------------------------------
void CollisionManager::Reset()
{
	all_.clear();
	for (auto& v : buckets_) v.clear();
}

/// -------------------------------------------------------------
///                 すべての当たり判定を確認する処理
/// -------------------------------------------------------------
void CollisionManager::CheckAllCollisions()
{
	lastEnemyCollisionCount_ = 0;
	lastEnemyPlayerCollisionCount_ = 0;
	lastBulletEnemyCollisionCount_ = 0;
	lastEnemyWorldCollisionCount_ = 0;
	using CId = uint32_t;
	const CId kPlayer = static_cast<CId>(CollisionTypeIdDef::kPlayer);
	const CId kEnemy = static_cast<CId>(CollisionTypeIdDef::kEnemy);
	const CId kBoss = static_cast<CId>(CollisionTypeIdDef::kBoss);
	const CId kBullet = static_cast<CId>(CollisionTypeIdDef::kBullet);
	const CId kEnemyBullet = static_cast<CId>(CollisionTypeIdDef::kEnemyBullet);
	const CId kBossBullet = static_cast<CId>(CollisionTypeIdDef::kBossBullet);
	const CId kItem = static_cast<CId>(CollisionTypeIdDef::kItem);
	const CId kWorld = static_cast<CId>(CollisionTypeIdDef::kWorld);

	// --- スナップショット（イベント中の追加/削除に備える） ---
	std::vector<K4E::Collider*> snapshot = all_;

	// --- 1) フレーム開始：衝突状態をローテーション ---
	for (K4E::Collider* c : snapshot)
	{
		if (c) c->BeginCollisionFrame();
	}

	// --- 2) ID -> Collider のマップ（Enter/Exit 解決用） ---
	std::unordered_map<uint32_t, K4E::Collider*> idMap;
	idMap.reserve(snapshot.size());
	for (K4E::Collider* c : snapshot)
	{
		if (!c) continue;
		idMap.emplace(c->GetUniqueID(), c);
	}

	// --- 3) 判定：当たったペアは両者に「このフレーム接触中」を登録 ---
	auto pairLoop = [&](CId aId, CId bId)
		{
			auto& A = buckets_[aId];
			auto& B = buckets_[bId];
			if (A.empty() || B.empty()) return;

			const bool hasEnemy = aId == kEnemy || bId == kEnemy;
			if (hasEnemy && !enableEnemyCollision_) return;
			auto checkPair = [&](K4E::Collider* a, K4E::Collider* b)
				{
					if (!a || !b) return;
					if (hasEnemy) ++lastEnemyCollisionCount_;
					CheckCollisionPair(a, b);
				};

			if (!hasEnemy || !useSimpleStressTestCollision_)
			{
				for (K4E::Collider* a : A)
				{
					for (K4E::Collider* b : B) checkPair(a, b);
				}
				return;
			}

			constexpr float kCellSize = 8.0f;
			auto cellCoordinate = [](float value) { return static_cast<int>(std::floor(value / kCellSize)); };
			auto cellKey = [](int x, int z)
				{
					return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(z);
				};
			std::unordered_map<uint64_t, std::vector<K4E::Collider*>> spatialHash;
			spatialHash.reserve(B.size() * 2);
			for (K4E::Collider* b : B)
			{
				if (!b) continue;
				const K4E::Vector3 center = b->GetCenterPosition();
				const K4E::Vector3 half = b->GetOBBHalfSize();
				for (int x = cellCoordinate(center.x - half.x); x <= cellCoordinate(center.x + half.x); ++x)
				{
					for (int z = cellCoordinate(center.z - half.z); z <= cellCoordinate(center.z + half.z); ++z) spatialHash[cellKey(x, z)].push_back(b);
				}
			}

			// 大量敵衝突の負荷を抑えるため、Spatial Hashの同一セルにいる候補だけ判定する。
			for (K4E::Collider* a : A)
			{
				if (!a) continue;
				const K4E::Vector3 center = a->GetCenterPosition();
				const K4E::Vector3 half = a->GetOBBHalfSize();
				std::unordered_set<K4E::Collider*> checked;
				for (int x = cellCoordinate(center.x - half.x); x <= cellCoordinate(center.x + half.x); ++x)
				{
					for (int z = cellCoordinate(center.z - half.z); z <= cellCoordinate(center.z + half.z); ++z)
					{
						auto it = spatialHash.find(cellKey(x, z));
						if (it == spatialHash.end()) continue;
						for (K4E::Collider* b : it->second) if (checked.insert(b).second) checkPair(a, b);
					}
				}
			}
		};

	// ここは片方向だけ回す（CheckCollisionPair 内で両者に登録するため）
	pairLoop(kBoss, kPlayer);
	const int beforeEnemyPlayer = lastEnemyCollisionCount_;
	pairLoop(kEnemy, kPlayer);
	lastEnemyPlayerCollisionCount_ = lastEnemyCollisionCount_ - beforeEnemyPlayer;
	const int beforeBulletEnemy = lastEnemyCollisionCount_;
	pairLoop(kBullet, kEnemy);
	lastBulletEnemyCollisionCount_ = lastEnemyCollisionCount_ - beforeBulletEnemy;
	pairLoop(kBoss, kBullet);
	pairLoop(kEnemyBullet, kPlayer);
	pairLoop(kPlayer, kBossBullet);
	pairLoop(kPlayer, kItem);
	pairLoop(kPlayer, kWorld);
	const int beforeEnemyWorld = lastEnemyCollisionCount_;
	pairLoop(kEnemy, kWorld);
	lastEnemyWorldCollisionCount_ = lastEnemyCollisionCount_ - beforeEnemyWorld;
	pairLoop(kBoss, kWorld);
	// Bullet vs World（壁に当てて消す）
	pairLoop(kBullet, kWorld);
	pairLoop(kEnemyBullet, kWorld);
	pairLoop(kBossBullet, kWorld);

	// --- 4) Enter/Stay/Exit を解決して通知 ---
	for (K4E::Collider* self : snapshot)
	{
		if (!self) continue;

		const auto& cur = self->GetCurrentCollisions();
		const auto& prev = self->GetPrevCollisions();

		// Enter / Stay
		for (uint32_t otherId : cur)
		{
			auto it = idMap.find(otherId);
			if (it == idMap.end()) continue;
			K4E::Collider* other = it->second;
			if (!other || other == self) continue;

			if (prev.find(otherId) == prev.end())
			{
				self->OnCollisionEnter(other);
			}
			else
			{
				self->OnCollisionStay(other);
			}
		}

		// Exit
		for (uint32_t otherId : prev)
		{
			if (cur.find(otherId) != cur.end()) continue;

			auto it = idMap.find(otherId);
			if (it == idMap.end()) continue;
			K4E::Collider* other = it->second;
			if (!other || other == self) continue;

			self->OnCollisionExit(other);
		}
	}
}

/// -------------------------------------------------------------
///                     コライダーを追加
/// -------------------------------------------------------------
void CollisionManager::AddCollider(K4E::Collider* other)
{
	if (!other) return;
	if (std::find(all_.begin(), all_.end(), other) != all_.end()) return;

	// 同一Colliderの二重登録を防ぎ、弾発射後にCollision負荷が増え続けないようにする。
	all_.push_back(other);
	const uint32_t id = other->GetTypeID();
	if (id < kMaxTypes) buckets_[id].push_back(other);
}

/// -------------------------------------------------------------
///                     コライダーを削除
/// -------------------------------------------------------------
void CollisionManager::RemoveCollider(K4E::Collider* other)
{
	all_.erase(std::remove(all_.begin(), all_.end(), other), all_.end());

	const uint32_t id = other->GetTypeID();
	if (id < kMaxTypes)
	{
		auto& v = buckets_[id];
		v.erase(std::remove(v.begin(), v.end(), other), v.end());
	}
}

bool CollisionManager::SegmentCast(uint32_t targetType, const K4E::Segment& seg, K4E::Collider** outHit) const
{
	if (outHit) *outHit = nullptr;
	if (targetType >= kMaxTypes) return false;

	float best = std::numeric_limits<float>::max();
	K4E::Collider* bestCol = nullptr;

	for (K4E::Collider* c : buckets_[targetType])
	{
		if (!c) continue;

		// Segment vs OBB 判定（既存の仕組みを利用）
		if (!K4E::CollisionUtility::IsCollision(c->GetOBB(), seg)) continue;

		// “近いものを優先” の簡易（最短でそれっぽく）
		const K4E::Vector3 d = c->GetCenterPosition() - seg.origin;
		const float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
		if (distSq < best)
		{
			best = distSq;
			bestCol = c;
		}
	}

	if (outHit) *outHit = bestCol;
	return bestCol != nullptr;
}

/// -------------------------------------------------------------
///                 コライダー２つの衝突判定と接触登録
/// -------------------------------------------------------------
void CollisionManager::CheckCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB)
{
	// 自分同士は無視
	if (colliderA == colliderB) return;

	// 衝突判定関数を取得
	auto key = std::make_pair(colliderA->GetTypeID(), colliderB->GetTypeID());

	// 衝突判定関数を検索
	auto it = collisionTable_.find(key);

	// 登録されていない型は無視
	if (it == collisionTable_.end()) return;

	// 衝突していなければ無視
	if (!it->second(colliderA, colliderB)) return;

	// 衝突していたので「このフレーム接触中」を両者へ登録
	colliderA->AddCollisionThisFrame(colliderB->GetUniqueID());
	colliderB->AddCollisionThisFrame(colliderA->GetUniqueID());
}

/// -------------------------------------------------------------
///                 コライダーの衝突判定関数の登録
/// -------------------------------------------------------------
void CollisionManager::RegisterCollisionFuncsions()
{
	using CollisionType = uint32_t;
	constexpr CollisionType kPlayer = static_cast<CollisionType>(CollisionTypeIdDef::kPlayer);
	constexpr CollisionType kEnemy = static_cast<CollisionType>(CollisionTypeIdDef::kEnemy);
	constexpr CollisionType kBullet = static_cast<CollisionType>(CollisionTypeIdDef::kBullet);
	constexpr CollisionType kEnemyBullet = static_cast<CollisionType>(CollisionTypeIdDef::kEnemyBullet);
	constexpr CollisionType kBossBullet = static_cast<CollisionType>(CollisionTypeIdDef::kBossBullet);
	constexpr CollisionType kItem = static_cast<CollisionType>(CollisionTypeIdDef::kItem);
	constexpr CollisionType kBoss = static_cast<CollisionType>(CollisionTypeIdDef::kBoss);
	constexpr CollisionType kWorld = static_cast<CollisionType>(CollisionTypeIdDef::kWorld);

	auto AddCollisionFunc = [&](CollisionType a, CollisionType b, const CollisionFunc& func) {
		collisionTable_[{a, b}] = func;
		};

	auto AddSymmetricCollisionFunc = [&](CollisionType a, CollisionType b, const CollisionFunc& func) {
		AddCollisionFunc(a, b, func);
		AddCollisionFunc(b, a, func);
		};

	// OBB vs OBB
	const CollisionFunc OBB_OBB = [](K4E::Collider* a, K4E::Collider* b) {
		return K4E::CollisionUtility::IsCollision(a->GetOBB(), b->GetOBB());
		};

	// Segment vs OBB（弾など）
	const CollisionFunc SEG_OBB = [](K4E::Collider* segOwner, K4E::Collider* obbOwner) {
		return K4E::CollisionUtility::IsCollision(obbOwner->GetOBB(), segOwner->GetSegment());
		};

	// OBB vs OBB（左右対称）
	for (auto [a, b] : std::initializer_list<std::pair<CollisionType, CollisionType>>{
		{kPlayer, kEnemy},
		{kPlayer, kBoss},
		{kPlayer, kItem},
		{kPlayer, kWorld},
		})
	{
		AddSymmetricCollisionFunc(a, b, OBB_OBB);
	}

	// Segment vs OBB（左右対称）
	//  - 弾は Segment、キャラ/ワールドは OBB として扱う
	for (auto [seg, obb] : std::initializer_list<std::pair<CollisionType, CollisionType>>{
		{kBullet, kEnemy},
		{kBullet, kBoss},
		{kBullet, kWorld},
		{kEnemyBullet, kPlayer},
		{kEnemyBullet, kWorld},
		{kBossBullet, kPlayer},
		{kBossBullet, kWorld},
		})
	{
		AddCollisionFunc(seg, obb, SEG_OBB);
		AddCollisionFunc(obb, seg, [=](K4E::Collider* obbOwner, K4E::Collider* segOwner) {
			return SEG_OBB(segOwner, obbOwner);
			});
	}
}
