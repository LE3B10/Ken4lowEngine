#pragma once
#include <Object3D.h>
#include <Collider.h>
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "ContactRecord.h"
#include "Vector4.h"

/// ---------- 前方宣言 ---------- ///
class Player;
class Enemy;

/// -------------------------------------------------------------
///						　ハンマークラス
/// -------------------------------------------------------------
class Hammer final : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	Hammer();

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 接触記録を抹消する処理
	void ClearContactRecord();

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	// 中心座標を取得する純粋仮想関数
	Vector3 GetCenterPosition() const override;

public: /// ---------- ゲッター ---------- ///

	// WorldTransformの取得
	WorldTransform& GetWorldTransform() { return worldTransform_; }

	// 回転を取得
	const Vector3& GetRotation() const { return worldTransform_.rotate_; }

	// 座標を取得
	const Vector3& GetTranslate() const { return worldTransform_.translation_; }

	// シリアルナンバーを取得
	uint32_t GetSerialNumber() { return serialNumber_; }

public: /// ---------- セッター ---------- ///

	// 親Transformを設定
	void SetParentTransform(const WorldTransform* parent) { parentTransform_ = parent; }

	// 回転を設定
	void SetRotation(const Vector3& rotation) { worldTransform_.rotate_ = rotation; }

	// 座標を設定
	void SetTranslate(const Vector3& translate) { worldTransform_.translation_ = translate; }

	// プレイヤーを設定
	void SetPlayer(Player* player) { player_ = player; }

	void SetEnemy(Enemy* enemy) { enemy_ = enemy; }

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr;

	ContactRecord contactRecord_;

	Enemy* enemy_ = nullptr;

	ParticleManager* particleManager_;
	std::unique_ptr<Object3D> object_;
	std::unique_ptr<ParticleEmitter> particleEmitter_;

	WorldTransform worldTransform_;
	const WorldTransform* parentTransform_ = nullptr;

	Vector3 offset_ = { 0.0f, 4.5f,0.0f };
	static inline Vector4 transformedPosition;

	// シリアルナンバー
	uint32_t serialNumber_ = 0;
	// 次のシリアルナンバー
	static inline uint32_t nextSerialNumber_ = 0;
};
