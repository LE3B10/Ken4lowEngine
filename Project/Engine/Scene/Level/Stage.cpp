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

		std::unique_ptr<LevelData> loaded = LevelLoader::LoadLevel(levelJsonPath);
		if (!loaded)
		{
			return;
		}

		stageModel_ = StageAssetLoader::BuildStageModel(*loaded, defaultModelName, offset_);

		StageCollisionBuildResult collisionResult =
			StageCollisionBuilder::Build(*loaded, offset_);

		worldAABBs_ = std::move(collisionResult.worldAABBs);
		worldColliders_ = std::move(collisionResult.worldColliders);
	}

	void Stage::Clear()
	{
		stageModel_.reset();
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
		if (stageModel_)
		{
			stageModel_->Draw();
		}
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