#include "Stage.h"

#include "CollisionManager.h"
#include "CollisionUtility.h"
#include "LevelLoader.h"
#include "StageAssetLoader.h"
#include "StageCollisionBuilder.h"
#include "Object3DCommon.h"

namespace Ken4lowEngine
{
	void Stage::Initialize(const std::string& levelJsonPath, const std::string& defaultModelName, bool instancedOnly)
	{
		Clear();

		levelData_ = LevelLoader::LoadLevel(levelJsonPath);
		if (!levelData_)
		{
			return;
		}

		const bool effectiveInstancedOnly = instancedOnly || defaultModelName.empty();
		const std::string primaryStageModelPath = effectiveInstancedOnly
			? std::string{}
			: StageAssetLoader::ResolveStageModelName(*levelData_, defaultModelName);
		if (!effectiveInstancedOnly)
		{
			stageModel_ = StageAssetLoader::BuildStageModel(*levelData_, defaultModelName, offset_);
			if (stageModel_)
			{
				stageModel_->Update();
				RebuildStageChunks();
			}
		}
		stageInstancingManager_.Build(*levelData_, primaryStageModelPath, offset_); // 空Model指定では全StaticMesh配置をGPUバッチ対象にする。

		StageCollisionBuildResult collisionResult =
			StageCollisionBuilder::Build(*levelData_, offset_);

		worldAABBs_ = std::move(collisionResult.worldAABBs);
		floorAABBs_ = std::move(collisionResult.floorAABBs);
		wallObstacleAABBs_ = std::move(collisionResult.wallObstacleAABBs);
		navigationObstacleAABBs_ = std::move(collisionResult.navigationObstacleAABBs);
		worldColliders_ = std::move(collisionResult.worldColliders);
		wallObstacleOBBs_ = std::move(collisionResult.wallObstacleOBBs);
		wallObstacleWalkable_ = std::move(collisionResult.wallObstacleWalkable);
		navigationObstacleOBBs_ = std::move(collisionResult.navigationObstacleOBBs);
		ladderColliders_ = std::move(collisionResult.ladderColliders);
		ladderAABBs_ = std::move(collisionResult.ladderAABBs);
		ladderOBBs_ = std::move(collisionResult.ladderOBBs);
		occlusionCullingSystem_.BuildAutoOccludersFromWorldAABBs(worldAABBs_);
	}

	void Stage::Clear()
	{
		levelData_.reset();
		stageModel_.reset();
		stageInstancingManager_.Clear();
		stageChunkManager_.Clear();
		occlusionCullingSystem_.ClearOccluders();
		worldAABBs_.clear();
		floorAABBs_.clear();
		wallObstacleAABBs_.clear();
		navigationObstacleAABBs_.clear();
		worldColliders_.clear();
		wallObstacleOBBs_.clear();
		wallObstacleWalkable_.clear();
		navigationObstacleOBBs_.clear();
		ladderColliders_.clear();
		ladderAABBs_.clear();
		ladderOBBs_.clear();
	}

	void Stage::Update()
	{
		if (stageModel_)
		{
			stageModel_->Update();
		}
	}

	void Stage::Draw()
	{
		if (useNormalStageDraw_ && stageModel_ && stageChunkManager_.NeedsRebuild())
		{
			RebuildStageChunks();
		}

		if (useNormalStageDraw_ && stageModel_)
		{
			if (stageChunkManager_.IsEnabled() && !stageChunkManager_.GetChunks().empty())
			{
				stageChunkManager_.UpdateVisibility(true);
				stageChunkManager_.ApplyOcclusionCulling(
					occlusionCullingSystem_,
					Object3DCommon::GetInstance()->GetFrustumCullingSystem().GetViewProjectionMatrix());
				stageChunkManager_.DrawVisibleChunks();
			}
			else
			{
				stageChunkManager_.UpdateVisibility(false);
				stageModel_->Draw();
			}
		}

		if (useNormalStageDraw_)
		{
			stageInstancingManager_.DrawUniqueObjects();
			if (!stageInstancingEnabled_ || !useInstancedStageDraw_)
			{
				stageInstancingManager_.DrawBatchSourcesNormally();
			}
		}
		if (stageInstancingEnabled_ && useInstancedStageDraw_)
		{
			stageInstancingManager_.DrawInstancedBatches();
		}
	}

	void Stage::DrawChunkDebug()
	{
		stageChunkManager_.SetShowOccludedBounds(occlusionCullingSystem_.IsShowOccludedBounds());
		stageChunkManager_.DrawDebugBounds();
		occlusionCullingSystem_.DrawDebugBounds();
	}

	void Stage::DrawShadow()
	{
		if (useNormalStageDraw_ && stageModel_)
		{
			stageModel_->DrawShadow();
		}
		stageInstancingManager_.DrawShadow(stageInstancingEnabled_, useInstancedStageDraw_, useNormalStageDraw_); // 一体型Modelが無いStage 2もインスタンス群から影を描く。
	}

	void Stage::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
	{
		if (stageModel_)
		{
			stageModel_->UpdateShadowMatrix(lightViewProjection);
		}
		stageInstancingManager_.UpdateShadowMatrix(lightViewProjection);
	}

	void Stage::SetFrustumCullingEnabled(bool enabled)
	{
		if (stageModel_)
		{
			// まずはステージの静的 Object3D だけを Draw スキップ対象にする。
			stageModel_->SetFrustumCullingEnabled(enabled);
		}
	}

	void Stage::SetStageChunkCullingEnabled(bool enabled)
	{
		stageChunkManager_.SetEnabled(enabled);
	}

	bool Stage::IsStageChunkCullingEnabled() const
	{
		return stageChunkManager_.IsEnabled();
	}

	void Stage::SetStageChunkBoundsVisible(bool visible)
	{
		stageChunkManager_.SetShowBounds(visible);
	}

	bool Stage::IsStageChunkBoundsVisible() const
	{
		return stageChunkManager_.IsShowBounds();
	}

	void Stage::SetStageChunkObjectBoundsVisible(bool visible)
	{
		stageChunkManager_.SetShowObjectBounds(visible);
	}

	bool Stage::IsStageChunkObjectBoundsVisible() const
	{
		return stageChunkManager_.IsShowObjectBounds();
	}

	void Stage::SetStageChunkAutoExcludeLargeObjects(bool enabled)
	{
		stageChunkManager_.SetAutoExcludeLargeObjects(enabled);
	}

	bool Stage::IsStageChunkAutoExcludeLargeObjects() const
	{
		return stageChunkManager_.IsAutoExcludeLargeObjects();
	}

	void Stage::SetStageChunkSize(float chunkSize)
	{
		stageChunkManager_.SetChunkSize(chunkSize);
		stageChunkManager_.MarkRebuildRequested();
	}

	float Stage::GetStageChunkSize() const
	{
		return stageChunkManager_.GetChunkSize();
	}

	void Stage::RebuildStageChunks()
	{
		if (!stageModel_)
		{
			stageChunkManager_.Clear();
			return; // Instanced-only Stageは一体型Model用Chunkを生成しない。
		}
		stageChunkManager_.Rebuild(stageModel_.get(), stageChunkManager_.GetChunkSize());
	}

	void Stage::RegisterColliders(CollisionManager* collisionManager)
	{
		(void)collisionManager;
		// Stage2の全Static ColliderをLegacy総当たりへ登録せず、Player Runtimeが周辺集合だけをPhysicsとLegacyへ同期する。
	}

	bool Stage::CheckLadderOverlap(const AABB& playerAABB) const
	{
		// Triggerイベント配送に依存せず、専用AABB群との交差だけで梯子エリア滞在を判定する。
		for (const AABB& ladderAABB : ladderAABBs_)
		{
			if (CollisionUtility::IsCollision(playerAABB, ladderAABB))
			{
				return true;
			}
		}
		return false;
	}

	std::vector<Collider*> Stage::GetWorldColliderPointers() const
	{
		// Stageが所有するWorld Colliderを、PhysicsWorldへ渡せる参照ポインタ一覧に変換する。
		std::vector<Collider*> colliders{};
		colliders.reserve(worldColliders_.size());
		for (const std::unique_ptr<Collider>& collider : worldColliders_)
		{
			if (collider)
			{
				colliders.push_back(collider.get());
			}
		}
		return colliders;
	}
} // namespace Ken4lowEngine
