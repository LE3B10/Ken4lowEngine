#pragma once
#include "BaseScene.h"
#include "Sprite.h"
#include <Boss.h>

#include <Sprite.h>
#include <Object3D.h>
#include <SkyBox.h>


#include <vector>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;

/// -------------------------------------------------------------
///					　	デバッグシーン
/// -------------------------------------------------------------
class DebugScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想初期化処理
	void Initialize() override;

	// 仮想更新処理
	void Update() override;

	// 仮想3D描画処理
	void Draw3DObjects() override;

	// 仮想2D描画処理
	void Draw2DSprites() override;

	// 仮想終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ関数 ---------- ///

	// デバッグカメラの更新
	void UpdateDebug();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr; // DirectXCommonのポインタ
	Input* input_ = nullptr; // Inputのポインタ

	std::unique_ptr<Boss> boss_; // ボス

	std::unique_ptr<Sprite> sprite_; // スプライト
	std::unique_ptr<Object3D> object3D_; // 3Dオブジェクト
	std::unique_ptr<SkyBox> skyBox_; // スカイボックス

	bool isDebugCamera_ = false; // デバッグカメラ使用フラグ
};

