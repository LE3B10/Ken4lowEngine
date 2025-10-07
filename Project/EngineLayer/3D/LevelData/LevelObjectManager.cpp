#include "LevelObjectManager.h"


/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void LevelObjectManager::Initialize(const LevelData& levelData, const std::string& modelName)
{
	objects_.clear();

	for (const auto& data : levelData.objects)
	{
		if (data.type == "Mesh")
		{
			std::unique_ptr<Object3D> obj = std::make_unique<Object3D>();
			obj->Initialize(modelName);
			obj->SetTranslate(data.position);
			obj->SetRotate(data.rotation);
			obj->SetScale(data.scale);
			objects_.emplace_back(std::move(obj));
		}
		else if (data.type == "PlayerSpawnPoint")
		{
			// ここでプレイヤーモデル（スキニング対応）を生成
			std::unique_ptr<AnimationModel> animationModel = std::make_unique<AnimationModel>();
			animationModel->Initialize(modelName, true); // スキニングを有効にする
			animationModel->SetTranslate(data.position);
			animationModel->SetRotate(data.rotation);
			animationModel->SetScale(data.scale);
			animationModels_.emplace_back(std::move(animationModel));
		}
	}
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void LevelObjectManager::Update()
{
	// オブジェクトの更新
	for (auto& obj : objects_)
	{
		obj->Update();
	}

	// アニメーションモデルの更新を追加
	for (auto& anim : animationModels_)
	{
		anim->Update();
	}
}


/// -------------------------------------------------------------
///				　		    描画処理
/// -------------------------------------------------------------
void LevelObjectManager::Draw()
{
	// オブジェクトの描画
	for (auto& obj : objects_)
	{
		obj->Draw();
	}

	// アニメーションモデルの描画を追加
	for (auto& anim : animationModels_)
	{
		anim->Draw();
	}
}
