#pragma once
#include "Sprite.h"

#include <memory>
#include <string>
#include <vector>

/// -------------------------------------------------------------
///					　	フェード管理クラス
/// -------------------------------------------------------------
class FadeManager
{
private: /// ---------- 列挙型 ---------- ///

	// 状態列挙型
	enum class State
	{
		None,
		TileCover,   // タイルが閉じる（覆う）
		Hold,        // ホールド（完全に覆われた状態を維持）
		TileUncover  // タイルが開く（戻る）
	};

private: /// ---------- 構造体 ---------- ///

	struct Tile
	{
		Vector2 center;                 // タイル中心座標（Sprite座標系）
		float delay = 0.0f;             // このタイルが開始する遅延（秒）
		std::unique_ptr<Sprite> sp;     // ★タイルごとにSpriteを持つ
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	bool Update(float dt);

	// 2Dオブジェクトの描画
	void Draw2DSprites();

	// 終了処理
	void Finalize();

	// 開き始める
	void StartUncover();

	// キャンセル
	void Cancel();

public: /// ---------- アクセサ関数 ---------- ///

	bool IsTransitioning() const { return state_ != State::None; }
	bool IsCovering() const { return state_ == State::TileCover; }
	bool IsHolding() const { return state_ == State::Hold; }
	State GetState() const { return state_; }

	// Hold の最低条件が満たされたか（Scene側のロード完了判定と組み合わせる）
	bool IsHoldMinSatisfied() const;

	/// 追加チューニング（必要なら）
	void SetTileSizePx(float px) { tileSizePx_ = px; InvalidateCache(); }
	void SetTileAnimSec(float sec) { tileAnimSec_ = sec; }
	void SetTileStaggerSec(float sec) { tileStaggerSec_ = sec; }

	// 覆い始める（ホールド指定つき）
	void StartCover(float holdSec, int holdFrames = 0);

	// Scene更新を止めるべきか（Cover中＋Hold中）
	bool IsBlockingSceneUpdate() const { return state_ == State::TileCover || state_ == State::Hold; }

private: /// ---------- ヘルパ関数 ---------- ///

	// タイル群の準備
	void EnsureTiles();

	// タイル群の描画
	void DrawTileOverlay();

	// 画面サイズ変化時のキャッシュ無効化
	void InvalidateCache() { cachedW_ = cachedH_ = -1.0f; }

private: /// ---------- メンバ変数 ---------- ///

	State state_ = State::None;
	float timer_ = 0.0f;

	// ---- Hold (最低保持) ----
	float minHoldSec_ = 0.0f;
	int minHoldFrames_ = 0;
	float holdTimer_ = 0.0f;
	int holdFramesLeft_ = 0;

	// ---- タイル遷移パラメータ ----
	float tileSizePx_ = 64.0f;          // タイル1枚のサイズ
	float tileAnimSec_ = 0.08f;         // 1枚が 0→100% になる時間（短いほどキビキビ）
	float tileStaggerSec_ = 0.0015f;    // 並べる速度（大きいほど遅い）
	float tileMaxDelay_ = 0.0f;

	int tilesX_ = 0;
	int tilesY_ = 0;
	float cachedW_ = -1.0f;
	float cachedH_ = -1.0f;

	std::vector<Tile> tiles_;
};

