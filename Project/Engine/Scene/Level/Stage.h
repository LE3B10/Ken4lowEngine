#pragma once
#include "AABB.h"
#include "Collider.h"
#include "OBB.h"
#include "Object3D.h"
#include "LevelData.h"
#include "OcclusionCullingSystem.h"
#include "StageChunkManager.h"

#include <memory>
#include <string>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class CollisionManager;

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				ステージ実行時クラス
	///	--------------------------------------------------------------
	class Stage
	{
	public: /// ---------- メンバ関数 ---------- ///

		Stage() = default;
		~Stage() = default;

		/// <summary>
		/// ステージを初期化する
		/// ・LevelLoader でレベルデータを読む
		/// ・StageAssetLoader で描画モデルを作る
		/// ・StageCollisionBuilder で衝突情報を作る
		/// </summary>
		void Initialize(const std::string& levelJsonPath, const std::string& defaultModelName);

		/// <summary>
		/// ステージ内部状態を破棄して空に戻す
		/// </summary>
		void Clear();

		/// <summary>
		/// 更新処理
		/// </summary>
		void Update();

		/// <summary>
		/// 描画処理
		/// </summary>
		void Draw();

		/// <summary>
		/// StageChunk Bounds のデバッグワイヤーを描画する。
		/// </summary>
		void DrawChunkDebug();

		/// <summary>
		/// シャドウ描画
		/// </summary>
		void DrawShadow();

		/// <summary>
		/// シャドウ行列更新
		/// </summary>
		void UpdateShadowMatrix(const Matrix4x4& lightViewProjection);

		/// <summary>
		/// ステージ描画モデルを Frustum Culling 対象にするか設定する。
		/// </summary>
		void SetFrustumCullingEnabled(bool enabled);

		void SetStageChunkCullingEnabled(bool enabled);
		bool IsStageChunkCullingEnabled() const;
		void SetStageChunkBoundsVisible(bool visible);
		bool IsStageChunkBoundsVisible() const;
		void SetStageChunkObjectBoundsVisible(bool visible);
		bool IsStageChunkObjectBoundsVisible() const;
		void SetStageChunkAutoExcludeLargeObjects(bool enabled);
		bool IsStageChunkAutoExcludeLargeObjects() const;
		void SetStageChunkSize(float chunkSize);
		float GetStageChunkSize() const;
		void RebuildStageChunks();
		StageChunkManager& GetStageChunkManager() { return stageChunkManager_; }
		const StageChunkManager& GetStageChunkManager() const { return stageChunkManager_; }
		OcclusionCullingSystem& GetOcclusionCullingSystem() { return occlusionCullingSystem_; }
		const OcclusionCullingSystem& GetOcclusionCullingSystem() const { return occlusionCullingSystem_; }

	public: /// ---------- アクセサ ---------- ///

		// StageCollision用データは移動解決向けAABBと汎用通知向けColliderを併存させ、段階統合できるよう保持する。
		const std::vector<AABB>& GetWorldAABBs() const { return worldAABBs_; }
		const std::vector<AABB>& GetFloorAABBs() const { return floorAABBs_; }
		const std::vector<AABB>& GetWallObstacleAABBs() const { return wallObstacleAABBs_; }
		const std::vector<AABB>& GetNavigationObstacleAABBs() const { return navigationObstacleAABBs_; }
		const std::vector<std::unique_ptr<Collider>>& GetWorldColliders() const { return worldColliders_; }
		const std::vector<OBB>& GetWallObstacleOBBs() const { return wallObstacleOBBs_; }
		const std::vector<OBB>& GetNavigationObstacleOBBs() const { return navigationObstacleOBBs_; }

		/// <summary>
		/// 保持しているワールドコライダーを CollisionManager に登録する
		/// </summary>
		void RegisterColliders(CollisionManager* collisionManager);

		const LevelData* GetLevelData() const { return levelData_ ? levelData_.get() : nullptr; }

	private: /// ---------- メンバ変数 ---------- ///

		std::unique_ptr<LevelData> levelData_;
		std::unique_ptr<Object3D> stageModel_;                 // ステージ描画モデル
		StageChunkManager stageChunkManager_;                 // 静的ステージを Chunk 単位で Draw スキップする管理クラス
		OcclusionCullingSystem occlusionCullingSystem_;          // Lv4: 遮蔽物裏の StageChunk Draw を安全側で止める管理クラス
		std::vector<AABB> worldAABBs_;                         // WorldCollisionResolver向けの静的AABB群
		std::vector<AABB> floorAABBs_;                         // 接地・スポーン補正向けFloor AABB
		std::vector<AABB> wallObstacleAABBs_;                  // 横押し出し用Obstacle AABB
		std::vector<AABB> navigationObstacleAABBs_;            // Navigation向け障害物AABB(Floor除外)
		std::vector<std::unique_ptr<Collider>> worldColliders_; // 汎用CollisionManager通知用のWorld Collider
		std::vector<OBB> wallObstacleOBBs_;                  // デバッグ表示用の壁/障害物OBB
		std::vector<OBB> navigationObstacleOBBs_;            // デバッグ表示用のNavigation障害物OBB
		Vector3 offset_ = { 0.0f, 0.0f, 0.0f };               // ステージ全体オフセット
	};
} // namespace Ken4lowEngine
