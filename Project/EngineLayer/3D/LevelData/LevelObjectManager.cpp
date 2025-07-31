#include "LevelObjectManager.h"


/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void LevelObjectManager::Initialize(const LevelData& levelData, const std::string& modelName)
{
	objects_.clear();

	for (const auto& data : levelData.objects) {
		std::unique_ptr<Object3D> obj = std::make_unique<Object3D>();
		obj->Initialize(modelName);
		obj->SetTranslate(data.position);
		obj->SetRotate(data.rotation);
		obj->SetScale(data.scale);
		objects_.emplace_back(std::move(obj));
	}
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void LevelObjectManager::Update()
{
	for (auto& obj : objects_)
	{
		obj->Update();
	}
}


/// -------------------------------------------------------------
///				　		    描画処理
/// -------------------------------------------------------------
void LevelObjectManager::Draw()
{
	for (auto& obj : objects_)
	{
		obj->Draw();
	}
}
