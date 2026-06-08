#include "CollisionManager.h"
#include "ParameterManager.h"
#include "Collider.h"
#include <CollisionUtility.h>
#include <CollisionTypeIdDef.h>

#include <algorithm>
#include <cmath>
#include <limits>
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
#ifdef _DEBUG
	isCollider_ = true;
#else
	isCollider_ = false;
#endif
	K4E::ParameterManager::GetInstance()->CreateGroup("K4E::Collider");
	K4E::ParameterManager::GetInstance()->AddItem("K4E::Collider", "isCollider", isCollider_);
	K4E::ParameterManager::GetInstance()->SetDisplayName("K4E::Collider", "isCollider", "コライダー表示");
	K4E::ParameterManager::GetInstance()->RegisterParameterApplier("K4E::Collider", this, [this]() {
#ifdef _DEBUG
		isCollider_ = K4E::ParameterManager::GetInstance()->GetValue<bool>("K4E::Collider", "isCollider");
#else
		isCollider_ = false;
#endif
	}); // 反映ボタンでCollider表示フラグをParameterManagerから再取得する。

	// 既存ペア表と同じ関係をResponseMatrixへ写し、Ignore/Block/Overlapの入口に使う。
	responseMatrix_.InitializeLegacyDefaults();

	// TraceChannelごとの問い合わせ対象を初期化し、既存SegmentCastとは別入口で使う。
	traceResponseMatrix_.InitializeLegacyDefaults();

	// 衝突判定関数の登録
	RegisterCollisionFuncsions();
}

/// -------------------------------------------------------------
///                         更新処理
/// -------------------------------------------------------------
void CollisionManager::Update()
{
#ifdef _DEBUG
	isCollider_ = K4E::ParameterManager::GetInstance()->GetValue<bool>("K4E::Collider", "isCollider");
#else
	// Releaseビルドでは保存済みパラメータがtrueでもColliderワイヤーを復活させない。
	isCollider_ = false;
#endif

	// Collider 本体の Update はデバッグ用（Wireframeなど）
	for (K4E::Collider* collider : all_) collider->Update();
}

/// -------------------------------------------------------------
///                         描画処理
/// -------------------------------------------------------------
void CollisionManager::Draw()
{
#ifndef _DEBUG
	return;
#endif
	if (!isCollider_) return;

	for (K4E::Collider* collider : all_)
		collider->Draw();
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
	ImGui::Text("Ignored Pair Loops: %u", ignoredPairLoopCount_);
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
	using CId = uint32_t;
	const CId kPlayer = static_cast<CId>(CollisionTypeIdDef::kPlayer);
	const CId kEnemy = static_cast<CId>(CollisionTypeIdDef::kEnemy);
	const CId kBoss = static_cast<CId>(CollisionTypeIdDef::kBoss);
	const CId kBullet = static_cast<CId>(CollisionTypeIdDef::kBullet);
	const CId kEnemyBullet = static_cast<CId>(CollisionTypeIdDef::kEnemyBullet);
	const CId kBossBullet = static_cast<CId>(CollisionTypeIdDef::kBossBullet);
	const CId kItem = static_cast<CId>(CollisionTypeIdDef::kItem);
	const CId kWorld = static_cast<CId>(CollisionTypeIdDef::kWorld);
	const CId kCrystal = static_cast<CId>(CollisionTypeIdDef::kCrystal);
	ignoredPairLoopCount_ = 0;

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
			const ECollisionResponse response = GetCollisionResponseForPair(aId, bId);

			// ResponseMatrixでIgnoreのペアだけを、既存と同じ「判定しない」扱いで早期終了する。
			if (ShouldSkipCollisionPair(response))
			{
				++ignoredPairLoopCount_;
				return;
			}

			auto& A = buckets_[aId];
			auto& B = buckets_[bId];
			if (A.empty() || B.empty()) return;

			for (K4E::Collider* a : A)
			{
				if (!a) continue;
				for (K4E::Collider* b : B)
				{
					if (!b) continue;
					ProcessCollisionPairByResponse(a, b, response);
				}
			}
		};

	// ここは片方向だけ回す（CheckCollisionPair 内で両者に登録するため）
	pairLoop(kBoss, kPlayer);
	pairLoop(kEnemy, kPlayer);
	pairLoop(kBullet, kEnemy);
	pairLoop(kBullet, kCrystal);
	pairLoop(kBoss, kBullet);
	pairLoop(kEnemyBullet, kPlayer);
	pairLoop(kPlayer, kBossBullet);
	pairLoop(kPlayer, kItem);
	pairLoop(kPlayer, kWorld);
	pairLoop(kEnemy, kWorld);
	pairLoop(kBoss, kWorld);
	// Bullet vs World（壁に当てて消す）
	pairLoop(kBullet, kWorld);
	pairLoop(kEnemyBullet, kWorld);
	pairLoop(kBossBullet, kWorld);

	// --- 4) Enter/Stay/Exit を解決して通知 ---
	DispatchCollisionEvents(snapshot, idMap);
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
	CollisionHitResult hitResult{};
	const bool hit = SegmentCastHit(targetType, seg, hitResult);
	if (outHit) *outHit = hitResult.collider;
	return hit;
}

bool CollisionManager::SegmentCastHit(uint32_t targetType, const K4E::Segment& seg, CollisionHitResult& outHit) const
{
	// HitResult版でも既存SegmentCastと同じclosest判定基準を使い、互換挙動を維持する。
	outHit = CollisionHitResult{};
	if (targetType >= kMaxTypes) return false;

	float best = std::numeric_limits<float>::max();
	K4E::Collider* bestCol = nullptr;
	K4E::Vector3 bestCenter{};

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
			bestCenter = c->GetCenterPosition();
		}
	}

	if (!bestCol) return false;

	outHit.hit = true;
	outHit.collider = bestCol;
	outHit.typeId = bestCol->GetTypeID();
	outHit.objectChannel = ToObjectChannel(outHit.typeId);
	outHit.response = ECollisionResponse::Block; // TraceChannel未導入のため、既存SegmentCast命中は遮蔽/命中扱いに寄せる。
	outHit.distance = std::sqrt(best);
	outHit.point = bestCenter; // TODO: OBB vs Segmentが交点を返せるようになったら正確な衝突点へ置き換える。
	outHit.normal = {}; // TODO: 形状問い合わせが法線を返せる段階で設定する。
	return true;
}

bool CollisionManager::SegmentCastByTraceChannel(ETraceChannel traceChannel, const K4E::Segment& seg, CollisionHitResult& outHit) const
{
	// TraceChannel版は用途別Matrixで対象ObjectChannelを選び、closest hit 1件だけを返す。
	outHit = CollisionHitResult{};

	float best = std::numeric_limits<float>::max();
	K4E::Collider* bestCol = nullptr;
	K4E::Vector3 bestCenter{};
	ECollisionResponse bestResponse = ECollisionResponse::Ignore;

	for (uint32_t typeId = 0; typeId < kMaxTypes; ++typeId)
	{
		const ECollisionResponse response = traceResponseMatrix_.GetResponse(traceChannel, typeId);
		if (response == ECollisionResponse::Ignore) continue;

		for (K4E::Collider* c : buckets_[typeId])
		{
			if (!c) continue;

			// Segment vs OBB 判定のみを使い、既存SegmentCastと同じ形状問い合わせに揃える。
			if (!K4E::CollisionUtility::IsCollision(c->GetOBB(), seg)) continue;

			const K4E::Vector3 d = c->GetCenterPosition() - seg.origin;
			const float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
			if (distSq < best)
			{
				best = distSq;
				bestCol = c;
				bestCenter = c->GetCenterPosition();
				bestResponse = response;
			}
		}
	}

	if (!bestCol) return false;

	outHit.hit = true;
	outHit.collider = bestCol;
	outHit.typeId = bestCol->GetTypeID();
	outHit.objectChannel = ToObjectChannel(outHit.typeId);
	outHit.response = bestResponse;
	outHit.distance = std::sqrt(best);
	outHit.point = bestCenter; // TODO: SegmentCastHitと同じく、交点取得対応後に正確な衝突点へ置き換える。
	outHit.normal = {}; // TODO: OBB vs Segmentが法線を返せる段階で設定する。
	return true;
}

ECollisionResponse CollisionManager::GetCollisionResponseForPair(uint32_t selfTypeId, uint32_t otherTypeId) const
{
	// TypeID同士の参照にまとめ、既存TypeIDからObjectChannelへの移行点を一箇所に寄せる。
	return responseMatrix_.GetResponse(selfTypeId, otherTypeId);
}

bool CollisionManager::ShouldSkipCollisionPair(ECollisionResponse response) const
{
	// 現段階で挙動に反映するのはIgnoreだけに限定する。
	return response == ECollisionResponse::Ignore;
}

bool CollisionManager::IsCollisionIgnored(uint32_t selfTypeId, uint32_t otherTypeId) const
{
	// Block/Overlapの意味はまだ使わず、Ignoreだけを安全なスキップ条件にする。
	return ShouldSkipCollisionPair(GetCollisionResponseForPair(selfTypeId, otherTypeId));
}

bool CollisionManager::TestCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB) const
{
	// 形状判定だけを担当し、接触状態やイベント通知はここでは更新しない。
	const auto key = std::make_pair(colliderA->GetTypeID(), colliderB->GetTypeID());
	const auto it = collisionTable_.find(key);
	if (it == collisionTable_.end()) return false;
	return it->second(colliderA, colliderB);
}

void CollisionManager::UpdateContactState(K4E::Collider* colliderA, K4E::Collider* colliderB)
{
	// 既存イベント互換のため、衝突成立時は両者へ同じタイミングで接触を登録する。
	colliderA->AddCollisionThisFrame(colliderB->GetUniqueID());
	colliderB->AddCollisionThisFrame(colliderA->GetUniqueID());
}

void CollisionManager::DispatchCollisionEvents(const std::vector<K4E::Collider*>& snapshot, const std::unordered_map<uint32_t, K4E::Collider*>& idMap)
{
	// 現在はBlock/Overlap兼用の互換イベントとして、既存のOnCollisionEnter/Stay/Exitだけを配送する。
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

void CollisionManager::ProcessCollisionPairByResponse(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response)
{
	// Block/Overlapの入口だけ分け、現段階では既存イベント互換の処理へ流す。
	switch (response)
	{
	case ECollisionResponse::Block:
		ProcessBlockCollisionPair(colliderA, colliderB);
		break;
	case ECollisionResponse::Overlap:
		ProcessOverlapCollisionPair(colliderA, colliderB);
		break;
	case ECollisionResponse::Ignore:
	default:
		break;
	}
}

void CollisionManager::ProcessBlockCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB)
{
	// 将来はBlock接触としてOnCollisionEnter/Stay/Exitへ寄せるが、押し戻し連携はまだ行わない。
	CheckCollisionPair(colliderA, colliderB);
}

void CollisionManager::ProcessOverlapCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB)
{
	// 将来はOnOverlapEnter/Stay/Exitを呼ぶ予定地だが、現段階では既存接触イベントを維持する。
	CheckCollisionPair(colliderA, colliderB);
}

/// -------------------------------------------------------------
///                 コライダー２つの衝突判定と接触登録
/// -------------------------------------------------------------
void CollisionManager::CheckCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB)
{
	// 自分同士は無視
	if (colliderA == colliderB) return;

	// ResponseMatrixでIgnoreの組み合わせは、既存の未登録ペアと同じく判定しない。
	if (IsCollisionIgnored(colliderA->GetTypeID(), colliderB->GetTypeID())) return;

	// 登録済み形状判定で交差したペアだけを、このフレームの接触として扱う。
	if (!TestCollisionPair(colliderA, colliderB)) return;

	// 衝突していたので「このフレーム接触中」を両者へ登録
	UpdateContactState(colliderA, colliderB);
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
	constexpr CollisionType kCrystal = static_cast<CollisionType>(CollisionTypeIdDef::kCrystal);

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
		{kBullet, kCrystal},
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
