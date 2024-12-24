#pragma once
#include "BaseScene.h"
#include <memory>
#include <stdexcept>
#include <string>


/// -------------------------------------------------------------
///				　　 　シーン工場　・　抽象クラス
/// -------------------------------------------------------------
class AbstractSceneFactory
{
public: /// ---------- 仮想メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~AbstractSceneFactory() = default;

	// シーン生成
	virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;

};

