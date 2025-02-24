#pragma once
#include <Sprite.h>
#include <TextureManager.h>
#include <Object3D.h>
#include <WavLoader.h>
#include "ParticleManager.h"
#include <BaseScene.h>
#include "AnimationManager.h"

#include "CollisionManager.h"

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


private: /// ---------- メンバ関数 ---------- ///

	// 衝突判定と応答
	void CheckAllCollisions();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	TextureManager* textureManager = nullptr;
	Input* input_ = nullptr;
	ParticleManager* particleManager = nullptr;

	std::unique_ptr<WavLoader> wavLoader_;
	std::unique_ptr<Object3D> objectTerrain_;
	std::unique_ptr<Object3D> objectBall_;
	std::unique_ptr<AnimationManager> animationManager_;
	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::unique_ptr<CollisionManager> collisionManager_;

	const std::string& particleGroupName = "Particle";

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;
};
