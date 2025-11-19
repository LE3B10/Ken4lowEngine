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
private: /// ---------- 構造体 ---------- ///

	// ロックアイコンセット
	struct LockIconSet
	{
		std::vector<std::unique_ptr<Sprite>> sprites; // ロックアイコンスプライト群
		float   scale = 0.5f;						  // スケール
		Vector2 offset = { 0.0f, 0.0f };			  // カード中心からのオフセット
	};

	// レイアウトパラメータ
	struct LayoutParam
	{
		Vector2 center = { 640.0f, 360.0f }; // グリッド中心位置
		float gapX = 360.0f;				 // 横間隔
		float gapY = 220.0f;                 // （今後縦グリッドにするとき用）
		float baseW = 300.0f;                // カード基本幅
		float baseH = 180.0f;                // カード基本高さ
		float focusScale = 0.08f;            // フォーカス時の拡大率
	};

	// スクロールドラッグ状態
	struct ScrollDragState
	{
		float scrollX = 0.0f;			 // スクロール位置
		float velocityX = 0.0f;			 // スクロール速度
		bool dragging = false;			 // ドラッグ中かどうか
		Vector2 lastMouse{};			 // 直近マウス位置
		Vector2 dragStart{};			 // ドラッグ開始位置
		float clickDeltaAccum = 0.0f;	 // クリックかドラッグかの判定用移動量
		std::optional<int> pressIndex;	 // 押下開始インデックス
		bool clickStartedOnCard = false; // クリックがカード上で始まったかどうか
		float lastDxPerSec = 0.0f;		 // 慣性用の速度
		float friction = 0.92f;			 // 今は未使用だが慣性調整用
		bool  loop = true;				 // ループの有無
	};

	// チューニングパラメータ
	struct TuningParam
	{
		float maxVel = 2000.0f;		  // 慣性の最大速度
		float maxDxPerFrame = 100.0f; // 1フレームの最大移動量
		float overdragFactor = 0.5f;  // 端でのオーバードラッグ係数
		float springK = 800.0f;		  // ばね定数（今は未使用）
		float snapK = 8.0f;			  // 中心カードへの吸着強さ
	};

	// トゥイーン状態
	struct TweenState
	{
		bool  active = false;  // トゥイーン中かどうか
		float startX = 0.0f;   // トゥイーン開始位置
		float targetX = 0.0f;  // トゥイーン目標位置
		float timer = 0.0f;    // トゥイーン経過時間
		float duration = 0.3f; // トゥイーン所要時間
	};

	// 画面振動状態
	struct ShakeState
	{
		bool active = false;	// シェイク中かどうか
		float timer = 0.0f;		// 経過時間
		float duration = 0.28f;	// 持続時間
		float ampPx = 18.0f;	// 振幅(px)
		float freqHz = 28.0f;	// 周波数(Hz)
	};

	// アンロックアニメーション状態
	struct UnlockAnimState
	{
		std::vector<float> timers; // 各カードの残り時間
		float duration = 0.6f;	   // アニメーション所要時間
	};

public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~GridStageSelector() = default;

	GridStageSelector();

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

	// アンロックアニメーション再生
	void PlayUnlockAnim(int index);

private: /// ---------- メンバ関数 ---------- ///

	// 押下
	void UpdatePress(Input* input, Vector2& mp);

	// ホイール更新
	void UpdateWheel();

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

	// 中央カード変更チェック
	void CheckCenterCardChanged(float deltaTime);

private: /// ---------- メンバ関数 ---------- ///

	// ユーティリティ
	int HitTestCardIndex(const Vector2& mousePosition) const; // -1 ならヒットなし
	void StartTweenToIndex(int index, float duration = 0.3f); // 指定インデックスへトゥイーン開始
	void CancelTween() { tween_.active = false; tween_.timer = 0.0f; } // トゥイーンキャンセル

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
	std::unique_ptr<Sprite> selShadow_;           // 選択枠影

	// ロックアイコン＆UI
	LockIconSet lockUI_{};

	// 中央カード変更コールバック
	std::function<void(uint32_t)> onCenterChanged_;
	int prevCenterIndex_ = -1;

	// レイアウト・見た目
	LayoutParam layout_{};

	// スクロール・ドラッグ状態
	ScrollDragState scroll_{};

	// 各種チューニング値
	TuningParam tuning_{};

	// クリック時のトゥイーン状態
	TweenState tween_{};

	// ロック時のシェイク状態
	ShakeState shake_{};

	// アンロック演出状態
	UnlockAnimState unlockAnim_{};
};

