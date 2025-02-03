#pragma once
#include "BaseCharacter.h"

/// -------------------------------------------------------------
///							エネミークラス
/// -------------------------------------------------------------
class Enemy : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

    // 初期化処理
    void Initialize(Object3DCommon* object3DCommon, Camera* camera) override;

    // 更新処理
    void Update() override;

    // 描画処理
    void Draw() override;

public:
    // エネミーの中心座標
    Vector3 GetCenterPosition() const override;

    // エネミーの現在位置を取得
    Vector3 GetPosition() const { return parts_[0].worldTransform.translate; }

    // エネミーの位置を設定
    void SetPosition(const Vector3& position) { parts_[0].worldTransform.translate = position; }

    // エネミーの回転角度を取得
    float GetRotation() const { return parts_[0].worldTransform.rotate.y; }

    // エネミーの回転角度を設定
    void SetRotation(float angle) { parts_[0].worldTransform.rotate.y = angle; }

    // エネミーが倒されたかを取得
    bool IsDead() const { return isDead_; }

    // エネミーを倒す
    void SetDead(bool dead) { isDead_ = dead; }

private: /// ---------- メンバ変数 ---------- ///

    bool isDead_ = false; // 敵の生存フラグ（デフォルトは生存）
    float angle_ = 0.0f;   // 現在の角度 (ラジアン)
    float radius_ = 50.0f;  // 円の半径
    float speed_ = 0.005f;  // 回転速度
    Vector3 centerPosition_ = { 0.0f, 0.0f, 0.0f }; // 円運動の中心座標
};
