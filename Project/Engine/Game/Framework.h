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

#include "SceneManager.h"

// クライアント領域サイズ
static const uint32_t kClientWidth = 1280;
static const uint32_t kClientHeight = 720;


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class ImGuiManager;


/// -------------------------------------------------------------
///				　			ゲーム全体
/// -------------------------------------------------------------
class Framework
{
public: /// ---------- メンバ変数 ---------- ///

	// 実行処理
	void Run();

public: /// ---------- 純粋仮想関数 ---------- ///

	// 仮想デストラクタ
	virtual ~Framework() = default;

	// 初期化処理
	virtual void Initialize();

	// 更新処理
	virtual void Update();

	// 描画処理
	virtual void Draw() = 0;

	// 終了処理
	virtual void Finalize();

	// 終了チェック
	virtual bool IsEndRequest() { return endRequest_; }

protected: /// ---------- メンバ変数 ---------- ///

	bool endRequest_ = false;

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

	std::unique_ptr<BaseScene> scene_;

};

