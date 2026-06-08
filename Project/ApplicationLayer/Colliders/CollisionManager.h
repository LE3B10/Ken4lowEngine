#pragma once
#include "CollisionHitResult.h"
#include "CollisionResponseMatrix.h"
#include "TraceResponseMatrix.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	class Collider;
	struct Segment;
}

namespace K4E = ::Ken4lowEngine;

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
	const CollisionResponseMatrix& GetResponseMatrix() const { return responseMatrix_; }

	// TraceChannelとObjectChannelの問い合わせ反応を確認する入口。
	ECollisionResponse GetTraceResponse(ETraceChannel traceChannel, uint32_t objectTypeId) const
	{
		return traceResponseMatrix_.GetResponse(traceChannel, objectTypeId);
	}

	// TraceResponseMatrixの内容確認・移行作業用に読み取り専用で公開する。
	const TraceResponseMatrix& GetTraceResponseMatrix() const { return traceResponseMatrix_; }

private: /// ---------- メンバ関数 ---------- ///

	// ResponseMatrixから、このTypeIDペアの現在の衝突反応を取得する。
	ECollisionResponse GetCollisionResponseForPair(uint32_t selfTypeId, uint32_t otherTypeId) const;

	// Ignoreだけを既存の「判定しない」ペアとして扱う入口にする。
	bool ShouldSkipCollisionPair(ECollisionResponse response) const;

	// ResponseMatrixのIgnore判定入口。現段階では既存ペア列挙の安全なスキップ確認にだけ使う。
	bool IsCollisionIgnored(uint32_t selfTypeId, uint32_t otherTypeId) const;

	// 登録済みの形状判定関数で、このColliderペアが実際に交差しているかだけを調べる。
	bool TestCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB) const;

	// 衝突が成立したペアを、このフレームの接触状態として両Colliderへ登録する。
	void UpdateContactState(K4E::Collider* colliderA, K4E::Collider* colliderB);

	// 現在/前回の接触状態から、既存互換のOnCollisionEnter/Stay/Exitを同じ順序で配送する。
	void DispatchCollisionEvents(const std::vector<K4E::Collider*>& snapshot, const std::unordered_map<uint32_t, K4E::Collider*>& idMap);

	// Response種別ごとの入口を分け、将来Block/Overlapの処理差分をここから広げる。
	void ProcessCollisionPairByResponse(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response);

	// Blockは将来OnCollisionEnter/Stay/Exit専用に寄せるが、現段階では既存接触登録へ流す。
	void ProcessBlockCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB);

	// Overlapは将来OnOverlapEnter/Stay/Exitへ分けるが、現段階では既存接触登録へ流す。
	void ProcessOverlapCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB);

	// コライダー2つの衝突判定（衝突したら両者へ接触を登録）
	void CheckCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB);

	// 衝突判定関数の登録
	void RegisterCollisionFuncsions();

private: /// ---------- メンバ変数 ---------- ///

	// コライダーの最大タイプ数
	static const uint32_t kMaxTypes = 32;

	// 型ごとのバケット
	std::array<std::vector<K4E::Collider*>, kMaxTypes> buckets_{};

	// 全コライダー（デバッグ描画・イベント解決に使用）
	std::vector<K4E::Collider*> all_;

	// 衝突判定関数の登録
	std::map<std::pair<uint32_t, uint32_t>, CollisionFunc> collisionTable_;

	// UE風ResponseMatrixを保持し、現段階ではIgnore判定だけに使用する。
	CollisionResponseMatrix responseMatrix_{};

	// UE風TraceChannel設定を保持し、現段階では新規Trace入口だけで使用する。
	TraceResponseMatrix traceResponseMatrix_{};

	// Debug表示用に、ResponseMatrixでIgnoreスキップされたペアループ数を保持する。
	uint32_t ignoredPairLoopCount_ = 0;

	// コライダーの可視化フラグ
	bool isCollider_ = true;
};
