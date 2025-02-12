#pragma once
#include <list>
#include <memory>

#include "Vector3.h"


/// ---------- 前方宣言 ---------- ///
class Collider;


/// -------------------------------------------------------------
///						当たり判定管理クラス
/// -------------------------------------------------------------
class CollisionManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理・可視化処理用
	void Initialize();

	// 更新処理・可視化処理用
	void Update();

	// 描画処理・可視化処理用
	void Draw();

	// リセット処理
	void Reset();

	// すべての当たり判定を確認する処理
	void CheckAllCollisions();

	// コライダーを追加
	void AddCollider(Collider* other);

	// ImGuiを描画
	void DrawImGui();

private: /// ---------- メンバ関数 ---------- ///

	// コライダー2つの衝突判定と応答処理
	void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

	// AABB同士の衝突判定（静的衝突判定用）
	bool IsAABBCollision(Collider* colliderA, Collider* colliderB);

private: /// ---------- メンバ変数 ---------- ///

	// コライダーリスト
	std::list<Collider*> colliders_;

	// コライダーの可視化フラグ
	bool isCollider_ = false;

};

