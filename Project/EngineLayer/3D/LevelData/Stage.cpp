#include "Stage.h"

#include "LevelLoader.h"
#include "LevelObjectManager.h"
#include "LevelData.h"

namespace Ken4lowEngine
{
	bool Stage::Load(const std::string& levelJsonPath, const std::string& defaultModelName)
	{
		Unload();

		loader_ = std::make_unique<LevelLoader>();
		objectManager_ = std::make_unique<LevelObjectManager>();

		// 1) JSON読み込み
		levelData_ = loader_->LoadLevel(levelJsonPath);
		// ↑ ここがあなたの既存の戻り値に合わせて調整ポイント
		// もし shared_ptr なら levelData_ の型を変える

		if (!levelData_)
		{
			loaded_ = false;
			return false;
		}

		// 2) 生成（Room分割glTFにも対応させる）
		// LevelObjectManager::Initialize(LevelData, defaultModelName)
		objectManager_->Initialize(*levelData_, defaultModelName);

		loaded_ = true;
		return true;
	}

	void Stage::Unload()
	{
		loaded_ = false;

		if (objectManager_)
		{
			objectManager_.reset();
		}
		if (loader_)
		{
			loader_.reset();
		}
		if (levelData_)
		{
			levelData_.reset();
		}
	}

	void Stage::Update(float dt)
	{
		(void)dt; // dt未使用警告回避
		if (!loaded_) return;

		// objectManager_側にUpdateがあるなら呼ぶ
		// objectManager_->Update(dt);
	}

	void Stage::Draw()
	{
		if (!loaded_) return;

		// objectManager_側にDrawがあるなら呼ぶ
		objectManager_->Draw();
	}
} // namespace Ken4lowEngine