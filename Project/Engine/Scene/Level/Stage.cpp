#include "Stage.h"

#include "CollisionManager.h"
#include "CollisionUtility.h"
#include "LevelLoader.h"
#include "StageAssetLoader.h"
#include "StageCollisionBuilder.h"
#include "Object3DCommon.h"

#include <algorithm>
#include <array>
#include <string>

namespace
{
	using Ken4lowEngine::LevelData;
	using Ken4lowEngine::ObjectData;
	using Ken4lowEngine::Vector3;

	bool IsExpandedMineStage(const std::string& levelJsonPath)
	{
		return levelJsonPath.find("wasureraretakoudou") != std::string::npos;
	}

	void AddMineBox(
		LevelData& levelData,
		std::string name,
		const Vector3& position,
		const Vector3& scale,
		const char* collisionType = nullptr)
	{
		ObjectData data{};
		data.name = std::move(name);
		data.type = "StaticMesh";
		data.modelName = "Sample/cube.gltf";
		data.position = position;
		data.scale = scale;
		if (collisionType)
		{
			data.collider.enabled = true;
			data.collider.type = "BOX";
			data.collider.size = { 2.0f, 2.0f, 2.0f };
			data.collider.collisionType = collisionType;
		}
		levelData.objects.push_back(std::move(data));
	}

	void BuildExpandedMineLayout(LevelData& levelData)
	{
		levelData.objects.erase(
			std::remove_if(
				levelData.objects.begin(),
				levelData.objects.end(),
				[](const ObjectData& data)
				{
					return data.type == "StaticMesh" || data.type == "MESH";
				}),
			levelData.objects.end());
		levelData.objects.reserve(levelData.objects.size() + 96u);

		for (int index = 0; index < 10; ++index)
		{
			const float z = -46.0f + static_cast<float>(index) * 18.0f;
			AddMineBox(
				levelData,
				"Floor_Long_" + std::to_string(index + 1),
				{ 0.0f, -0.5f, z },
				{ 50.0f, 0.5f, 9.0f },
				"Floor");
		}

		AddMineBox(levelData, "Wall_South", { 0.0f, 3.0f, -56.0f }, { 51.0f, 3.0f, 1.0f }, "Obstacle");
		AddMineBox(levelData, "Wall_North", { 0.0f, 3.0f, 126.0f }, { 51.0f, 3.0f, 1.0f }, "Obstacle");
		AddMineBox(levelData, "Wall_West", { -51.0f, 3.0f, 35.0f }, { 1.0f, 3.0f, 91.0f }, "Obstacle");
		AddMineBox(levelData, "Wall_East", { 51.0f, 3.0f, 35.0f }, { 1.0f, 3.0f, 91.0f }, "Obstacle");

		AddMineBox(levelData, "EntryGate_L", { -30.0f, 2.5f, -34.0f }, { 20.0f, 2.5f, 1.0f }, "Obstacle");
		AddMineBox(levelData, "EntryGate_R", { 30.0f, 2.5f, -34.0f }, { 20.0f, 2.5f, 1.0f }, "Obstacle");

		struct BoxSpec
		{
			const char* name;
			Vector3 position;
			Vector3 scale;
		};

		constexpr std::array<BoxSpec, 16> walls = {{
			{ "MainWall_L_01", { -14.0f, 2.5f, -24.0f }, { 1.0f, 2.5f, 8.0f } },
			{ "MainWall_L_02", { -14.0f, 2.5f, 10.0f }, { 1.0f, 2.5f, 16.0f } },
			{ "MainWall_L_03", { -14.0f, 2.5f, 45.0f }, { 1.0f, 2.5f, 8.0f } },
			{ "MainWall_R_01", { 14.0f, 2.5f, -18.0f }, { 1.0f, 2.5f, 14.0f } },
			{ "MainWall_R_02", { 14.0f, 2.5f, 20.0f }, { 1.0f, 2.5f, 10.0f } },
			{ "MainWall_R_03", { 14.0f, 2.5f, 60.0f }, { 1.0f, 2.5f, 7.0f } },
			{ "WestRoom_South", { -33.0f, 2.5f, -25.0f }, { 17.0f, 2.5f, 1.0f } },
			{ "WestRoom_North", { -33.0f, 2.5f, 7.0f }, { 17.0f, 2.5f, 1.0f } },
			{ "EastRoom_South", { 33.0f, 2.5f, 30.0f }, { 17.0f, 2.5f, 1.0f } },
			{ "EastRoom_North", { 33.0f, 2.5f, 62.0f }, { 17.0f, 2.5f, 1.0f } },
			{ "DeepGate_L", { -31.0f, 3.0f, 72.0f }, { 19.0f, 3.0f, 1.0f } },
			{ "DeepGate_R", { 31.0f, 3.0f, 72.0f }, { 19.0f, 3.0f, 1.0f } },
			{ "DeepRoom_L", { -32.0f, 2.5f, 99.0f }, { 1.0f, 2.5f, 26.0f } },
			{ "DeepRoom_R", { 32.0f, 2.5f, 99.0f }, { 1.0f, 2.5f, 26.0f } },
			{ "DeepBack_L", { -22.0f, 2.5f, 119.0f }, { 10.0f, 2.5f, 1.0f } },
			{ "DeepBack_R", { 22.0f, 2.5f, 119.0f }, { 10.0f, 2.5f, 1.0f } }
		}};
		for (const BoxSpec& wall : walls)
		{
			AddMineBox(levelData, wall.name, wall.position, wall.scale, "Obstacle");
		}

		constexpr std::array<Vector3, 24> pillarPositions = {{
			{ -8.0f, 2.2f, -44.0f }, { 8.0f, 2.2f, -44.0f },
			{ -8.0f, 2.2f, -24.0f }, { 8.0f, 2.2f, -24.0f },
			{ -40.0f, 2.2f, -18.0f }, { -28.0f, 2.2f, -18.0f },
			{ -40.0f, 2.2f, 0.0f }, { -28.0f, 2.2f, 0.0f },
			{ -8.0f, 2.2f, 4.0f }, { 8.0f, 2.2f, 4.0f },
			{ -8.0f, 2.2f, 28.0f }, { 8.0f, 2.2f, 28.0f },
			{ 28.0f, 2.2f, 36.0f }, { 40.0f, 2.2f, 36.0f },
			{ 28.0f, 2.2f, 56.0f }, { 40.0f, 2.2f, 56.0f },
			{ -8.0f, 2.2f, 58.0f }, { 8.0f, 2.2f, 58.0f },
			{ -24.0f, 2.2f, 82.0f }, { 24.0f, 2.2f, 82.0f },
			{ -24.0f, 2.2f, 104.0f }, { 24.0f, 2.2f, 104.0f },
			{ -8.0f, 2.2f, 112.0f }, { 8.0f, 2.2f, 112.0f }
		}};
		for (size_t index = 0; index < pillarPositions.size(); ++index)
		{
			AddMineBox(
				levelData,
				"Pillar_" + std::to_string(index + 1),
				pillarPositions[index],
				{ 1.2f, 2.2f, 1.2f },
				"Pillar");
		}

		constexpr std::array<BoxSpec, 14> covers = {{
			{ "Cover_01", { -6.0f, 1.1f, -46.0f }, { 2.2f, 1.1f, 1.2f } },
			{ "Cover_02", { 7.0f, 1.1f, -40.0f }, { 1.5f, 1.1f, 1.8f } },
			{ "Cover_03", { -6.0f, 1.1f, -14.0f }, { 2.0f, 1.1f, 1.0f } },
			{ "Cover_04", { -41.0f, 1.1f, -8.0f }, { 2.5f, 1.1f, 1.2f } },
			{ "Cover_05", { -26.0f, 1.1f, -2.0f }, { 1.8f, 1.1f, 1.8f } },
			{ "Cover_06", { 6.0f, 1.1f, 18.0f }, { 1.3f, 1.1f, 2.0f } },
			{ "Cover_07", { 36.0f, 1.1f, 38.0f }, { 2.3f, 1.1f, 1.1f } },
			{ "Cover_08", { 27.0f, 1.1f, 49.0f }, { 1.7f, 1.1f, 1.7f } },
			{ "Cover_09", { 42.0f, 1.1f, 55.0f }, { 2.0f, 1.1f, 1.0f } },
			{ "Cover_10", { -5.0f, 1.1f, 50.0f }, { 2.0f, 1.1f, 1.0f } },
			{ "Cover_11", { 6.0f, 1.1f, 68.0f }, { 1.2f, 1.1f, 2.2f } },
			{ "Cover_12", { -18.0f, 1.1f, 84.0f }, { 2.3f, 1.1f, 1.2f } },
			{ "Cover_13", { 18.0f, 1.1f, 94.0f }, { 1.8f, 1.1f, 1.8f } },
			{ "Cover_14", { -12.0f, 1.1f, 110.0f }, { 2.0f, 1.1f, 1.0f } }
		}};
		for (const BoxSpec& cover : covers)
		{
			AddMineBox(levelData, cover.name, cover.position, cover.scale, "Obstacle");
		}

		constexpr std::array<float, 9> beamZ = { -44.0f, -24.0f, -4.0f, 16.0f, 36.0f, 56.0f, 78.0f, 98.0f, 116.0f };
		for (size_t index = 0; index < beamZ.size(); ++index)
		{
			const float halfWidth = beamZ[index] < 72.0f ? 14.0f : 30.0f;
			AddMineBox(
				levelData,
				"Beam_" + std::to_string(index + 1),
				{ 0.0f, 5.4f, beamZ[index] },
				{ halfWidth, 0.35f, 0.7f });
		}

		constexpr std::array<float, 10> stripZ = { -46.0f, -28.0f, -10.0f, 8.0f, 26.0f, 44.0f, 62.0f, 80.0f, 98.0f, 116.0f };
		for (size_t index = 0; index < stripZ.size(); ++index)
		{
			AddMineBox(
				levelData,
				"Strip_Main_" + std::to_string(index + 1),
				{ 0.0f, 0.04f, stripZ[index] },
				{ 0.65f, 0.03f, 8.0f });
		}
		AddMineBox(levelData, "Strip_West", { -25.0f, 0.04f, -10.0f }, { 11.0f, 0.03f, 0.55f });
		AddMineBox(levelData, "Strip_East", { 25.0f, 0.04f, 45.0f }, { 11.0f, 0.03f, 0.55f });
		// Stage 2は約100m×180mの長方形へ広げ、全モジュールを同一モデルの1インスタンスバッチに維持する。
	}
}

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
		if (IsExpandedMineStage(levelJsonPath))
		{
			BuildExpandedMineLayout(*levelData_);
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
