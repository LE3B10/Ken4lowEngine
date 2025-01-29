#pragma once
#include <array>
#include "Object3D.h"
#include "Input.h"
#include "Collider.h"
#include "ContactRecord.h"

/// ---------- 前方宣言 ---------- ///
class Object3DCommon;


/// -------------------------------------------------------------
///                     プレイヤーの挙動
/// -------------------------------------------------------------
class Player : public Collider
{
public: /// ---------- 構造体 ---------- ///

	// レーン管理用構造体
	struct LaneInfo
	{
		std::array<float, 3> positions = { -3.0f, 0.0f, 3.0f }; // レーンのX座標
		int currentIndex = 1; // 現在のレーン（中央）
		float moveSpeed = 0.1f; // 線形補間のスピード
	};

	// ジャンプ処理用構造体
	struct JumpInfo
	{
		bool isJumping = false; // ジャンプ中フラグ
		float velocity = 0.25f;  // ジャンプ初速度
		float gravity = -0.01f; // 重力加速度
		float height = 0.0f;    // ジャンプのY座標オフセット
	};

	// 回転処理用構造体
	struct RotationInfo
	{
		bool isRotating = false; // 回転中フラグ
		float angle = 0.0f;      // 現在の回転角度
		float speed = 10.0f;     // 回転速度 (度/フレーム)
		int count = 0;           // 回転カウント
	};

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	Player();

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 接触記録を削除
	void ClearContactRecord();

public: /// ---------- オーバーライド関数 ---------- ///

	// 衝突時に呼ばれる関数
	void OnCollision([[maybe_unused]] Collider* other) override;

	// 中心座標を取得
	Vector3 GetCenterPosition() const override { return worldTransform_.translate; }

	// サイズを取得
	Vector3 GetSize() const override { return worldTransform_.scale; }

	/// <summary>
	/// 向き（Orientation）を取得
	/// </summary>
	/// <param name="index">番号 0: X軸, 1: Y軸, 2: Z軸 </param>
	/// <returns></returns>
	Vector3 GetOrientation(int index) const override;

public: /// ---------- セッタ ---------- ///

	void SetLanePositions(const std::array<float, 3>& lanePositions) { laneInfo_.positions = lanePositions; }
	void SetCamera(Camera* camera) { camera_ = camera; }

public: /// ---------- ゲッタ ---------- ///

	int GetLaneindex() const { return laneInfo_.currentIndex; } // 現在のレーンを取得
	WorldTransform GetTransform() const { return worldTransform_; }

	// シリアルナンバーのゲッタ
	uint32_t GetSerialNumber() const { return serialNumber_; }

private: /// ---------- メンバ変数 ---------- ///

	Camera* camera_ = nullptr; // カメラ参照
	Input* input = nullptr;

	WorldTransform worldTransform_; // 位置情報
	ContactRecord contactRecord_;

	std::unique_ptr<Object3D> playerObject_; // プレイヤーの3Dオブジェクト

	LaneInfo laneInfo_; // レーン管理
	JumpInfo jumpInfo_; // ジャンプ処理
	RotationInfo rotationInfo_; // 回転処理

	// シリアルナンバー
	uint32_t serialNumber_ = 0;
	// 次のシリアルナンバー
	static inline uint32_t nextSerialNumber_ = 0;
};
