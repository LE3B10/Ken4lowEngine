#pragma once
#include "IStageSelector.h"
#include <Sprite.h>
#include <Vector2.h>

#include <optional>
#include <memory>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　	グリッド型ステージセレクター
/// -------------------------------------------------------------
class GridStageSelector : public IStageSelector
{
private: /// ---------- 構造体 ---------- ///

	// ロックアイコンセット
	struct LockIconSet
	{
		std::vector<std::unique_ptr<K4E::Sprite>> sprites; // ロックアイコンスプライト群
		float   scale = 0.34f;						  // スケール
		K4E::Vector2 offset = { 0.0f, 0.0f };			  // カード中心からのオフセット
	};

	// レイアウトパラメータ
	struct LayoutParam
	{
		K4E::Vector2 center = { 960.0f, 510.0f }; // グリッド中心位置
		float gapX = 500.0f;                 // 横間隔
		float baseW = 440.0f;                // カード基本幅
		float baseH = 248.0f;                // カード基本高さ
		float focusScale = 0.24f;            // フォーカス時の拡大率
		float nonSelectedTint = 0.52f;       // 未選択カードの暗さ
		float edgeAlpha = 0.74f;             // 画面端カードの透明度
		float centerYRatio = 0.47f;          // 画面高に対するカード中央位置
	};

	// スクロールドラッグ状態
	struct ScrollDragState
	{
		float scrollX = 0.0f;			 // スクロール位置
		float velocityX = 0.0f;			 // スクロール速度
		bool dragging = false;			 // ドラッグ中かどうか
		K4E::Vector2 lastMouse{};			 // 直近マウス位置
		K4E::Vector2 dragStart{};			 // ドラッグ開始位置
		float clickDeltaAccum = 0.0f;	 // クリックかドラッグかの判定用移動量
		std::optional<int> pressIndex;	 // 押下開始インデックス
		bool clickStartedOnCard = false; // クリックがカード上で始まったかどうか
		float lastDxPerSec = 0.0f;		 // 慣性用の速度
		bool  loop = true;				 // ループの有無
	};

	// チューニングパラメータ
	struct TuningParam
	{
		float maxVel = 2000.0f;		  // 慣性の最大速度
		float maxDxPerFrame = 100.0f; // 1フレームの最大移動量
		float overdragFactor = 0.5f;  // 端でのオーバードラッグ係数
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

	virtual ~GridStageSelector() = default;
	GridStageSelector();
	void Initialize(const SelectorContext& context) override;
	void Update(float deltaTime) override;
	void Draw3DObjects() override;
	void Draw2DSprites() override;
	void OnEnter() override;
	void OnExit() override;
	void FocusToIndex(int index, bool tween = true) override;
	void SetOnCenterChanged(std::function<void(uint32_t)> callback) { onCenterChanged_ = callback; }
	void PlayUnlockAnim(int index);

private: /// ---------- メンバ関数 ---------- ///

	void UpdatePress(K4E::Input* input, K4E::Vector2& mp);
	void UpdateWheel();
	void UpdateDrassing(K4E::Input* input, K4E::Vector2& mp, float deltaTime);
	void UpdateRelease(K4E::Input* input, K4E::Vector2& mp);
	void UpdateTween(float deltaTime);
	void UpdateInertia(float deltaTime);
	void UpdateLayout();
	void CheckCenterCardChanged(float deltaTime);

private: /// ---------- メンバ関数 ---------- ///

	int HitTestCardIndex(const K4E::Vector2& mousePosition) const;
	float GetWrappedScrollX(int cardCount) const;
	float GetCardOffsetX(int index, int cardCount, float wrappedScrollX) const;
	void StartTweenToIndex(int index, float duration = 0.3f);
	void CancelTween() { tween_.active = false; tween_.timer = 0.0f; }
	int GetCenterIndex() const;
	void TriggerLockedShake();
	void UpdateShake(float dt);
	float GetShakeOffsetX() const;

private: /// ---------- メンバ変数 ---------- ///

	SelectorContext context_{};
	const std::vector<StageInfo>* stages_ = nullptr;
	std::vector<std::unique_ptr<K4E::Sprite>> thumbs_;
	std::unique_ptr<K4E::Sprite> selShadow_;
	LockIconSet lockUI_{};
	std::function<void(uint32_t)> onCenterChanged_;
	int prevCenterIndex_ = -1;
	LayoutParam layout_{};
	ScrollDragState scroll_{};
	TuningParam tuning_{};
	TweenState tween_{};
	ShakeState shake_{};
	UnlockAnimState unlockAnim_{};
	float selectionPulseTimer_ = 0.0f;
};
