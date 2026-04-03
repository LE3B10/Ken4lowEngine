#pragma once
#include "LevelData.h"
#include "Object3D.h"

#include <memory>
#include <string>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				ステージ描画アセット生成
	/// -------------------------------------------------------------
	class StageAssetLoader
	{
	public:
		/// <summary>
		/// LevelData からステージ用描画モデルを構築する
		/// </summary>
		static std::unique_ptr<Object3D> BuildStageModel(
			const LevelData& levelData,
			const std::string& defaultModelName,
			const Vector3& offset);
	};
} // namespace Ken4lowEngine