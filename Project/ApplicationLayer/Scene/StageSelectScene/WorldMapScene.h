#pragma once
#include <BaseScene.h>
#include "IStageSelector.h"
#include "WorldMapStageSelector.h"  // 既に作成済みのセレクタ
#include "FadeController.h"

#include <memory>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;

/// -------------------------------------------------------------
///				　		ワールドマップシーン
/// -------------------------------------------------------------
class WorldMapScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~WorldMapScene() = default;

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

	std::vector<StageInfo> stages_;
	std::unique_ptr<FadeController> fade_;
	SelectorContext ctx_;

	std::unique_ptr<IStageSelector> selector_; // WorldMapStageSelector
	IStageSelector* active_ = nullptr;
};

