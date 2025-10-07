#pragma once
#include <list>
#include <vector>
#include <array>
#include <functional>
#include <map>
#include <memory>

#include "Vector3.h"
#include "OBB.h"


/// ---------- 前方宣言 ---------- ///
class Collider;


/// -------------------------------------------------------------
///						当たり判定管理クラス
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

	// リセット処理
	void Reset();

	// すべての当たり判定を確認する処理
	void CheckAllCollisions();

	// コライダーを追加
	void AddCollider(Collider* other);

	// コライダーを削除
	void RemoveCollider(Collider* other);

	// 衝突判定
	using CollisionFunc = std::function<bool(Collider*, Collider*)>;

private: /// ---------- メンバ関数 ---------- ///

	// コライダー2つの衝突判定と応答処理
	void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

	// 初期化関数
	void RegisterCollisionFuncsions();

private: /// ---------- メンバ変数 ---------- ///

	static const uint32_t kMaxTypes = 32; // コライダーの最大タイプ数
	std::array<std::vector<Collider*>, kMaxTypes> buckets_; // 型ごとのバケット
	std::vector<Collider*> all_; // デバッグ用
	// 衝突判定関数の登録
	std::map<std::pair<uint32_t, uint32_t>, CollisionFunc> collisionTable_;

	// コライダーリスト
	//std::list<Collider*> colliders_;

	// コライダーの可視化フラグ
	bool isCollider_ = true;

};

