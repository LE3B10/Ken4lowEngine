#pragma once
#include <BaseCharacter.h>
#include <Object3D.h>
#include "ContactRecord.h"

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }
class CollisionManager;
class Enemy;

/// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~Player() = default;

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update(float deltaTime) override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui() override;

	// 衝突判定を行う
	void OnCollision(K4E::Collider* other) override;

	// ワールド変換の取得
	K4E::WorldTransformEx* GetWorldTransform() { return &body_.transform; }

private: /// ----------メンバ変数 ---------- ///

	K4E::Input* input_ = nullptr; // 入力クラス

	K4E::ContactRecord contactRecord_; // 接触記録

	std::string skinTexturePath_ = "steve.png"; // スキンテクスチャパス

	// 体力
	float maxHp_ = 100.0f;
	float hp_ = 100.0f;
};

