#pragma once
#include "AnimationModel.h"
#include "Collider.h"

#include <unordered_map> // 忘れずに！

/// ---------- 前方宣言 ---------- ///
class Player;
class CollisionManager;

enum class BossState
{
	Idle,		   // 待機
	Chase,		   // チェイス
	Melee,		   // 近接攻撃
	Shoot,		   // 射撃
	SpecialAttack, // スペシャル攻撃
	Dead,		   // 死亡
};


/// -------------------------------------------------------------
///				　			ボスキャラクター
/// -------------------------------------------------------------
class Boss : public Collider
{
private: /// ---------- 構造体 ---------- ///

	// ★各部位ごとの Collider を保持
	struct PartCol
	{
		std::string name;
		std::unique_ptr<Collider> col;
	};
	std::vector<PartCol> bodyCols_;

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGuiを描画する処理
	void DrawImGui();

	// すべての部位コライダーを CollisionManager へ登録
	void RegisterColliders(CollisionManager* collisionManager) const;

	// 衝突処理
	void OnCollision(Collider* other) override;

public:	/// ---------- アクセッサ ---------- ///

	void SetPosition(const Vector3& position) { model_->SetTranslate(position); }
	void SetRotation(const Vector3& rotate) { model_->SetRotate(rotate); }

	bool IsDead() const { return isDead_; }
	float GetHP() const { return hp_; }
	void TakeDamage(float damage);

	void ChangeState(BossState newState);

	// プレイヤーをセット
	void SetPlayer(Player* player) { player_ = player; }

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr;
	BossState state_ = BossState::Idle;
	AnimationModel* model_ = nullptr;
	//std::unique_ptr<AnimationModel> model_;
	// ステートに対応するモデル（もしくはアニメーション）
	std::unordered_map<BossState, std::string> stateModelFiles_;
	std::unordered_map<BossState, std::unique_ptr<AnimationModel>> models_;

	float hp_ = 2000.0f;
	float maxHP_ = 2000.0f;
	bool isDead_ = false;
	float scaleFactor_ = 2.0f;

	bool didShoot_ = false;
	bool didMelee_ = false;

	float meleeCooldown_ = 0.0f;
	float deltaTime_ = 1.0f / 60.0f;

	float shootDuration_ = 0.0f;
	const float shootMaxDuration_ = 1.0f;

	float shootCooldown_ = 0.0f;
	const float shootCooldownMax_ = 2.0f; // 2秒間は再発射禁止

	float meleeDuration_ = 0.0f;
	const float meleeMaxDuration_ = 1.0f;

	bool isDying_ = false;   // 死亡演出中フラグ
	float deathTime_ = 0.0f;   // 経過時間
	const float kDeathDuration_ = 3.8f;   // 演出時間（秒）

	bool isDissolving_ = false;
	float dissolveTime_ = 0.0f;
	float dissolveDuration_ = 2.0f; // ディゾルブ完了までの時間
};

