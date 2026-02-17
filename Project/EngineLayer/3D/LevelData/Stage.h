#pragma once
#include <memory>
#include <string>

namespace Ken4lowEngine
{
	class LevelLoader;
	class LevelObjectManager;
	struct LevelData;

	class Stage
	{
	public:
		Stage() = default;
		~Stage() = default;

		// レベルJSONを読み込み、ステージを生成
		// levelJsonPath: 例 "Resources/JSON/levels/doom_like.json"
		// defaultModelName: model指定が無い場合に使う（例 "Stage/Stage_All.gltf"）
		bool Load(const std::string& levelJsonPath, const std::string& defaultModelName);

		void Unload();

		void Update(float dt);
		void Draw();

		// 必要なら当たり判定マネージャ/オブジェクトマネージャへアクセス
		LevelObjectManager* GetObjectManager() { return objectManager_.get(); }
		const LevelObjectManager* GetObjectManager() const { return objectManager_.get(); }

	private:
		std::unique_ptr<LevelLoader> loader_;
		std::unique_ptr<LevelObjectManager> objectManager_;
		std::unique_ptr<LevelData> levelData_;

		bool loaded_ = false;
	};
} // namespace Ken4lowEngine