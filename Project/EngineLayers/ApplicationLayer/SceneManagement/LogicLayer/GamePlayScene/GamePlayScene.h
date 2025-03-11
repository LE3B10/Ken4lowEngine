#pragma once
#include <Sprite.h>
#include <TextureManager.h>
#include <Object3D.h>
#include <WavLoader.h>
#include "ParticleManager.h"
#include <BaseScene.h>

#include "Player.h"
#include "Skydome.h"
#include "Ground.h"
#include "FollowCamera.h"
#include "Enemy.h"
#include "LockOn.h"
#include "Hammer.h"

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

	std::unique_ptr<Player> player_;
	std::unique_ptr<Skydome> skydome_;
	std::unique_ptr<Ground> ground_;
	std::unique_ptr<FollowCamera> followCamera_;
	std::list<std::unique_ptr<Enemy>> enemies_;
	std::unique_ptr<LockOn> lockOn_;
	std::unique_ptr<Hammer> hammer_;
	
	// 衝突マネージャ
	std::unique_ptr<CollisionManager> collisionManager_;
	
	std::vector<std::unique_ptr<Sprite>> sprites_;



	const std::string& particleGroupName = "Particle";

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;
};
