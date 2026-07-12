#pragma once

#include "AbstractSceneFactory.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				　		ゲーム用のシーン工場
	/// -------------------------------------------------------------
	class SceneFactory : public AbstractSceneFactory
	{
	public:
		using SceneCreator = std::function<std::unique_ptr<BaseScene>()>;

		SceneFactory();

		/// <summary>SceneClass名からC++ Sceneインスタンスを生成します。</summary>
		std::unique_ptr<BaseScene> CreateScene(const std::string& sceneClassName) override;

	private:
		void RegisterSceneClass(std::string sceneClassName, SceneCreator creator);

		std::unordered_map<std::string, SceneCreator> creators_; // JSONのSceneNameとC++型名を分離して登録する。
	};
} // namespace Ken4lowEngine
