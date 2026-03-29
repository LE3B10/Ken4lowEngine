#include "StageAssetLoader.h"

namespace Ken4lowEngine
{
	namespace
	{
		std::string FindStageModelName(const LevelData& levelData, const std::string& defaultModelName)
		{
			for (const ObjectData& data : levelData.objects)
			{
				// MESH を優先して採用したいなら下を有効化
				// if (data.type != "MESH") { continue; }

				if (!data.modelName.empty())
				{
					return data.modelName;
				}
			}

			return defaultModelName;
		}
	}

	std::unique_ptr<Object3D> StageAssetLoader::BuildStageModel(
		const LevelData& levelData,
		const std::string& defaultModelName,
		const Vector3& offset)
	{
		const std::string modelName = FindStageModelName(levelData, defaultModelName);

		auto stageModel = std::make_unique<Object3D>();
		stageModel->Initialize(modelName);
		stageModel->SetTranslate(offset);
		stageModel->SetRotate({ 0.0f, 0.0f, 0.0f });
		stageModel->SetScale({ 1.0f, 1.0f, 1.0f });

		return stageModel;
	}
} // namespace Ken4lowEngine