#pragma once
#include "LevelData.h"
#include <Object3D.h>
#include <AnimationModel.h>
#include "Collider.h"
#include "CollisionManager.h"

#include "AABB.h"

#include <memory>
#include <vector>


/// -------------------------------------------------------------
///				レベルオブジェクトマネージャークラス
/// -------------------------------------------------------------
class LevelObjectManager : Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const LevelData& levelData, const std::string& modelName);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	// コライダーのワールド座標リストを取得
	const std::vector<std::unique_ptr<Collider>>& GetWorldColliders() const { return colliders_; }

	std::vector<AABB> GetWorldAABBs() const {
		std::vector<AABB> result;
		result.reserve(colliders_.size());
		for (const auto& c : colliders_) {
			// 各コライダーの OBB を AABB に変換
			const OBB& obb = c->GetOBB();
			const Vector3& center = obb.center;
			const Vector3& half = obb.size; // すでにHalfSize指定でSet済み
			AABB box = {};
			box.min = center - half;
			box.max = center + half;
			result.push_back(box);
		}
		return result;
	}

private: /// ---------- メンバ変数 ---------- ///

	LevelData levelData_ = {};  // レベルデータ

	// レベルデータから生成されたオブジェクトのリスト
	std::vector<std::unique_ptr<Object3D>> objects_;
	std::vector<std::unique_ptr<AnimationModel>> animationModels_; // アニメーションモデル用のリスト
	std::vector<std::unique_ptr<Collider>> colliders_; // コライダーのリスト

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突マネージャー
};
