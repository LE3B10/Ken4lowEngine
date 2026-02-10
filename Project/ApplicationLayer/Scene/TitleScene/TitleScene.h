#pragma once
#include <Sprite.h>
#include <Object3D.h>
#include <BaseScene.h>
#include "SkyBox.h"

#include <memory>
#include <numbers>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }
namespace Ken4lowEngine { class Camera; }
class SceneManager;

/// -------------------------------------------------------------
///					　ゲームタイトルシーンクラス
/// -------------------------------------------------------------
class TitleScene : public BaseScene
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

	K4E::DirectXCommon* dxCommon_ = nullptr; // DirectX共通管理
	K4E::Input* input_ = nullptr;			// 入力管理
	K4E::Camera* camera_ = nullptr;			// メインカメラ

	bool isDebugCamera_ = false; // デバッグモード

	std::unique_ptr<K4E::SkyBox> skyBox_;	// スカイボックス
	std::unique_ptr<K4E::Object3D> terrain_; // 地形オブジェクト
};

