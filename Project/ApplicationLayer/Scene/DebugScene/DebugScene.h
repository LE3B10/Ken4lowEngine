#pragma once
#include "BaseScene.h"
#include "Sprite.h"
#include <Boss.h>
#include <SkyBox.h>

#include "FadeManager.h"
#include "SpriteFractureEffect.h"
#include "SpriteDebrisEmitter.h"

#include <vector>
#include <memory>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }

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

	K4E::DirectXCommon* dxCommon_ = nullptr; // DirectXCommonのポインタ
	K4E::Input* input_ = nullptr; // Inputのポインタ

	std::unique_ptr<Boss> boss_; // ボス

	std::unique_ptr<K4E::Sprite> sprite_; // スプライト
	std::unique_ptr<K4E::SkyBox> skyBox_; // スカイボックス

	bool isDebugCamera_ = false; // デバッグカメラ使用フラグ

	std::unique_ptr<FadeManager> fadeManager_; // フェードマネージャー

	int prevW_ = 0;
	int prevH_ = 0;

	// --- ひび割れ＆分解デモ用 ---
	std::unique_ptr<K4E::Sprite> crackDemoSprite_;
	std::unique_ptr<K4E::SpriteFractureEffect> fracture_;

	bool fractureActive_ = false;
	float fractureProgress_ = 0.0f;

	// ImGuiで触る値（Updateでも使うのでメンバ化）
	bool crackEnable_ = true;
	float crackProgress_ = 0.0f;
	float crackScale_ = 18.0f;
	float crackThickness_ = 0.03f;
	float crackIntensity_ = 1.0f;
	K4E::Vector2 hitUV_ = { 0.5f, 0.5f };


	std::unique_ptr<K4E::Sprite> blockSprite_;        // 下の絵（タイル/ブロック）
	std::unique_ptr<K4E::Sprite> crackOverlaySprite_; // 上のひび割れ

	// 0..1 の進行度
	float breakProgress_ = 0.0f;

	// ひび割れアニメ（10段階）
	static constexpr int kCrackFrames = 10;

	// CrackAtlas の1コマのピクセルサイズ（あなたの作った画像に合わせる）
	K4E::Vector2 crackFrameSizePx_ = { 128.0f, 128.0f }; // 例：128x128/フレーム

	bool atlasAuto_ = true;
	float atlasFps_ = 12.0f;
	float atlasTime_ = 0.0f;
	bool atlasHideAtZero_ = false; // テスト中はfalse推奨（0でも表示）

	std::unique_ptr<K4E::SpriteDebrisEmitter> debris_;
	int prevCrackStage_ = 0;

	// テスト用：欠片ON/OFF
	bool debrisEnable_ = true;
	int debrisBurstBase_ = 10;
};

