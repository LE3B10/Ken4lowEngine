#pragma once
#include "BaseCharacter.h"
#include "ContactRecord.h"
#include "BossBehaviorTree.h"
#include "IBossAttackState.h"

#include <optional>

/// ---------- 前方宣言 ---------- ///
class Player;
class LevelObjectManager;

/// -------------------------------------------------------------
///					　ボス敵キャラクタークラス
/// -------------------------------------------------------------
class BossEnemy : public BaseCharacter
{
public: /// ---------- 列挙型 ---------- ///

	// ボス敵の状態
	enum class State
	{
		Appear, // 登場演出中
		Battle, // 戦闘中
		Dead,   // 死亡
	};

	// ボスの攻撃種類
	enum class AttackKind
	{
		kNone,  // なし（待機）
		kRush,  // 突進攻撃
		kSpinAttack,    // 近距離回転攻撃


		kSideStepSlash, // 横ステップ → 斬りつけ
		kBackstepRush,  // バックステップ → 高速突進
		kMultiRush,     // 連続ラッシュ（2～3回）
		kJumpSlam,      // 素早く飛び上がっての叩きつけ
		// TODO: kSwipe, kJumpSlam, kShotgun などを追加予定
	};

public: /// ---------- 構造体 ---------- ///

	// 登場演出用状態構造体
	struct AppearState
	{
		bool  active = true;        // 登場演出中か
		bool  finished = false;     // 登場演出が完了したか
		float timer = 0.0f;         // 経過時間
		float duration = 2.0f;      // 演出時間（秒）
		Vector3 startPosition;      // 開始位置
		Vector3 endPosition;        // 終了位置
	};

	// フラッシュ情報の構造体
	struct FlashInfo
	{
		float duration = 0.08f;   // フラッシュ継続秒
		float timer = 0.0f;    // 残りタイム
		Vector4 baseColor = { 1.0f,1.0f,1.0f,1.0f }; // 元の色
		Vector4 hitColor = { 1.0f,0.0f,0.0f,1.0f };  // ヒット時の色
		Vector4 colorModulate = { 1.0f,1.0f,1.0f,1.0f }; // 現在の色補正
	};

	// 戦闘スキル情報
	struct AttackPattern
	{
		AttackKind kind = AttackKind::kNone;       // スキルの種類
		float      elapsed = 0.0f;               // 発動からの経過時間
		float      duration = 0.0f;               // スキル全体の長さ
		Vector3    moveDirection{ 0.0f,0.0f,0.0f }; // 移動方向（Rush 用）

		int   phase = 0;        // 0:サイドステップ, 1:斬りつけ
		float phaseTime = 0.0f;

		int comboIndex = 0; // 今が何段目か
		int comboMax = 0; // 何段コンボか

		float jumpHeight = 0.0f; // 最大高さ

		// 回転攻撃用：開始時のヨー角（体の向き）
		float startYaw = 0.0f;
	};

public: /// ---------- 分解用構造体 ---------- ///

	// 分解運動データ構造体
	struct GibMotion
	{
		Vector3 velocity;		 // 初速度
		Vector3 angularVelocity; // 角速度（ラジアン）
	};

	// 死亡演出状態構造体
	struct DeathEnemyState
	{
		bool  active = false;		 // 死亡演出中
		bool  finished = false;		 // 完了（クリア判定用）
		float timer = 0.0f;			 // 経過時間
		float duration = 1.2f;		 // 分解が終わるまでの時間
		std::vector<GibMotion> gibs; // 各部位の分解運動データ
		GibMotion bodyGib;			 // 体幹部位の分解運動データ
	};

public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~BossEnemy() = default;

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update(float deltaTime) override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui() override;

	// 衝突判定を行う
	void OnCollision(Collider* other) override;

	// 中心座標を取得
	Vector3 GetCenterPosition() const override;

private: /// ---------- メンバ関数 ---------- ///

	// ステージと衝突判定
	void SolveWorldCollision(const Vector3& oldTranslate);

	// ダメージフラッシュ更新
	void UpdateDamageFlash(float deltaTime);

	// 全部位に色を適用
	void ApplyColorToAll(const Vector4& color);

	// 死亡演出更新
	void UpdateDeath(float deltaTime);

public: /// ---------- メンバ関数 ---------- ///

	// 移動方向に体の向きを合わせる
	void UpdateFacingDirection(const Vector3& moveDir, float deltaTime);

	// 次の攻撃種類を選択
	AttackKind DecideNextAttackKind();

public: /// ---------- アクセッサ ---------- ///

	// 死亡状態かどうか取得
	bool IsDeadNow() const { return death_.finished; }

	Player* GetPlayer() const { return player_; }
	void SetPlayer(Player* player) { player_ = player; }

	// レベルマネージャー設定
	void SetLevelObjectManager(LevelObjectManager* levelObjectManager) { levelObjectManager_ = levelObjectManager; }

	// 位置の取得・設定（body の位置）
	Vector3 GetPosition() const { return body_.transform.translate_; }
	void SetPosition(const Vector3& pos) { body_.transform.translate_ = pos; }

	// Y回転の取得・設定
	float GetYaw() const { return body_.transform.rotate_.y; }
	void SetYaw(float yaw) { body_.transform.rotate_.y = yaw; }

	// 攻撃クールタイム
	float GetAttackCooldown() const { return attackCooldown_; }
	void SetAttackCooldown(float v) { attackCooldown_ = v; }

	// 「プレイヤーから離れている時間」
	float GetFarFromPlayerTimer() const { return farFromPlayerTimer_; }
	void SetFarFromPlayerTimer(float v) { farFromPlayerTimer_ = v; }

	// ダメージを受け取る
	void ChangeAttackState(AttackKind nextKind);

public: /// ---------- ビヘイビアツリー用 ---------- ///

	// HP 0 か？（死亡判定用）
	bool IsDead() const;

	// 死亡ステートへの遷移要求（BT から呼ばれる）
	void RequestDeadState();

	// 登場演出が終わったか？
	bool IsAppearFinished() const;

	// 登場演出の更新（BT のアクションから呼ばれる）
	void UpdateAppear(float deltaTime);

	// HP 割合（0.0～1.0）
	float GetHPRate() const;

	// 通常フェーズ更新（Phase1）
	BehaviorStatus UpdateNormalPhase(float deltaTime);

	// 激怒フェーズ更新（Phase2）
	BehaviorStatus UpdateRagePhase(float deltaTime);

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr;                     // プレイヤーへの参照
	LevelObjectManager* levelObjectManager_ = nullptr; // レベルマネージャーへの参照

	State state_ = State::Appear; // 現在の状態

	FlashInfo flashInfo_;	 // ダメージフラッシュ情報
	DeathEnemyState death_;  // 死亡演出状態
	AppearState     appear_; // 登場演出状態

	ContactRecord contactRecord_; // 接触記録

	// 攻撃状態
	std::unique_ptr<IBossAttackState> attackState_;
	AttackKind currentAttackKind_ = AttackKind::kNone;

	// HP
	float maxHP_ = 2000.0f;
	float currentHP_ = maxHP_;

	float attackPower_ = 20.0f; // ボスがプレイヤーに与えるダメージ量（仮）

	// 現在発動中のスキル（なければ std::nullopt）
	std::optional<AttackPattern> currentAttack_;

	// 次のスキルまでのクールタイム
	float attackCooldown_ = 0.0f;

	// プレイヤーから離れた状態が続いている時間
	float farFromPlayerTimer_ = 0.0f;

	// ビヘイビアツリー
	std::unique_ptr<BossBehaviorTree> behaviorTree_;

	// スキンテクスチャのパス
	const std::string skinTexturePath_ = "zombie.png";
};

