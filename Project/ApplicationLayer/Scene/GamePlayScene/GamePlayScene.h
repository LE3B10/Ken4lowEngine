#pragma once
#include <BaseScene.h>
#include <Sprite.h>
#include <SkyBox.h>
#include "CollisionManager.h"
#include "Player.h"

#include <memory>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }


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

	// Debug用更新処理
	void UpdateDebug();

private: /// ---------- メンバ変数 ---------- ///

	K4E::DirectXCommon* dxCommon_ = nullptr;
	K4E::Input* input_ = nullptr;

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突マネージャー

	std::unique_ptr<K4E::SkyBox> skyBox_ = nullptr; // スカイボックス

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;
	bool isLockedCursor_ = false;

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Player> player_ = nullptr; // プレイヤー
};
