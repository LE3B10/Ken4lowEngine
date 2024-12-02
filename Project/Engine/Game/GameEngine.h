#pragma once
#include "WinApp.h"
#include "Input.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "D3DResourceLeakChecker.h"
#include "LogString.h"
#include "PipelineStateManager.h"
#include "ResourceManager.h"
#include "ModelManager.h"

#include "Framework.h"
#include <GamePlayScene.h>

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

	WinApp* winApp;
	DirectXCommon* dxCommon;
	Input* input;
	ImGuiManager* imguiManager;
	TextureManager* textureManager;
	ModelManager* modelManager;

	std::unique_ptr<SRVManager> srvManager;
	std::unique_ptr<Camera> camera_;
	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::vector<std::unique_ptr<Object3D>> objects3D_;
	std::unique_ptr<Object3DCommon> object3DCommon_;
	std::unique_ptr<PipelineStateManager> pipelineStateManager_;
	std::unique_ptr<WavLoader> wavLoader_;

	// テクスチャのパスをリストで管理
	std::vector<std::string> texturePaths_;
	std::vector<std::string> objectFiles;
	std::vector<Vector3> initialPositions;


private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<GamePlayScene> scene_;

};

