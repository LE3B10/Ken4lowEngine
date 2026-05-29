#pragma once

/// ---------- 前方宣言 ---------- ///
class BossBase;

/// ---------------------------------------------------------------
///					攻撃の共通インターフェース
/// ---------------------------------------------------------------
class IBossAttack
{
public: /// ---------- 初期化 ---------- ///

	// デストラクタは仮想関数にしておく
	virtual ~IBossAttack() = default;


	// 攻撃所有者を受け取って初期化
	virtual void Initialize(BossBase* owner) = 0;

public: /// ---------- 攻撃開始 / 更新 / 終了 ---------- ///

	// 攻撃開始
	virtual void Start() = 0;

	// 攻撃更新
	virtual void Update(float deltaTime) = 0;

	// 攻撃終了
	virtual void End() = 0;

public: /// ---------- 判定系 ---------- ///

	// 今この攻撃を開始できるか
	virtual bool CanStart() const = 0;

	// 攻撃が終了したか
	virtual bool IsFinished() const = 0;

	// 現在実行中か
	virtual bool IsActive() const = 0;

public: /// ---------- クールダウン系 ---------- ///

	// クールダウン更新
	virtual void TickCooldown(float deltaTime) = 0;

	// 残りクールダウン時間
	virtual float GetCooldownRemaining() const = 0;

public: /// ---------- 参照用情報 ---------- ///

	// 攻撃名
	virtual const char* GetName() const = 0;

	// 攻撃の優先度
	virtual int GetPriority() const = 0;

	// 有効距離の最小
	virtual float GetMinRange() const = 0;

	// 有効距離の最大
	virtual float GetMaxRange() const = 0;

public: /// ---------- 描画 ---------- ///

	// 攻撃固有の描画
	virtual void Draw() = 0;

	// シャドウ描画
	virtual void DrawShadow() {}

	// デバッグ描画 / ImGui
	virtual void DrawImGui() {}
};