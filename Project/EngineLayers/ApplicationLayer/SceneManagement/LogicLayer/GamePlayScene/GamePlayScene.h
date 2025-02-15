#pragma once
#include <Sprite.h>
#include <TextureManager.h>
#include <Object3D.h>
#include <WavLoader.h>
#include "ParticleManager.h"
#include <BaseScene.h>

#include "AABB.h"
#include "OBB.h"

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

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	TextureManager* textureManager = nullptr;
	Input* input_ = nullptr;
	ParticleManager* particleManager = nullptr;

	std::unique_ptr<WavLoader> wavLoader_;
	std::unique_ptr<Object3D> objectTerrain_;
	std::unique_ptr<Object3D> objectBall_;
	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::string particleGroupName;

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;

	float time = 0.0f;

	OBB obb{};

	Vector3 baseCenter = { -10.0f, 0.0f, 0.0f }; // 三角錐の底面の中心
	float baseSize = 2.0f; // 底面の一辺の長さ
	float height = 3.0f; // 頂点の高さ
	Vector3 axis = { 0.0f, 1.0f, 0.0f }; // Y軸方向に立つ三角錐
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色

	Vector3 center = { 0.0f, 0.0f, 0.0f }; // 五角柱の中心
	float radius = 2.0f; // 底面の外接円半径
};
