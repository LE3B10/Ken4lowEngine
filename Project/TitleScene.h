#pragma once
#include <BaseScene.h>


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

	// 描画処理
	void Draw() override;

	// 終了処理
	void Finalize() override;

private: /// ---------- メンバ変数 ---------- ///



};

