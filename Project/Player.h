#pragma once
#include "BaseCharacter.h"


/// ---------- 前方宣言 ---------- ///
class Object3DCommon;


/// -------------------------------------------------------------
///							プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon, Camera* camera) override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

private: /// ---------- メンバ関数 ---------- ///

	// 浮遊ギミック初期化処理
	void InitializeFlaotingGimmick();

	// 浮遊ギミックの更新処理
	void UpdateFloatingGimmick();

private: /// ---------- メンバ変数 ---------- ///

	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f }; // 回転角度 (x, y, z)
	Vector3 velocity = { 0.0f, 0.0f, 0.0f }; // カメラの速度

	const float damping = 0.9f;            // 減衰率（小さいほど追従がゆっくり）
	const float stiffness = 0.1f;          // カメラの目標位置への「引っ張り力」

	// ギミックのパラメータ
	float floatingParameter_ = 0.0f;

	// ステップを決める
	const uint16_t periodTime = static_cast<uint16_t>(60.0f);  // 1.0f / 60.0f ではなく、フレーム数として定義
	const float step = 2.0f * PI / periodTime;

	// 腕の振り用のパラメータ
	float armSwingParameter_ = 0.0f;
	const float armSwingSpeed_ = 1.0f;  // 揺れの速さ
	const float armSwingAmplitude_ = 0.6f;  // 揺れの振幅

};
