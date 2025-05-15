#pragma once
#include <Sprite.h>
#include <Object3D.h>
#include <WavLoader.h>
#include <MP3Loader.h>
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include <BaseScene.h>

#include "CollisionManager.h"

#include "Player.h"
#include "FollowCamera.h"


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

	std::unique_ptr<Player> player_ = nullptr; // プレイヤーオブジェクト
	std::unique_ptr<FollowCamera> camera_ = nullptr; // カメラオブジェクト

	std::unique_ptr<CollisionManager> collisionManager_;

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;
	bool isLockedCursor_ = false;
};
