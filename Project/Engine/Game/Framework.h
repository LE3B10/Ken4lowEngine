#pragma once

/// -------------------------------------------------------------
///				　			ゲーム全体
/// -------------------------------------------------------------
class Framework
{
public: /// ---------- メンバ変数 ---------- ///

	// 実行処理
	void Run();

public: /// ---------- 純粋仮想関数 ---------- ///

	// 仮想デストラクタ
	virtual ~Framework() = default;

	// 初期化処理
	virtual void Initialize();

	// 更新処理
	virtual void Update();

	// 描画処理
	virtual void Draw() = 0;

	// 終了処理
	virtual void Finalize();

	// 終了チェック
	virtual bool IsEndRequest() { return endRequest_; }

protected: /// ---------- メンバ変数 ---------- ///

	bool endRequest_ = false;
};

