#pragma once
#include <Sprite.h>
#include <TextureManager.h>
#include <Object3D.h>
#include <Object3DCommon.h>
#include <WavLoader.h>
#include <SRVManager.h>

#include <BaseScene.h>
#include <ParticleEmitter.h>

// クライアント領域サイズ
static const uint32_t kClientWidth = 1280;
static const uint32_t kClientHeight = 720;

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;
class ImGuiManager;


/// -------------------------------------------------------------
///				　		ゲームプレイシーン
/// -------------------------------------------------------------
class GamePlayScene : public BaseScene
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

	void DrawImGui() override;

private: /// ---------- メンバ変数 ---------- ///

	Transform particleTransform;

	DirectXCommon* dxCommon_ = nullptr;
	TextureManager* textureManager = nullptr;
	Input* input_ = nullptr;

	SRVManager* srvManager = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<WavLoader> wavLoader_;

	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::vector<std::unique_ptr<Object3D>> objects3D_;
	std::unique_ptr<Object3DCommon> object3DCommon_;

	// テクスチャのパスをリストで管理
	std::vector<std::string> texturePaths_;
	std::vector<std::string> objectFiles;
	std::vector<Vector3> initialPositions;


	std::unique_ptr<ParticleEmitter> particleEmitter_;
	std::string particleGroupName;

	std::unique_ptr<Object3D > particleObject_;
};

