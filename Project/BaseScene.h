#pragma once

/// -------------------------------------------------------------
///					　	シーンの基底クラス
/// -------------------------------------------------------------
class BaseScene
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// 仮想デストラクタ
	virtual ~BaseScene() = default;

	// 仮想初期化処理
	virtual void Initialize() = 0;

	// 仮想更新処理
	virtual void Update() = 0;
	
	// 仮想描画処理
	virtual void Draw() = 0;
	
	// 仮想終了処理
	virtual void Finalize() = 0;

	virtual void DrawImGui() = 0;
};

