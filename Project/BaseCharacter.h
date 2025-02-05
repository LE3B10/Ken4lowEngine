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
///						キャラクターの基底クラス
/// -------------------------------------------------------------
class BaseCharacter
{
protected: /// ---------- 構造体 ---------- ///

    struct Part {
        WorldTransform worldTransform;
        std::shared_ptr<Object3D> object3D;
        std::string modelFile;
        int parentIndex = -1;
        Vector3 localOffset = { 0.0f, 0.0f, 0.0f };
    };

public: /// ---------- メンバ仮想関数 ---------- ///

    // 初期化処理
    virtual void Initialize(Object3DCommon* object3DCommon, Camera* camera);

    // 更新処理
    virtual void Update();

    // 描画処理
    virtual void Draw();

    virtual Vector3 GetCenterPosition() const;

protected: /// ---------- メンバ変数 ---------- ///

    std::vector<Part> parts_; // 各部位をまとめて管理
    Input* input_ = nullptr;
    Camera* camera_ = nullptr;

    const float PI = 3.1415926535897932462643383279502884197169399375f;

};

