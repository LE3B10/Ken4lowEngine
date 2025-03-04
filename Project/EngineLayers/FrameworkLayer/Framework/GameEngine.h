#pragma once
#include "Input.h"
#include "D3DResourceLeakChecker.h"
#include "LogString.h"
#include "PipelineStateManager.h"
#include "ResourceManager.h"
#include "ModelManager.h"

#include <Framework.h>
#include <SceneManager.h>


/// -------------------------------------------------------------
///				　	ゲーム全体を管理するクラス
/// -------------------------------------------------------------
class GameEngine : public Framework
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

	// 終了処理
	void Finalize() override;

private: /// ---------- メンバ変数 ---------- ///

	ModelManager* modelManager = nullptr;

	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::vector<std::unique_ptr<Object3D>> objects3D_;
	std::unique_ptr<PipelineStateManager> pipelineStateManager_;
};

