#pragma once
#include "BaseScene.h"
#include <Sprite.h>
#include <FadeController.h>

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <limits>

#include "IStageSelector.h"        // StageInfo / SelectorContext / IStageSelector
#include "GridStageSelector.h"     // まずは Grid を使う

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;


/// -------------------------------------------------------------
///				　	ステージセレクトシーン
/// -------------------------------------------------------------
class StageSelectScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~StageSelectScene() = default;

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

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Input* input_ = nullptr;

	// データ
	std::vector<StageInfo> stages_;

	// フェード用
	std::unique_ptr<FadeController> fadeController_ = nullptr;

	// セレクタ
	SelectorContext context_{};
	std::unique_ptr<IStageSelector> gridSelector_ = nullptr;
	IStageSelector* activeSelector_ = nullptr; // 生ポインタでアクセス

	// 背景色
	std::unique_ptr<Sprite> bg_;
	Vector4 bgNow_ = { 0.10f, 0.10f, 0.10f, 1.0f };
	Vector4 bgTarget_ = bgNow_;
};

