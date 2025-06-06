#pragma once
#include <Sprite.h>
#include <Object3D.h>
#include <WavLoader.h>
#include <MP3Loader.h>
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include <BaseScene.h>
#include "AnimationModel.h"

#include "SkyBox.h"

#include "CollisionManager.h"

#include "AABB.h"
#include "OBB.h"

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;


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

	// 3Dオブジェクトの描画
	void Draw3DObjects() override;

	// 2Dオブジェクトの描画
	void Draw2DSprites() override;

	// 終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;


private: /// ---------- メンバ関数 ---------- ///

	// 衝突判定と応答
	void CheckAllCollisions();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Input* input_ = nullptr;
	ParticleManager* particleManager = nullptr;

	std::unique_ptr<Object3D> objectTerrain_;
	std::unique_ptr<Object3D> objectBall_;
	std::unique_ptr<Sprite> sprites_;
	std::unique_ptr<CollisionManager> collisionManager_;
	std::unique_ptr<SkyBox> skyBox_;
	std::unique_ptr<AnimationModel> animationModel_;
	std::unique_ptr<AnimationModel> animationModel2_;

	// パーティクル
	std::unique_ptr< ParticleEmitter> cylinderEmitter_, magicRingEmitter_;
	std::unique_ptr< ParticleEmitter> muzzleFlashEffect_, smokeEffect_;

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;

	bool isCharging = false;
	float chargeTimer = 0.0f;
};
