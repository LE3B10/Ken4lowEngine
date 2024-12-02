#pragma once
#include <Sprite.h>
#include <TextureManager.h>
#include <Object3D.h>
#include <Object3DCommon.h>
#include <WavLoader.h>
#include <SRVManager.h>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class ImGuiManager;

/// -------------------------------------------------------------
///				　		ゲームプレイシーン
/// -------------------------------------------------------------
class GamePlayScene
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 終了処理
	void Finalize();

	void DrawImGui();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	TextureManager* textureManager;

	std::unique_ptr<SRVManager> srvManager;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<WavLoader> wavLoader_;

	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::vector<std::unique_ptr<Object3D>> objects3D_;
	std::unique_ptr<Object3DCommon> object3DCommon_;

	// テクスチャのパスをリストで管理
	std::vector<std::string> texturePaths_;
	std::vector<std::string> objectFiles;
	std::vector<Vector3> initialPositions;

};

