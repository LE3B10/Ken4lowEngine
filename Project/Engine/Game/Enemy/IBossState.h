#pragma once

/// -------------------------------------------------------------
///			ボスキャラクターの行動状態を示す抽象クラス
/// -------------------------------------------------------------
class IBossState
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// デストラクタ
	virtual ~IBossState() = default;

	// 状態に入るときの初期化処理
	virtual void Initialize() = 0;

	// 更新処理
	virtual void Update() = 0;

	// 描画処理
	virtual void Draw() = 0;

	// 終了処理
	virtual void Exit() = 0;

};

