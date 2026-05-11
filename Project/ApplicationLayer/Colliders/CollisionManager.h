#pragma once
#include <vector>
#include <array>
#include <functional>
#include <map>
#include <unordered_map>
#include <cstdint>
#include "Collider.h"

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

private: /// ---------- メンバ関数 ---------- ///

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

    // コライダーの可視化フラグ
    bool isCollider_ = true;
};
