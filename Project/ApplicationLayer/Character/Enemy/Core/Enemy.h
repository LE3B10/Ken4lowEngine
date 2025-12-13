#pragma once
#include "BaseCharacter.h"
#include "ContactRecord.h"

#include <IEnemyAIState.h>
#include <EnemyAIWanderState.h>
#include <EnemyConfig.h>
#include <EnemyType.h>
#include <BehaviorTree.h>
#include <BossEnemyVfx.h>

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Player;
class LevelObjectManager;
class GpuParticleManager;

/// -------------------------------------------------------------
///					　敵キャラクタークラス
/// -------------------------------------------------------------
class Enemy : public BaseCharacter
{
public: /// ---------- 列挙型 ---------- ///

	// 敵の状態
	enum class AIState
	{
		Spawn,		// 出現待機
		Idle,		// 待機
		Wander, 	// 徘徊
		Chase,		// 追跡
		Attack,		// 攻撃
		Damaged,	// ダメージ
		Dead		// 死亡
	};

	// Todo: ステートパターンで書き直す予定 : AIStateごとにクラスを分ける

public: /// ---------- 構造体 ---------- ///

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

		bool startBurstDone = false; // 死亡エフェクトの最初の爆発が起きたかどうか
	};

	// 攻撃情報構造体
	struct AttackInfo
	{
		float range = 1.2f;				// これより近いと殴れる
		float power = 0.3f;				// ノックバックの強さ
		float cooldown = 0.8f;			// 次の攻撃までの待ち(秒)
		float cooldownTimer = 0.0f;
		bool  didHitThisAttack = false; // このAttackステート中、もうノックバック済みか？
		float damage = 25.0f;			// 攻撃力

		float windup = 0.18f;			// 溜める(腕を上げる)時間
		float swing = 0.12f;			// 振り下ろし(ヒット発生)時間
		float recover = 0.20f;			// 腕を戻す時間
		float reachMargin = 0.06f;
	};

	// 徘徊情報構造体
	struct WanderInfo
	{
		float idlePoseAngleDeg = -70.0f; // スポーン時にもう腕を前に出してる
		float walkSpeed = 0.03f;		// うろつき時の移動速度
		float changeIntervalMin = 1.5f; // 方向を変える最短間隔(秒)
		float changeIntervalMax = 3.5f; // 方向を変える最長間隔(秒)
		float timer = 0.0f;             // 次に方向を変えるまでの残り時間
		float targetYaw = 0.0f;         // 目指す向き(ラジアン)
		float turnSpeed = 2.5f;         // どれくらいの速さでその向きへ向きなおす(ラジアン毎秒)
		float detectRadius = 10.0f;		// これ以内までプレイヤーが近づいたら襲いかかる
		float stuckThreshold = 0.01f;   // これ未満しか動けてなかったら詰まってるとみなす
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

public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~Enemy();

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update(float deltaTime) override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui() override;

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	// 中心座標を取得する純粋仮想関数
	Vector3 GetCenterPosition() const override;

	// 出現座標をセット（GamePlaySceneとかから呼ぶ）
	void SetSpawnPosition(const Vector3& pos) { body_.transform.translate_ = pos; }

	// プレイヤーのポインタを設定
	void SetPlayerPointer(Player* player) { player_ = player; }

	// レベルオブジェクトマネージャー設定
	void SetLevelObjectManager(LevelObjectManager* mgr) { levelObjectManager_ = mgr; }


	// 死亡状態かどうか取得
	bool IsDead() const { return currentStateId_ == AIState::Dead; }

	// 今まさに死亡演出中かどうか取得
	bool IsDeadNow() const { return death_.finished; }

	/// <summary>
	/// ステージのパラメータを適用
	/// </summary>
	/// <param name="hp">エンティティまたはステージのヒットポイント（HP）。</param>
	/// <param name="walkSpeed">通常移動（歩行）時の移動速度。</param>
	/// <param name="chaseSpeed">ターゲット追跡時の移動速度。</param>
	/// <param name="attackDamage">攻撃1回あたりのダメージ量。</param>
	/// <param name="attackCooldown">攻撃のクールダウン時間（秒などの時間単位）。</param>
	/// <param name="detectRadius">ターゲットを検出する半径（距離）。</param>
	void ApplyStageParams(float hp, float walkSpeed, float chaseSpeed, float attackDamage, float attackCooldown, float detectRadius);

	// 死亡時のドロップ位置を取得
	const Vector3& GetDropPosAtDeath() const { return dropPosAtDeath_; }

public: /// ---------- アクセッサ ---------- ///

	float GetStateTimer() const { return stateTimer_; }
	void  SetStateTimer(float time) { stateTimer_ = time; }
	void  ResetStateTimer() { stateTimer_ = 0.0f; }

	float GetDelayDuration() const { return delayDuration_; }

	float GetRaiseAngleDeg() const { return raiseAngleDeg; }
	float GetHitAngleDeg() const { return hitAngleDeg; }
	float GetReturnAngleDeg() const { return returnAngleDeg; }

	// スポーン済みかどうか取得
	bool IsActive() const { return isActive_; }
	void SetActive(bool active) { isActive_ = active; }

	float GetPersonalSpaceRadius() const { return personalSpaceRadius_ + playerSpaceRadius_; }
	float GetPersonalSpaceRadiusSq() const { float r = GetPersonalSpaceRadius(); return r * r; }
	float GetChaseSpeed() const { return chaseSpeed_; }

	// 敵の種類を取得
	EnemyType& GetEnemyType() { return type_; }

	// 敵の設定を取得
	const EnemyConfig& GetEnemyConfig() const { return config_; }

	// 接触記録を取得
	ContactRecord& GetContactRecord() { return contactRecord_; }

	// 死亡演出状態を取得
	DeathEnemyState& GetDeathState() { return death_; }

	// ドロップ位置を設定
	void SetDropPosAtDeath(const Vector3& pos) { dropPosAtDeath_ = pos; }

	AttackInfo& GetAttackInfo() { return attack_; }
	WanderInfo& GetWanderInfo() { return wander_; }
	FlashInfo& GetFlashInfo() { return flash_; }

	Player* GetPlayer() const { return player_; }

	float GetMaxHp() const { return maxHp_; }
	float GetHp() const { return hp_; }
	void  SetHp(float hp) { hp_ = hp; }

	Vector3& GetPrevPos() { return prevPos_; }
	Vector3& GetDropPosAtDeath() { return dropPosAtDeath_; }

	// レベルマネージャーを取得
	LevelObjectManager* GetLevelObjectManager() const { return levelObjectManager_; }

	BossEnemyVfx* GetVfx() const { return vfx_.get(); }
	void SetVfx(std::unique_ptr<BossEnemyVfx> vfx) { vfx_ = std::move(vfx); }

private: /// ---------- メンバ関数 ---------- ///

	// ワールド衝突解決処理
	void SolveWorldCollision(const Vector3& oldTranslate);

	// ビヘイビアツリー初期化
	void InitializeBehaviorTree();

public: /// ---------- メンバ関数 ---------- ///

	// プレイヤーが攻撃範囲内にいるかどうか
	bool IsPlayerInAttackRange() const;

	// プレイヤーが視認範囲内にいるかどうか
	bool CanSeePlayer() const;

	// プレイヤーに向かって移動する
	void MoveTowardPlayer(float deltaTime);

	// 徘徊行動
	void Wander(float deltaTime);

	// 攻撃行動
	void Attack();

	// 状態変更処理
	void ChangeState(std::unique_ptr<IEnemyAIState> newState);

	void SetState(AIState newStateId) { currentStateId_ = newStateId; }
	AIState& GetCurrentState() { return currentStateId_; }

public: /// ---------- BT から呼びたい「状態リクエスト」関数 ---------- ///

	// 状態リクエスト関数群
	void RequestSpawnState();
	void RequestIdleState();
	void RequestChaseState();
	void RequestAttackState();
	void RequestWanderState();
	void RequestDamagedState();
	void RequestDeadState();

private: /// ---------- メンバ関数 ---------- ///

	Player* player_ = nullptr; // プレイヤーへのポインタ
	LevelObjectManager* levelObjectManager_ = nullptr; // ステージコリジョン用
	GpuParticleManager* gpuParticleManager_ = nullptr; // GPUパーティクルマネージャーへの参照

	AIState currentStateId_ = AIState::Spawn;
	std::unique_ptr<IEnemyAIState> currentState_ = nullptr;

	std::unique_ptr<BehaviorTree> behaviorTree_;

	std::unique_ptr<BossEnemyVfx> vfx_ = nullptr; // エフェクト管理

	EnemyType type_{};    // 種類
	EnemyConfig config_{}; // パラメータ

	// 攻撃
	AttackInfo attack_ = {};

	// 徘徊
	WanderInfo wander_ = {};

	// ヒットフラッシュ
	FlashInfo flash_ = {};

	// 
	DeathEnemyState death_;

	ContactRecord contactRecord_; // 接触記録

	// テクスチャスキンパス
	std::string skinTexturePath_ = "yellow.png";

private: /// ---------- メンバ変数 ---------- ///

	float   stateTimer_ = 0.0f;           // 状態内で経過時間を測る
	bool    isActive_ = false;            // スポーン済みかどうか

private: /// ---------- 調整用パラメータ ---------- ///

	float delayDuration_ = 1.5f;   // スポーン待機時間(秒)
	float walkSpeed_ = 0.03f;  // うろつき時の移動速度
	float chaseSpeed_ = 0.08f;  // 追跡時の移動速度

	// プレイヤーとゼロ距離で重ならないようにする距離管理
	float personalSpaceRadius_ = 0.8f; // 敵の半径
	float playerSpaceRadius_ = 0.8f; // プレイヤー側の半径もだいたい同じ


	float raiseAngleDeg = -120.0f; // 溜めでさらに持ち上げる(もっと前/上)
	float hitAngleDeg = -60.0f; // 振り下ろした瞬間(ちょい下がる)
	float returnAngleDeg = -55.0f; // 回復途中でだんだん下がってくる角度

	Vector3 prevPos_;                      // 1フレーム前の位置(スタック検出用)

	bool hasAggro_ = false;  // 攻撃されて警戒中か？

private: /// ---------- 定数 ---------- ///

	float maxHp_ = 250.0f; // 最大体力
	float hp_ = maxHp_;    // 現在体力

	Vector3 dropPosAtDeath_{};
};

