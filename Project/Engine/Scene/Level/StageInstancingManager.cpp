#define NOMINMAX
#include "StageInstancingManager.h"
#include "MineHiddenArenaLayout.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace Ken4lowEngine
{
	namespace
	{
		bool IsStageMeshType(const std::string& type)
		{
			return type == "StaticMesh" || type == "MESH";
		}

		bool ContainsIgnoreCase(const std::string& text, const std::string& token)
		{
			std::string loweredText = text;
			std::string loweredToken = token;
			std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::transform(loweredToken.begin(), loweredToken.end(), loweredToken.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return loweredText.find(loweredToken) != std::string::npos;
		}

		Vector4 ResolveStageInstanceColor(const ObjectData& data)
		{
			if (ContainsIgnoreCase(data.name, "hiddenpassageguide")) return { 0.12f, 0.42f, 0.72f, 1.0f };
			if (ContainsIgnoreCase(data.name, "hiddenpassage")) return { 0.18f, 0.19f, 0.20f, 1.0f };
			if (ContainsIgnoreCase(data.name, "hiddensupport") || ContainsIgnoreCase(data.name, "hiddengate")) return { 0.23f, 0.21f, 0.18f, 1.0f };
			if (ContainsIgnoreCase(data.name, "domearena")) return { 0.22f, 0.21f, 0.19f, 1.0f };
			if (ContainsIgnoreCase(data.name, "domewall") || ContainsIgnoreCase(data.name, "domeentry")) return { 0.19f, 0.18f, 0.17f, 1.0f };
			if (ContainsIgnoreCase(data.name, "domering") || ContainsIgnoreCase(data.name, "domerib")) return { 0.12f, 0.16f, 0.19f, 1.0f };
			if (ContainsIgnoreCase(data.name, "domerockseal")) return { 0.25f, 0.23f, 0.20f, 1.0f };
			if (ContainsIgnoreCase(data.name, "strip")) return { 0.22f, 0.64f, 0.72f, 1.0f };
			if (ContainsIgnoreCase(data.name, "rockdetail")) return { 0.24f, 0.23f, 0.22f, 1.0f };
			if (ContainsIgnoreCase(data.name, "rubble")) return { 0.36f, 0.30f, 0.24f, 1.0f };
			if (ContainsIgnoreCase(data.name, "brokenbeam")) return { 0.25f, 0.21f, 0.18f, 1.0f };
			if (ContainsIgnoreCase(data.name, "raisedfloor") || ContainsIgnoreCase(data.name, "step")) return { 0.36f, 0.33f, 0.28f, 1.0f };
			if (ContainsIgnoreCase(data.name, "beam")) return { 0.17f, 0.19f, 0.20f, 1.0f };
			if (ContainsIgnoreCase(data.name, "pillar")) return { 0.48f, 0.37f, 0.22f, 1.0f };
			if (ContainsIgnoreCase(data.name, "cover")) return { 0.39f, 0.31f, 0.24f, 1.0f };
			if (data.collider.collisionType == "Floor") return { 0.29f, 0.27f, 0.24f, 1.0f };
			if (data.collider.collisionType == "Obstacle") return { 0.32f, 0.30f, 0.28f, 1.0f };
			return data.color;
		}
	}

	void StageInstancingManager::Build(
		const LevelData& levelData,
		const std::string& primaryStageModelPath,
		const Vector3& stageOffset)
	{
		Clear();
		std::map<std::string, std::vector<StageInstanceSource>> sourcesByModel;
		auto appendSource = [&](const ObjectData& data)
		{
			if (!IsStageMeshType(data.type) || data.modelName.empty() || (!primaryStageModelPath.empty() && data.modelName == primaryStageModelPath))
			{
				return;
			}

			StageInstanceSource source{};
			source.modelPath = data.modelName;
			source.worldMatrix = Matrix4x4::MakeAffineMatrix(data.scale, data.rotation, data.position + stageOffset);
			source.color = ResolveStageInstanceColor(data);
			sourcesByModel[source.modelPath].push_back(source);
		};

		for (const ObjectData& data : levelData.objects) appendSource(data);
		const std::vector<ObjectData> hiddenArenaObjects = MineHiddenArenaLayout::Build(levelData);
		for (const ObjectData& data : hiddenArenaObjects) appendSource(data); // Stage2追加区画も既存cubeバッチへ合流させ、DrawCallを増やさない。

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
			if (object) object->Draw();
		}
	}

	void StageInstancingManager::DrawBatchSourcesNormally()
	{
		for (auto& batch : batches_)
		{
			EnsureNormalFallbackObjects(batch);
			for (auto& object : batch.normalFallbackObjects)
			{
				if (object) object->Draw();
			}
		}
	}

	void StageInstancingManager::DrawInstancedBatches()
	{
		for (auto& batch : batches_)
		{
			if (batch.renderer) batch.renderer->Draw();
		}
	}

	void StageInstancingManager::DrawShadow(bool instancingEnabled, bool useInstancedDraw, bool useNormalDraw)
	{
		if (useNormalDraw)
		{
			for (auto& object : uniqueObjects_) if (object) object->DrawShadow();
			if (!instancingEnabled || !useInstancedDraw)
			{
				for (auto& batch : batches_)
				{
					EnsureNormalFallbackObjects(batch);
					for (auto& object : batch.normalFallbackObjects) if (object) object->DrawShadow();
				}
			}
		}
		if (instancingEnabled && useInstancedDraw)
		{
			for (auto& batch : batches_) if (batch.renderer) batch.renderer->DrawShadow();
		}
	}

	void StageInstancingManager::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
	{
		for (auto& object : uniqueObjects_) if (object) object->UpdateShadowMatrix(lightViewProjection);
		for (auto& batch : batches_)
		{
			for (auto& object : batch.normalFallbackObjects) if (object) object->UpdateShadowMatrix(lightViewProjection);
		}
	}

	void StageInstancingManager::EnsureNormalFallbackObjects(StageInstanceBatch& batch)
	{
		if (!batch.normalFallbackObjects.empty()) return;
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
} // namespace Ken4lowEngine
