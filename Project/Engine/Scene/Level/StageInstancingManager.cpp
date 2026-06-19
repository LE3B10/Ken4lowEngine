#define NOMINMAX
#include "StageInstancingManager.h"

#include <map>

namespace Ken4lowEngine
{
	namespace
	{
		bool IsStageMeshType(const std::string& type)
		{
			return type == "StaticMesh" || type == "MESH";
		}
	}

	void StageInstancingManager::Build(
		const LevelData& levelData,
		const std::string& primaryStageModelPath,
		const Vector3& stageOffset)
	{
		Clear();
		std::map<std::string, std::vector<StageInstanceSource>> sourcesByModel;
		for (const ObjectData& data : levelData.objects)
		{
			// 一体型ステージはStageChunkManagerへ残し、明示的な外部モデル配置だけを安全に抽出する。
			if (!IsStageMeshType(data.type) || data.modelName.empty() || data.modelName == primaryStageModelPath)
			{
				continue;
			}

			StageInstanceSource source{};
			source.modelPath = data.modelName;
			source.worldMatrix = Matrix4x4::MakeAffineMatrix(data.scale, data.rotation, data.position + stageOffset);
			sourcesByModel[source.modelPath].push_back(source);
		}

		for (auto& [modelPath, sources] : sourcesByModel)
		{
			if (sources.size() < 2)
			{
				uniqueObjects_.push_back(CreateNormalObject(sources.front()));
				continue;
			}

			StageInstanceBatch batch{};
			batch.modelPath = modelPath;
			batch.sources = std::move(sources);
			batch.instances.reserve(batch.sources.size());
			for (const StageInstanceSource& source : batch.sources)
			{
				InstancedObject3DRenderer::InstanceData instance{};
				instance.world = source.worldMatrix;
				instance.worldInverseTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(source.worldMatrix));
				instance.color = source.color;
				batch.instances.push_back(instance);
			}

			batch.renderer = std::make_unique<InstancedObject3DRenderer>();
			batch.renderer->Initialize(batch.modelPath, batch.instances.size());
			batch.renderer->SetFrustumCullingEnabled(true);
			batch.renderer->SetInstances(batch.instances);
			totalInstanceCount_ += batch.instances.size();
			batches_.push_back(std::move(batch));
		}
	}

	void StageInstancingManager::Clear()
	{
		batches_.clear();
		uniqueObjects_.clear();
		totalInstanceCount_ = 0;
	}

	void StageInstancingManager::DrawUniqueObjects()
	{
		for (auto& object : uniqueObjects_)
		{
			if (object) { object->Draw(); }
		}
	}

	void StageInstancingManager::DrawBatchSourcesNormally()
	{
		for (auto& batch : batches_)
		{
			EnsureNormalFallbackObjects(batch);
			for (auto& object : batch.normalFallbackObjects)
			{
				if (object) { object->Draw(); }
			}
		}
	}

	void StageInstancingManager::DrawInstancedBatches()
	{
		for (auto& batch : batches_)
		{
			if (batch.renderer) { batch.renderer->Draw(); }
		}
	}

	void StageInstancingManager::EnsureNormalFallbackObjects(StageInstanceBatch& batch)
	{
		if (!batch.normalFallbackObjects.empty()) { return; }
		batch.normalFallbackObjects.reserve(batch.sources.size());
		for (const StageInstanceSource& source : batch.sources)
		{
			batch.normalFallbackObjects.push_back(CreateNormalObject(source));
		}
	}

	std::unique_ptr<Object3D> StageInstancingManager::CreateNormalObject(const StageInstanceSource& source) const
	{
		auto object = std::make_unique<Object3D>();
		object->Initialize(source.modelPath);
		object->UpdateWithWorldMatrix(source.worldMatrix);
		object->SetColor(source.color);
		return object;
	}
}
