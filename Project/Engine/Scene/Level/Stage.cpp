#include "Stage.h"

#include "CollisionManager.h"
#include "LevelLoader.h"
#include "StageAssetLoader.h"
#include "StageCollisionBuilder.h"

namespace Ken4lowEngine
{
	void Stage::Initialize(const std::string& levelJsonPath, const std::string& defaultModelName)
	{
		Clear();

		levelData_ = LevelLoader::LoadLevel(levelJsonPath);
		if (!levelData_)
		{
			return;
		}

		stageModel_ = StageAssetLoader::BuildStageModel(*levelData_, defaultModelName, offset_);
		stageModel_->Update();
		RebuildStageChunks();

		StageCollisionBuildResult collisionResult =
			StageCollisionBuilder::Build(*levelData_, offset_);

		worldAABBs_ = std::move(collisionResult.worldAABBs);
		worldColliders_ = std::move(collisionResult.worldColliders);
	}

	void Stage::Clear()
	{
		levelData_.reset();
		stageModel_.reset();
		stageChunkManager_.Clear();
		worldAABBs_.clear();
		worldColliders_.clear();
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
		if (!stageModel_)
		{
			return;
		}

		if (stageChunkManager_.NeedsRebuild())
		{
			RebuildStageChunks();
		}

		if (stageChunkManager_.IsEnabled() && !stageChunkManager_.GetChunks().empty())
		{
			stageChunkManager_.UpdateVisibility(true);
			stageChunkManager_.DrawVisibleChunks();
			return;
		}

		stageChunkManager_.UpdateVisibility(false);
		stageModel_->Draw();
	}

	void Stage::DrawChunkDebug()
	{
		stageChunkManager_.DrawDebugBounds();
	}

	void Stage::DrawShadow()
	{
		if (stageModel_)
		{
			stageModel_->DrawShadow();
		}
	}

	void Stage::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
	{
		if (stageModel_)
		{
			stageModel_->UpdateShadowMatrix(lightViewProjection);
		}
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
		stageChunkManager_.Rebuild(stageModel_.get(), stageChunkManager_.GetChunkSize());
	}

	void Stage::RegisterColliders(CollisionManager* collisionManager)
	{
		if (!collisionManager)
		{
			return;
		}

		for (auto& collider : worldColliders_)
		{
			collisionManager->AddCollider(collider.get());
		}
	}
} // namespace Ken4lowEngine