#pragma once
#include "BaseCharacter.h"


/// ---------- 前方宣言 ---------- ///
class Hammer;


/// -------------------------------------------------------------
///							　敵クラス
/// -------------------------------------------------------------
class Enemy : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	Enemy();

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	// 中心座標を取得する純粋仮想関数
	Vector3 GetCenterPosition() const override;

public: /// ---------- セッター ---------- ///

	// 初期角度を設定するセッタ
	void SetInitialAngle(float angle) { angle_ = angle; }

	// 回転の中心座標を設定するセッタ
	void SetCenterPosition(const Vector3& position) { center_ = position; }


public: /// ---------- ゲッター ---------- ///

	// シリアルナンバーを取得
	uint32_t GetSerialNumber() { return serialNumber_; }

private: /// ---------- メンバ関数 ---------- ///

	// 腕のアニメーションを更新する処理
	void UpdateArmAnimation();

private: /// ---------- メンバ変数 ---------- ///

	Hammer* hammer_ = nullptr;

	float angle_ = 0.0f;
	float radius_ = 100.0f;
	float speed_ = 0.000f;

	Vector3 center_ = { 0.0f, 0.0f, 0.0f }; // 回転の中心座標（デフォルトは原点）

	// 腕のアニメーション用の媒介変数
	float armSwingParameter_ = 0.0f;

	// 腕の振りの最大角度（ラジアン）
	const float kMaxArmSwingAngle = 0.3f;

	// 腕の振りの速度
	const float kArmSwingSpeed = 0.1f;

	// シリアルナンバー
	uint32_t serialNumber_ = 0;
	// 次のシリアルナンバー
	static inline uint32_t nextSerialNumber_ = 0;
};

