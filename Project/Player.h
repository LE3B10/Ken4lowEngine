#pragma once
#include <Object3D.h>
#include <array>
#include <Transform.h>

class Object3DCommon;
class Input;

/// -------------------------------------------------------------
///				　		プレイヤーの挙動
/// -------------------------------------------------------------
class Player
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
		float velocity = 0.2f;  // ジャンプ初速度
		float gravity = -0.01f; // 重力加速度
		float height = -1.0f;	// ジャンプのY座標オフセット
	};

	// 回転処理用構造体
	struct RotationInfo
	{
		bool isRotating = false; // 回転中フラグ
		float angle = 0.0f;		 // 現在の回転角度
		float speed = 10.0f;	 // 回転速度 (度/フレーム)
		int count = 0;			 // 回転カウント
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon);

	// 更新処理
	void Update(Input* input);
	
	// 描画処理
	void Draw();

	// セッタ
	void SetLanePositions(const std::array<float, 3>& lanePositions) { laneInfo_.positions = lanePositions; }

	// ゲッタ
	Transform GetTransform() const { return transform_; }

private: /// ---------- メンバ変数 ---------- ///
	
	std::unique_ptr<Object3D> playerObject_;
	Transform transform_;

	LaneInfo laneInfo_; // レーン管理
	JumpInfo jumpInfo_; // ジャンプ処理
	RotationInfo rotationInfo_; // 回転処理
};

