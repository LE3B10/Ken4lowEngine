#pragma once
#include <list>
#include <vector>
#include <array>
#include <functional>
#include <map>
#include <memory>

#include "Vector3.h"
#include "OBB.h"

namespace K4E = ::Ken4lowEngine;


/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Collider; }


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
	void AddCollider(K4E::Collider* other);

	// コライダーを削除
	void RemoveCollider(K4E::Collider* other);

	// 衝突判定
	using CollisionFunc = std::function<bool(K4E::Collider*, K4E::Collider*)>;

private: /// ---------- メンバ関数 ---------- ///

	// コライダー2つの衝突判定と応答処理
	void CheckCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB);

	// 初期化関数
	void RegisterCollisionFuncsions();

private: /// ---------- メンバ変数 ---------- ///

	// コライダーの最大タイプ数
	static const uint32_t kMaxTypes = 32;

	// 型ごとのバケット
	std::array<std::vector<K4E::Collider*>, kMaxTypes> buckets_;

	// デバッグ用
	std::vector<K4E::Collider*> all_;

	// 衝突判定関数の登録
	std::map<std::pair<uint32_t, uint32_t>, CollisionFunc> collisionTable_;

	// コライダーリスト
	//std::list<K4E::Collider*> colliders_;

	// コライダーの可視化フラグ
	bool isCollider_ = true;
};

