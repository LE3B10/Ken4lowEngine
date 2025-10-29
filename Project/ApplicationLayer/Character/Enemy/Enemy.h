#pragma once
#include "BaseCharacter.h"
#include "ContactRecord.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Player;
class LevelObjectManager;

/// -------------------------------------------------------------
///					　敵キャラクタークラス
/// -------------------------------------------------------------
class Enemy : public BaseCharacter
{
private: /// ---------- 列挙型 ---------- ///

	// 敵の状態
	enum class AIState
	{
		SpawnDelay, // 出現待機
		Idle,		// 待機
		Wander, 	// 徘徊
		Chase,		// 追跡
		Attack,		// 攻撃
		Damaged,	// ダメージ
		Dead		// 死亡
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
	void SetSpawnPosition(const Vector3& pos) {
		body_.transform.translate_ = pos;
	}

	// プレイヤーのポインタを設定
	void SetPlayerPointer(Player* player) { player_ = player; }

	// レベルオブジェクトマネージャー設定
	void SetLevelObjectManager(LevelObjectManager* mgr) { levelObjectManager_ = mgr; }

	// スポーン済みかどうか取得
	bool IsActive() const { return isActive_; }

private: /// ---------- メンバ関数 ---------- ///

	// 状態遷移処理
	void TransitionState(AIState newState);

	// 各状態の更新処理
	void UpdateSpawnDelay(float deltaTime); // 出現待機
	void UpdateIdle(float deltaTime);		// 待機
	void UpdateWander(float deltaTime);		// 徘徊
	void UpdateChase(float deltaTime);		// 追跡
	void UpdateAttack(float deltaTime);		// 攻撃
	void UpdateDamaged(float deltaTime);	// ダメージ
	void UpdateDead(float deltaTime);		// 死亡

	void SolveWorldCollision(const Vector3& oldTranslate);

	void PickNewWanderDirection();

private: /// ---------- メンバ関数 ---------- ///

	Player* player_ = nullptr; // プレイヤーへのポインタ
	LevelObjectManager* levelObjectManager_ = nullptr; // ステージコリジョン用

	AIState aiState_ = AIState::SpawnDelay; // 敵の現在の状態

	ContactRecord contactRecord_; // 接触記録

	// テクスチャスキンパス
	std::string skinTexturePath_ = "zombie.png";

private: /// ---------- メンバ変数 ---------- ///

	float   stateTimer_ = 0.0f;           // 状態内で経過時間を測る
	bool    isActive_ = false;            // スポーン済みかどうか

private: /// ---------- 調整用パラメータ ---------- ///

	// パラメータ調整しやすいようにまとめておく
	float delayDuration_ = 1.5f;   // スポーン待機時間(秒)
	float walkSpeed_ = 0.03f;  // うろつき時の移動速度
	float chaseSpeed_ = 0.08f;  // 追跡時の移動速度
	float detectRadius_ = 10.0f;  // これ以内までプレイヤーが近づいたら襲いかかる

private: /// ---------- 攻撃判定用定数 ---------- ///

	// 攻撃関連
	float attackRange_ = 1.2f;   // これより近いと殴れる
	float attackPower_ = 0.3f;  // ノックバックの強さ
	float attackCooldown_ = 0.8f;   // 次の攻撃までの待ち(秒)
	float attackCooldownTimer_ = 0.0f;
	bool  didHitThisAttack_ = false; // このAttackステート中、もうノックバック済みか？
	float attackDamage_ = 25.0f;      // 攻撃力

	// プレイヤーとゼロ距離で重ならないようにする距離管理
	float personalSpaceRadius_ = 0.8f; // 敵の半径
	float playerSpaceRadius_ = 0.8f; // プレイヤー側の半径もだいたい同じ

	float attackWindup_ = 0.18f; // 溜める(腕を上げる)時間
	float attackSwing_ = 0.12f; // 振り下ろし(ヒット発生)時間
	float attackRecover_ = 0.20f; // 腕を戻す時間
	float attackReachMargin_ = 0.06f;

	// 腕パーツのインデックス(左腕・右腕)を覚えておく
	uint32_t leftArmIndex_ = 1;
	uint32_t rightArmIndex_ = 2;

	float idlePoseAngleDeg = -70.0f; // スポーン時にもう腕を前に出してる
	float raiseAngleDeg = -120.0f; // 溜めでさらに持ち上げる(もっと前/上)
	float hitAngleDeg = -60.0f; // 振り下ろした瞬間(ちょい下がる)
	float returnAngleDeg = -55.0f; // 回復途中でだんだん下がってくる角度

	// --- 徘徊(Wander)用 ---
	float wanderChangeIntervalMin_ = 1.5f; // 方向を変える最短間隔(秒)
	float wanderChangeIntervalMax_ = 3.5f; // 方向を変える最長間隔(秒)
	float wanderTimer_ = 0.0f;             // 次に方向を変えるまでの残り時間

	float wanderTargetYaw_ = 0.0f;         // 目指す向き(ラジアン)
	float wanderTurnSpeed_ = 2.5f;         // どれくらいの速さでその向きへ向きなおす(ラジアン毎秒)

	Vector3 prevPos_;                      // 1フレーム前の位置(スタック検出用)
	float stuckThreshold_ = 0.01f;         // これ未満しか動けてなかったら詰まってるとみなす
};

