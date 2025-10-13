#pragma once
#include "IStageSelector.h"
#include <Sprite.h>
#include <Vector2.h>

#include <optional>
#include <memory>

/// -------------------------------------------------------------
///				　	グリッド型ステージセレクター
/// -------------------------------------------------------------
class GridStageSelector : public IStageSelector
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~GridStageSelector() = default;

	// 初期化処理 : 依存注入：コンテキストを受け取る
	void Initialize(const SelectorContext& context) override;

	// 更新処理 : ⊿タイム付き
	void Update(float deltaTime) override;

	// 3Dオブジェクトの描画
	void Draw3DObjects() override;

	// 2Dオブジェクトの描画
	void Draw2DSprites() override;

	// モード切替のフック
	void OnEnter() override;

	// モード切替のフック
	void OnExit() override;

	// 外部からの指示用
	void FocusToIndex(int index, bool tween = true) override; // 指定インデックスへフォーカス

	// 中央カード変更時コールバック登録
	void SetOnCenterChanged(std::function<void(uint32_t)> callback) { onCenterChanged_ = callback; }

private: /// ---------- メンバ関数 ---------- ///

	// 押下
	void UpdatePress(Input* input, Vector2& mp);

	// ホイール更新
	void UpdateWheel(Vector2& mouse);

	// ドラッグ更新
	void UpdateDrassing(Input* input, Vector2& mp, float deltaTime);

	// 離し
	void UpdateRelease(Input* input, Vector2& mp);

	// クリックTween更新
	void UpdateTween(float deltaTime);

	// 慣性更新
	void UpdateInertia(float deltaTime);

	// レイアウト更新
	void UpdateLayout();

private: /// ---------- メンバ関数 ---------- ///

	// ユーティリティ
	int HitTestCardIndex(const Vector2& mousePosition) const; // -1 ならヒットなし
	void StartTweenToIndex(int index, float duration = 0.3f); // 指定インデックスへトゥイーン開始
	void CancelTween() { tweenActive_ = false; tweenTimer_ = 0.0f; } // トゥイーンキャンセル

	int GetSelectedIndex(Vector2& mousePosition) const;

	int GetCenterIndex() const;   // 中央に最も近いインデックス

	void  TriggerLockedShake();     // 鍵クリック時に呼ぶ
	void  UpdateShake(float dt);    // Update から呼ぶ
	float GetShakeOffsetX() const;  // 現在のオフセット(px)

private: /// ---------- メンバ変数 ---------- ///

	// コンテキスト
	SelectorContext context_{};
	const std::vector<StageInfo>* stages_ = nullptr; // ステージ情報リスト（外部管理）

	// サムネイルグリッド設定
	std::vector<std::unique_ptr<Sprite>> thumbs_; // ステージサムネイルスプライト
	std::unique_ptr<Sprite> selShadow_; // 選択枠影

	// ロックアイコン
	std::vector<std::unique_ptr<Sprite>> lockIcons_;
	float lockScale_ = 0.5f;
	Vector2 lockOffset_ = { 0.0f, 0.0f };

	std::function<void(uint32_t)> onCenterChanged_; // 中央カード変更時コールバック
	int prevCenterIndex_ = -1; // 前回の中央インデックス

	// レイアウト・見た目
	Vector2 center_ = { 640.0f, 360.0f }; // グリッド中心位置
	float gapX_ = 360.0f; // 横間隔
	float gapY_ = 220.0f; // 縦間隔
	float baseW_ = 300.0f; // カード基本幅
	float baseH_ = 180.0f; // カード基本高
	float focusScale_ = 0.08f; // フォーカス時の拡大率

	// スクロール・ドラッグ
	float scrollX_ = 0.0f;      // スクロール位置
	float velocityX_ = 0.0f;    // スクロール速度
	bool dragging_ = false;   // ドラッグ中かどうか
	Vector2 lastMouse_{};
	Vector2 dragStart_{};
	float clickDeltaAccum_ = 0.0f; // クリックとドラッグの判定用
	std::optional<int> pressIndex_; // 押下開始時のインデックス（-1ならカード外、nulloptならカード外で押している）
	bool clickStartedOnCard_ = false; // 押下開始がカード上かどうか
	float lastDxPerSec_ = 0.0f; // 直近フレームのマウス移動速度（慣性用）
	float friction_ = 0.92f; // 慣性摩擦係数
	bool loop_ = true;

	// チューニング
	float maxVel_ = 2000.0f;        // 慣性の最大速度
	float maxDxPerFrame_ = 100.0f;   // 1フレームあたりの最大移動量（ドラッグの暴発防止）
	float overdragFactor_ = 0.5f;   // 端でのオーバードラッグ抑制係数
	float springK_ = 800.0f;         // ばね定数（慣性減衰）
	float snapK_ = 8.0f;            // スナップ係数（手動スクロール時に近くのカードへ吸着する）

	// クリック時の時間ベース移動
	bool tweenActive_ = false;   // トゥイーン中かどうか
	float tweenStartX_ = 0.0f;  // トゥイーン開始時のスクロール位置
	float tweenTargetX_ = 0.0f; // トゥイーン目標位置
	float tweenTimer_ = 0.0f;  // トゥイーン経過時間
	float tweenDuration_ = 0.3f; // トゥイーン総時間

	bool  shakeActive_ = false;
	float shakeTimer_ = 0.0f;
	float shakeDuration_ = 0.28f;   // ブレ時間
	float shakeAmpPx_ = 18.0f;   // 振幅(px)
	float shakeFreqHz_ = 28.0f;   // 周波数(Hz)
};

