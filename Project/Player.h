#pragma once
#include "WorldTransform.h"
#include "Camera.h"
#include "Object3D.h"
#include "Input.h"

#include <vector>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class Object3DCommon;

/// -------------------------------------------------------------
///							プレイヤークラス
/// -------------------------------------------------------------
class Player
{
private: /// ---------- 構造体 ---------- ///

	struct Part
	{
		WorldTransform worldTransform;
		std::shared_ptr<Object3D> object3D; // shared_ptrでコピー可能
		std::string modelFile;
		int parentIndex = -1; // 親のインデックス (-1なら親なし)
		Vector3 localOffset = { 0.0f, 0.0f, 0.0f }; // 親に対する相対位置
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon, Camera* camera);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

private: /// ---------- メンバ関数 ---------- ///

	// 浮遊ギミック初期化処理
	void InitializeFlaotingGimmick();

	// 浮遊ギミックの更新処理
	void UpdateFloatingGimmick();

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr;
	Camera* camera_ = nullptr;

	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f }; // 回転角度 (x, y, z)
	Vector3 velocity = { 0.0f, 0.0f, 0.0f }; // カメラの速度

	const float damping = 0.9f;            // 減衰率（小さいほど追従がゆっくり）
	const float stiffness = 0.1f;          // カメラの目標位置への「引っ張り力」
	const float PI = 3.1415926535897932462643383279502884197169399375f;

	// ギミックのパラメータ
	float floatingParameter_ = 0.0f;

	// ステップを決める
	const uint16_t periodTime = static_cast<uint16_t>(60.0f);  // 1.0f / 60.0f ではなく、フレーム数として定義
	const float step = 2.0f * PI / periodTime;

	// 腕の振り用のパラメータ
	float armSwingParameter_ = 0.0f;
	const float armSwingSpeed_ = 1.0f;  // 揺れの速さ
	const float armSwingAmplitude_ = 0.6f;  // 揺れの振幅

	std::vector<Part> parts_; // 各部位をまとめて管理

};
