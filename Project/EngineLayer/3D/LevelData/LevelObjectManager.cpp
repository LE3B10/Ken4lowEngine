#include "LevelObjectManager.h"

void LevelObjectManager::Initialize(const LevelData& levelData)
{
	objects_.clear();

	for (const auto& data : levelData.objects) {
		std::unique_ptr<Object3D> obj = std::make_unique<Object3D>();
		obj->Initialize("cube.gltf");
		obj->SetTranslate(data.position);
		obj->SetRotate(data.rotation);
		obj->SetScale(data.scale);
		obj->SetReflectivity(0.0f); // 固定値、適宜調整
		objects_.emplace_back(std::move(obj));
	}
}

void LevelObjectManager::Update()
{
	for (auto& obj : objects_)
	{
		obj->Update();
	}
}

void LevelObjectManager::Draw()
{
	for (auto& obj : objects_)
	{
		obj->Draw();
	}
}
