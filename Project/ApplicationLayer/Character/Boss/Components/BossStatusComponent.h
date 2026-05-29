#pragma once
#include <algorithm>

/// -------------------------------------------------------------
///					ボス用ステータスコンポーネント
/// -------------------------------------------------------------
class BossStatusComponent
{
public: /// ---------- 初期化 ---------- ///

	/// <summary>
	/// ステータスを初期化する
	/// </summary>
	/// <param name="maxHP">最大HP</param>
	void Initialize(float maxHP);

	/// <summary>
	/// 終了処理
	/// 今は特別な後始末はないが、将来拡張用に用意しておく
	/// </summary>
	void Finalize() {}

	/// <summary>
	/// 毎フレーム更新
	/// 将来的に「一定時間だけ無敵」や「ダウン耐性回復」などを足せる
	/// </summary>
	void Update(float deltaTime);

public: /// ---------- HP操作 ---------- ///

	/// <summary>
	/// ダメージ適用
	/// 無敵中ならダメージを受けない
	/// HP は 0 未満にならないように丸める
	/// </summary>
	/// <param name="damage">ダメージ量</param>
	void ApplyDamage(float damage);

	/// <summary>
	/// HP回復
	/// 最大HPを超えないように丸める
	/// </summary>
	/// <param name="value">回復量</param>
	void Heal(float value);

	/// <summary>
	/// HPを最大まで全回復する
	/// </summary>
	void FullRecover();

	/// <summary>
	/// 最大HPを変更する
	/// 既存HPが新しい最大HPを超えていたら切り詰める
	/// </summary>
	void SetMaxHP(float maxHP);

	/// <summary>
	/// 現在HPを直接セットする
	/// 0～maxHPの範囲に収める
	/// </summary>
	void SetHP(float hp);

public: /// ---------- 無敵状態 ---------- ///

	/// <summary>
	/// 無敵フラグを設定
	/// </summary>
	void SetInvincible(bool isInvincible);

	/// <summary>
	/// 一定時間の無敵を付与
	/// </summary>
	void SetInvincibleTimer(float timeSec);

	/// <summary>
	/// 無敵か
	/// </summary>
	bool IsInvincible() const { return isInvincible_; }

public: /// ---------- 参照 ---------- ///

	// 現在HP
	float GetHP() const { return hp_; }

	// 最大HP
	float GetMaxHP() const { return maxHP_; }

	/// <summary>
	/// HP割合を返す
	/// maxHP が 0 以下なら 0 を返す
	/// </summary>
	float GetHPRate() const;

	/// <summary>
	/// HPが0以下なら死亡扱い
	/// </summary>
	bool IsDead() const { return hp_ <= 0.0f; }

	/// <summary>
	/// HPが1以上残っていれば生存扱い
	/// </summary>
	bool IsAlive() const { return !IsDead(); }

private: /// ---------- 内部状態 ---------- ///

	// 現在HP
	float hp_ = 0.0f;

	// 最大HP
	float maxHP_ = 0.0f;

	// 常時無敵フラグ
	bool isInvincible_ = false;

	// 一定時間の無敵
	float invincibleTimer_ = 0.0f;
};