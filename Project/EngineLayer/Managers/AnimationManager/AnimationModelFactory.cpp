#include "AnimationModelFactory.h"
#include <cassert>

// クラス外での静的変数の定義
std::unordered_map<std::string, std::shared_ptr<AnimationModel>> AnimationModelFactory::modelCache_;

void AnimationModelFactory::PreLoadModel(const std::string& modelName)
{
	if (modelCache_.count(modelName) == 0)
	{
		auto model = std::make_shared<AnimationModel>();
		model->Initialize(modelName);
		modelCache_[modelName] = model;
	}
}

std::shared_ptr<AnimationModel> AnimationModelFactory::CreateInstance(const std::string& modelName)
{
	assert(modelCache_.count(modelName) && "モデル未ロード");

	return modelCache_[modelName]->Clone();
}

void AnimationModelFactory::ClearAll()
{
	for (auto& pair : modelCache_)
	{
		if (pair.second)
		{
			pair.second->Clear();
		}
	}
	modelCache_.clear();
}
