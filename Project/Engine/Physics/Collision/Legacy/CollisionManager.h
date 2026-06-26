#pragma once
#include "CollisionBroadPhase.h"
#include "CollisionHitResult.h"
#include "CollisionPresetLibrary.h"
#include "ObjectCollisionResponseMatrix.h"
#include "TraceResponseMatrix.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	class Collider;
	struct Segment;
}

namespace K4E = ::Ken4lowEngine;

// BroadPhaseの試験切り替えモード。デフォルトは既存挙動互換のBruteForce。
enum class ECollisionBroadPhaseMode : uint8_t
{
	BruteForce,
	UniformGrid,
};

// Debug比較用にBroadPhase候補ペアの差分だけを保持する。
struct CollisionBroadPhaseDebugComparison
{
	size_t bruteForcePairCount = 0;
	size_t uniformGridPairCount = 0;
	size_t uniformGridMissingPairCount = 0;
	size_t uniformGridDuplicatePairCount = 0;
	bool uniformGridHasMissingPairs = false;
};

// Colliderペアを順序に依存せず識別するキー。A-B/B-Aを同じ接触として扱う。
struct CollisionEventPairKey
{
	uint32_t lowId = 0;
	uint32_t highId = 0;

	bool operator==(const CollisionEventPairKey& other) const
	{
		return lowId == other.lowId && highId == other.highId;
	}
};

struct CollisionEventPairKeyHash
{
	size_t operator()(const CollisionEventPairKey& key) const
	{
		return (static_cast<size_t>(key.lowId) << 32) ^ static_cast<size_t>(key.highId);
	}
};

// 1フレーム内で成立した接触ペアと、その最終Responseを保持するイベント配送用データ。
struct CollisionEventContact
{
	CollisionEventPairKey key{};
	K4E::Collider* colliderA = nullptr;
	K4E::Collider* colliderB = nullptr;
	ECollisionResponse response = ECollisionResponse::Ignore;
};

/// -------------------------------------------------------------
///                     当たり判定管理クラス
/// -------------------------------------------------------------
class CollisionManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// Collision Debugパネル用のImGui表示処理
	void DrawImGui();

	// リセット処理
	void Reset();

	// すべての当たり判定を確認する処理
	void CheckAllCollisions();

	// コライダーを追加
	void AddCollider(K4E::Collider* other);

	// コライダーを削除
	void RemoveCollider(K4E::Collider* other);

	// 衝突判定
	using CollisionFunc = std::function<bool(K4E::Collider*, K4E::Collider*)>;

	// セグメントキャスト
	bool SegmentCast(uint32_t targetType, const K4E::Segment& seg, K4E::Collider** outHit = nullptr) const;

	// SegmentCastのHitResult版入口。現段階ではclosest hit 1件だけを返す。
	bool SegmentCastHit(uint32_t targetType, const K4E::Segment& seg, CollisionHitResult& outHit) const;

	// TraceChannel指定のSegmentCast入口。用途別に対象ObjectChannelを選ぶ準備として使う。
	bool SegmentCastByTraceChannel(ETraceChannel traceChannel, const K4E::Segment& seg, CollisionHitResult& outHit) const;

	// RaycastSingleはTraceChannelに従って最も近い1件だけを返す。イベント通知は発生させない。
	bool RaycastSingle(const RaycastQuery& query, RaycastHit& outHit) const;

	// RaycastAllはTraceChannelに反応する全Hitを距離順で返す。クロスヘア/ロックオン候補列挙に使える。
	std::vector<RaycastHit> RaycastAll(const RaycastQuery& query) const;

	// 指定タイプのCollider一覧を返す。
	// 爆風ダメージなど、通常の接触ペア以外で近傍検索したい時に使う。
	const std::vector<K4E::Collider*>& GetCollidersByType(uint32_t typeId) const
	{
		static const std::vector<K4E::Collider*> empty{};
		if (typeId >= kMaxTypes) return empty;
		return buckets_[typeId];
	}

	// Detailsの簡易表示用に現在登録中のCollider総数だけを公開する。
	size_t GetColliderCount() const { return all_.size(); }

	// Debug表示用にタイプ別Collider数を公開する。
	size_t GetColliderCountByType(uint32_t typeId) const
	{
		if (typeId >= kMaxTypes) return 0;
		return buckets_[typeId].size();
	}

	// CheckAllCollisionsや移行作業からTypeID同士のResponseを確認する入口。
	ECollisionResponse GetCollisionResponse(uint32_t selfTypeId, uint32_t otherTypeId) const
	{
		return responseMatrix_.GetResponse(selfTypeId, otherTypeId);
	}

	// 将来のObject Channel指定用入口。現段階では既存TypeIDと同じ数値へ対応させる。
	ECollisionResponse GetCollisionResponse(EObjectChannel self, EObjectChannel other) const
	{
		return responseMatrix_.GetResponse(self, other);
	}

	// ResponseMatrixの内容確認・移行作業用に読み取り専用で公開する。
	const ObjectCollisionResponseMatrix& GetResponseMatrix() const { return responseMatrix_; }

	// TraceChannelとObjectChannelの問い合わせ反応を確認する入口。
	ECollisionResponse GetTraceResponse(ETraceChannel traceChannel, uint32_t objectTypeId) const
	{
		return traceResponseMatrix_.GetResponse(traceChannel, objectTypeId);
	}

	// TraceResponseMatrixの内容確認・移行作業用に読み取り専用で公開する。
	const TraceResponseMatrix& GetTraceResponseMatrix() const { return traceResponseMatrix_; }

	// BroadPhase試験実装の切り替え入口。既存CheckAllCollisionsへの反映はまだ行わない。
	void SetBroadPhaseMode(ECollisionBroadPhaseMode mode) { broadPhaseMode_ = mode; }
	ECollisionBroadPhaseMode GetBroadPhaseMode() const { return broadPhaseMode_; }

	// Debug確認用に、現在選択中のBroadPhaseで候補ペア数だけを収集する。
	size_t CollectPotentialPairsForDebug(std::vector<CollisionPair>& outPairs) const;

	// Debug確認用に、BruteForceとUniformGridの候補漏れ・重複を比較する。
	CollisionBroadPhaseDebugComparison CompareBroadPhasesForDebug() const;

	// CollisionPresetの読み取り専用確認入口。Json失敗時もコード既定値を返す。
	const CollisionPresetLibrary& GetCollisionPresetLibrary() const { return presetLibrary_; }

private: /// ---------- メンバ関数 ---------- ///

	// ResponseMatrixから、このTypeIDペアの現在の衝突反応を取得する。
	ECollisionResponse GetCollisionResponseForPair(uint32_t selfTypeId, uint32_t otherTypeId) const;

	// Collider個別Responseがあればそれを使い、未設定なら既存ResponseMatrixへフォールバックする。
	ECollisionResponse GetCollisionResponseForCollider(K4E::Collider* self, K4E::Collider* other) const;

	// 双方向Responseを合成し、Ignore/Overlap/Blockの最終挙動を決める。
	ECollisionResponse ResolveCollisionResponseForPair(K4E::Collider* colliderA, K4E::Collider* colliderB) const;

	// Ignoreだけを既存の「判定しない」ペアとして扱う入口にする。
	bool ShouldSkipCollisionPair(ECollisionResponse response) const;

	// ResponseMatrixのIgnore判定入口。現段階では既存ペア列挙の安全なスキップ確認にだけ使う。
	bool IsCollisionIgnored(uint32_t selfTypeId, uint32_t otherTypeId) const;

	// Collider個別Responseも含めたIgnore判定入口。
	bool IsCollisionIgnored(K4E::Collider* colliderA, K4E::Collider* colliderB) const;

	// Colliderの有効状態とOwner状態を見て、判定/Trace候補に入れてよいかを判断する。
	bool IsColliderProcessable(K4E::Collider* collider) const;

	// Debug Viewerが描画/表示してよいEditor Mode中かを判定する。
	bool ShouldShowCollisionDebugViewer() const;

	// Debug描画色をCollider状態と現在接触Responseから決める。
	K4E::Vector4 GetColliderDebugDrawColor(K4E::Collider* collider) const;

	// 現在フレームで接触中なら、その最終Responseを返す。
	bool TryGetCurrentContactResponse(K4E::Collider* collider, ECollisionResponse& outResponse) const;

	// 判定対象から外れている理由をDebug表示用に返す。
	const char* GetColliderSkipReason(K4E::Collider* collider) const;

	// RaycastQueryを正規化し、方向長や距離の不正値を安全な問い合わせへ丸める。
	RaycastQuery NormalizeRaycastQuery(const RaycastQuery& query) const;

	// TraceChannel/Collider状態/形状を見てRaycast候補1件を作る。
	bool RaycastCollider(const RaycastQuery& query, K4E::Collider* collider, RaycastHit& outHit) const;

	// 形状別Raycast。対応外形状は派生AABBへフォールバックする。
	bool RaycastShape(const RaycastQuery& query, K4E::Collider* collider, float& outDistance, K4E::Vector3& outNormal) const;
	bool RaycastAABB(const RaycastQuery& query, const K4E::AABB& aabb, float& outDistance, K4E::Vector3& outNormal) const;
	bool RaycastSphere(const RaycastQuery& query, const K4E::Sphere& sphere, float& outDistance, K4E::Vector3& outNormal) const;
	bool RaycastOBB(const RaycastQuery& query, const K4E::OBB& obb, float& outDistance, K4E::Vector3& outNormal) const;

	// Editor Mode中だけRaycast Debug Lineを保持する。
	void RecordRaycastDebugLine(const RaycastQuery& query, const std::vector<RaycastHit>& hits) const;

	// 登録済みの形状判定関数で、このColliderペアが実際に交差しているかだけを調べる。
	bool TestCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB) const;

	// 衝突が成立したペアを、このフレームの接触状態として両Colliderへ登録する。
	void UpdateContactState(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response);

	// Colliderペアを順序に依存しないキーへ正規化する。
	CollisionEventPairKey MakeCollisionEventPairKey(K4E::Collider* colliderA, K4E::Collider* colliderB) const;

	// CollisionHitを自己視点で組み立て、Owner側が相手とResponseを読めるようにする。
	K4E::CollisionHit BuildCollisionHit(K4E::Collider* self, K4E::Collider* other, ECollisionResponse response) const;

	// 現在/前回の接触ペアから、Enter/Stay/ExitとBlock/Overlapを切り分けて配送する。
	void DispatchCollisionEvents();

	// 1ペアぶんのEnter/Stayを両Colliderへ通知する。
	void DispatchActiveCollisionEvent(const CollisionEventContact& contact, bool wasTouching);

	// 1ペアぶんのExitを両Colliderへ通知する。
	void DispatchExitCollisionEvent(const CollisionEventContact& contact);

	// Response種別ごとの入口を分け、将来Block/Overlapの処理差分をここから広げる。
	void ProcessCollisionPairByResponse(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response);

	// 既存CheckAllCollisionsと同じTypeIDペア列挙を、Broad Phase互換入口へ渡せる形にする。
	std::vector<CollisionBroadPhaseTypePair> BuildLegacyBroadPhaseTypePairs() const;

	// BruteForceBroadPhaseで既存総当たりと同じ候補ペアを収集する準備入口。
	void CollectPotentialPairsWithBruteForceBroadPhase(std::vector<CollisionPair>& outPairs) const;

	// UniformGridBroadPhaseで近傍候補ペアを収集する試験入口。
	void CollectPotentialPairsWithUniformGridBroadPhase(std::vector<CollisionPair>& outPairs) const;

	// Blockは将来OnCollisionEnter/Stay/Exit専用に寄せるが、現段階では既存接触登録へ流す。
	void ProcessBlockCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB);

	// Overlapは将来OnOverlapEnter/Stay/Exitへ分けるが、現段階では既存接触登録へ流す。
	void ProcessOverlapCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB);

	// コライダー2つの衝突判定（衝突したら両者へ接触を登録）
	void CheckCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response);

	// 衝突判定関数の登録
	void RegisterCollisionFuncsions();

	// ImGuiから明示ApplyされたColliderだけ、Bucket登録を保ったままPresetを反映する。
	bool ApplyCollisionPresetToRegisteredCollider(K4E::Collider* collider, const CollisionPreset& preset);

private: /// ---------- メンバ変数 ---------- ///

	// コライダーの最大タイプ数
	static const uint32_t kMaxTypes = 32;

	// Phase 13設計メモ: Broad Phaseは候補ペア収集、Narrow PhaseはTestCollisionPair以降の形状判定とイベント登録を担当する。
	// 候補名: CollisionBroadPhase(抽象入口), UniformGridBroadPhase(初期実装候補), CollisionPairCollector(既存pairLoop互換の収集結果)。
	// 最初はUniform Gridで動的Collider/弾/敵の近傍ペアだけを集め、CheckAllCollisionsのTypeID列挙とResponseMatrix判定は維持する方針。

	// 型ごとのバケット
	ICollisionBroadPhase::ColliderBuckets buckets_{};

	// 全コライダー（デバッグ描画・イベント解決に使用）
	std::vector<K4E::Collider*> all_;

	// 衝突判定関数の登録
	std::map<std::pair<uint32_t, uint32_t>, CollisionFunc> collisionTable_;

	// UE風ResponseMatrixを保持し、現段階ではIgnore判定だけに使用する。
	ObjectCollisionResponseMatrix responseMatrix_{};

	// UE風TraceChannel設定を保持し、現段階では新規Trace入口だけで使用する。
	TraceResponseMatrix traceResponseMatrix_{};

	// Json対応Presetを保持し、読み込み失敗時はコード既定Presetへフォールバックする。
	CollisionPresetLibrary presetLibrary_{};

	// 既存総当たりと同じ候補を返す互換Broad Phase。現段階では本番判定へはまだ反映しない。
	BruteForceBroadPhase bruteForceBroadPhase_{};

	// 近傍セルだけから候補を返す試験Broad Phase。デフォルトでは使わない。
	UniformGridBroadPhase uniformGridBroadPhase_{};

	// BroadPhase切り替え状態。既存挙動維持のため初期値はBruteForce。
	ECollisionBroadPhaseMode broadPhaseMode_ = ECollisionBroadPhaseMode::BruteForce;

	// Debug表示用に、選択中BroadPhaseで最後に収集した候補ペア数を保持する。
	mutable size_t lastBroadPhaseCandidatePairCount_ = 0;

	// Debug表示用に、BruteForceとUniformGridの候補比較結果を保持する。
	mutable CollisionBroadPhaseDebugComparison lastBroadPhaseComparison_{};

	// Debug表示用に、最後のSegmentCast/TraceChannel Cast結果を保持する。
	mutable CollisionHitResult lastTraceHitResult_{};
	mutable ETraceChannel lastTraceChannel_ = ETraceChannel::Visibility;
	mutable bool lastTraceUsedTraceChannel_ = false;
	mutable bool hasLastTraceHitResult_ = false;

	// Debug表示用に、ResponseMatrixでIgnoreスキップされたペアループ数を保持する。
	uint32_t ignoredPairLoopCount_ = 0;

	using CollisionEventContactMap = std::unordered_map<CollisionEventPairKey, CollisionEventContact, CollisionEventPairKeyHash>;

	// 接触イベント用の前フレーム/現在フレームペア集合。差分比較でEnter/Stay/Exitを確定する。
	CollisionEventContactMap previousContacts_{};
	CollisionEventContactMap currentContacts_{};

	// Debug表示用に最後のイベント配送数を保持する。Editor Mode以外では表示しない。
	uint32_t lastCollisionEnterEventCount_ = 0;
	uint32_t lastCollisionStayEventCount_ = 0;
	uint32_t lastCollisionExitEventCount_ = 0;
	uint32_t lastOverlapBeginEventCount_ = 0;
	uint32_t lastOverlapStayEventCount_ = 0;
	uint32_t lastOverlapEndEventCount_ = 0;

	struct RaycastDebugLine
	{
		K4E::Vector3 start{};
		K4E::Vector3 end{};
		K4E::Vector4 color{};
		bool hit = false;
	};

	// Debug表示用に直近Raycastの線分を保存する。Release/Game Previewでは描画しない。
	mutable std::vector<RaycastDebugLine> raycastDebugLines_{};
	mutable RaycastQuery lastRaycastQuery_{};
	mutable std::vector<RaycastHit> lastRaycastHits_{};
	mutable bool hasLastRaycastQuery_ = false;

	// コライダーの可視化フラグ
	bool isCollider_ = true;
};
