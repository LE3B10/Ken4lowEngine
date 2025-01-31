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

private: /// ---------- メンバ変数 ---------- ///

    float angle_ = 0.0f;   // 現在の角度 (ラジアン)
    float radius_ = 50.0f;  // 円の半径
    float speed_ = 0.005f;  // 回転速度
    Vector3 centerPosition_ = { 0.0f, 0.0f, 0.0f }; // 円運動の中心座標

};


