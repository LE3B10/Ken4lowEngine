#pragma once
#include "IBossAttack.h"

/// ---------- 前方宣言 ---------- ///
class BossBase;

/// ---------------------------------------------------------------
///						ボスの重攻撃パンチ
/// ---------------------------------------------------------------
class BossHeavyPunchAttack : public IBossAttack
{
public: /// ---------- 列挙型 ---------- ///

	/// <summary>
	/// 攻撃の内部フェーズ
	/// </summary>
	enum class Phase
	{
		None,       // 未使用
		Windup,     // 溜め
		Hold,		// 溜め切って一瞬止める
		Active,     // 発生
		Recovery    // 攻撃後の硬直
	};

public: /// ---------- 初期化 ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	~BossHeavyPunchAttack() override = default;

	/// <summary>
	/// 所有者を受け取って初期化
	/// </summary>
	void Initialize(BossBase* owner) override;

public: /// ---------- 攻撃開始 / 更新 / 終了 ---------- ///

	/// <summary>
	/// 攻撃開始
	/// </summary>
	void Start() override;

	/// <summary>
	/// 攻撃更新
	/// </summary>
	void Update(float deltaTime) override;

	/// <summary>
	/// 攻撃終了
	/// </summary>
	void End() override;

public: /// ---------- 判定系 ---------- ///

	/// <summary>
	/// 今この攻撃を開始できるか
	/// </summary>
	bool CanStart() const override;

	/// <summary>
	/// 攻撃終了済みか
	/// </summary>
	bool IsFinished() const override { return isFinished_; }

	/// <summary>
	/// 実行中か
	/// </summary>
	bool IsActive() const override { return isActive_; }

public: /// ---------- クールダウン系 ---------- ///

	/// <summary>
	/// クールダウン更新
	/// </summary>
	void TickCooldown(float deltaTime) override;

	/// <summary>
	/// 残りクールダウン
	/// </summary>
	float GetCooldownRemaining() const override { return cooldownRemaining_; }

public: /// ---------- 参照用情報 ---------- ///

	/// <summary>
	/// 攻撃名
	/// </summary>
	const char* GetName() const override { return "HeavyPunch"; }

	/// <summary>
	/// 攻撃優先度
	/// Punch より高めにしておく
	/// </summary>
	int GetPriority() const override { return priority_; }

	/// <summary>
	/// 最小距離
	/// </summary>
	float GetMinRange() const override { return minRange_; }

	/// <summary>
	/// 最大距離
	/// </summary>
	float GetMaxRange() const override { return maxRange_; }

public: /// ---------- デバッグ参照 ---------- ///

	// 現在のフェーズ
	Phase GetPhase() const { return phase_; }

	// フェーズ内経過時間
	float GetPhaseTimer() const { return phaseTimer_; }

	// ヒット判定をすでに出したか
	bool HasHit() const { return hasHit_; }

	/// <summary>
	/// 攻撃判定リーチを取得
	/// </summary>
	float GetHitRange() const { return hitRange_; }

	/// <summary>
	/// 攻撃判定半径を取得
	/// </summary>
	float GetHitRadius() const { return hitRadius_; }

	/// <summary>
	/// 攻撃判定前方オフセットを取得
	/// </summary>
	float GetHitForwardOffset() const { return hitForwardOffset_; }

public: /// ---------- パラメータ反映 ---------- ///

	/// <summary>
	/// 攻撃開始距離を設定
	/// </summary>
	void SetValidRange(float minRange, float maxRange);

	/// <summary>
	/// 実際の攻撃判定用パラメータを設定
	/// </summary>
	void SetHitParameters(float hitRange, float hitRadius, float hitForwardOffset);

public: /// ---------- 描画 ---------- ///

	// 攻撃固有の描画
	void Draw() override;

	// シャドウ描画は特にない想定
	void DrawShadow() override {}

	// デバッグ用 ImGui 描画
	void DrawImGui() override;

private: /// ---------- 内部処理 ---------- ///

	/// <summary>
	/// 溜め更新
	/// </summary>
	void UpdateWindup(float deltaTime);

	/// <summary>
	/// 溜め切り保持更新
	/// 一瞬だけ予兆ポーズを見せる
	/// </summary>
	void UpdateHold(float deltaTime);

	/// <summary>
	/// 発生更新
	/// </summary>
	void UpdateActive(float deltaTime);

	/// <summary>
	/// 硬直更新
	/// </summary>
	void UpdateRecovery(float deltaTime);

	/// <summary>
	/// フェーズ切り替え
	/// </summary>
	void ChangePhase(Phase newPhase);

	/// <summary>
	/// プレイヤーにヒットするか試す
	/// 発生中に1回だけ呼ぶ
	/// </summary>
	void TryHitPlayer();

	/// <summary>
	/// 攻撃開始可能な距離か
	/// </summary>
	bool IsTargetInValidRange() const;

	/// <summary>
	/// デバッグ表示用フェーズ名
	/// </summary>
	const char* GetPhaseName() const;

private: /// ---------- 参照 ---------- ///

	// 攻撃所有者
	BossBase* owner_ = nullptr;

private: /// ---------- 実行状態 ---------- ///

	bool isActive_ = false;	  // 実行中か
	bool isFinished_ = false; // 今回の実行が終わったか
	bool hasHit_ = false;	  // 発生中にすでにヒット判定を出したか

	Phase phase_ = Phase::None; // 現在フェーズ
	float phaseTimer_ = 0.0f;   // フェーズ内経過時間
	float totalTimer_ = 0.0f;	// 攻撃開始からの合計時間

private: /// ---------- 距離条件 ---------- ///

	// HeavyPunch は少しだけ踏み込みがある想定
	float minRange_ = 0.0f;
	float maxRange_ = 6.50f;

private: /// ---------- フェーズ時間 ---------- ///

	// 通常パンチより重く見せたいので溜めを長くする
	float windupTime_ = 0.55f;

	float holdTime_ = 0.12f;

	// 発生は短め
	float activeTime_ = 0.12f;

	// 硬直は長くして隙を作る
	float recoveryTime_ = 0.80f;

private: /// ---------- ヒット判定 ---------- ///

	float damage_ = 40.0f;              // 通常より高威力
	float hitRange_ = 6.0f;             // ボス正面方向に届く攻撃判定リーチ
	float hitRadius_ = 2.0f;            // 重攻撃判定の半径
	float hitForwardOffset_ = 3.0f;     // ボス中心から前方へ判定開始位置をずらす距離
	float targetRadius_ = 0.65f;        // 仮プレイヤー半径

private: /// ---------- クールダウン ---------- ///

	float cooldownSec_ = 2.20f;		 // 通常パンチより長めのクールダウン
	float cooldownRemaining_ = 0.0f; // クールダウン残り時間

private: /// ---------- 優先度 ---------- ///

	int priority_ = 80; // Punch より高く、他の攻撃よりも優先されるように
};