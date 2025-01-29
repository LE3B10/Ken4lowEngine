#pragma once
#include <Sprite.h>
#include <TextureManager.h>
#include <Object3D.h>
#include <Object3DCommon.h>
#include <WavLoader.h>
#include <SRVManager.h>

#include <BaseScene.h>

#include "ParticleEmitter.h"

#include "CollisionManager.h"
#include "Player.h"
#include "Floor.h"

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
	
	// すべての衝突判定と応答
	void CheckAllCollisions();

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<ParticleEmitter> fireEmitter;

	DirectXCommon* dxCommon_ = nullptr;
	TextureManager* textureManager = nullptr;
	Input* input_ = nullptr;
	ParticleManager* particleManager = nullptr;
	SRVManager* srvManager = nullptr;

	std::unique_ptr<Camera> camera_;
	std::unique_ptr<WavLoader> wavLoader_;
	std::unique_ptr<Object3DCommon> object3DCommon_;

	std::vector<std::unique_ptr<Sprite>> sprites_;

	std::string particleGroupName;

	// 衝突マネージャ
	std::unique_ptr<CollisionManager> collisionManager_;

	std::unique_ptr<Player> player_;
	std::unique_ptr<Floor> floorBlock_;
};
