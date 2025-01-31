#include "Player.h"
#include "Object3DCommon.h"
#include "VectorMath.h"
#include "MatrixMath.h"


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize(Object3DCommon* object3DCommon, Camera* camera)
{
    BaseCharacter::Initialize(object3DCommon, camera);

    // 浮遊ギミックの初期化
    InitializeFlaotingGimmick();

    parts_ = {
        // 胴体（親なし）
        { {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}}, nullptr, "Player/body.gltf", -1 },

        // 左腕（親は胴体）
        { {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {-0.5f, 4.5f, 0.0f}}, nullptr, "Player/L_Arm.gltf", 0 },

        // 右腕（親は胴体）
        { {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 4.5f, 0.0f}}, nullptr, "Player/R_Arm.gltf", 0 },

        // 頭（親は胴体）
        { {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 5.0f, 0.0f}}, nullptr, "Player/Head.gltf", 0 }
    };

    for (size_t i = 0; i < parts_.size(); ++i)
    {
        auto& part = parts_[i];
        part.object3D = std::make_unique<Object3D>();
        part.object3D->Initialize(object3DCommon, part.modelFile);
        part.object3D->SetTranslate(part.worldTransform.translate);

        // 子オブジェクトのローカル座標を記録
        if (part.parentIndex != -1)
        {
            part.localOffset = part.worldTransform.translate - parts_[part.parentIndex].worldTransform.translate;
        }
    }

    // プレイヤーの初期回転をカメラの角度に合わせる
    if (camera_)
    {
        float cameraYaw = camera_->GetYaw();
        parts_[0].worldTransform.rotate.y = cameraYaw; // カメラ基準で補正
    }
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
    BaseCharacter::Update();

    // 浮遊ギミックの更新処理
    UpdateFloatingGimmick();

    // 移動量を初期化
    Vector3 movement = { 0.0f, 0.0f, 0.0f };

    // 入力取得 (移動)
    if (input_->PushKey(DIK_W)) { movement.z += 1.0f; }
    if (input_->PushKey(DIK_S)) { movement.z -= 1.0f; }
    if (input_->PushKey(DIK_A)) { movement.x -= 1.0f; }
    if (input_->PushKey(DIK_D)) { movement.x += 1.0f; }

    float targetYaw = parts_[0].worldTransform.rotate.y; // 現在の角度を基準に

    // カメラの向きに基づいて移動方向を計算
    if (camera_)
    {
        Vector3 forward = camera_->GetForwardDirection();
        forward.y = 0.0f; // 水平移動のためY成分をゼロにする

        if (Length(forward) > 0.001f) {
            forward = Normalize(forward);
        }
        else {
            forward = Vector3(0.0f, 0.0f, 1.0f); // デフォルト方向を設定
        }

        Vector3 right = Normalize(Cross(Vector3(0.0f, 1.0f, 0.0f), forward)); // 水平方向の右ベクトル

        Vector3 adjustedMovement = -forward * movement.z + -right * movement.x;
        adjustedMovement.y = 0.0f;

        // 胴体（親）の移動を適用
        parts_[0].worldTransform.translate += adjustedMovement * 0.3f; // 移動速度を調整

        // 移動している場合のみ目標角度を計算
        if (Length(adjustedMovement) > 0.001f)
        {
            Vector3 forwardDirection = Normalize(adjustedMovement);
            targetYaw = atan2f(-forwardDirection.x, forwardDirection.z); // 移動方向を向く
        }
    }

    // 現在の角度と目標角度を補間
    float currentYaw = parts_[0].worldTransform.rotate.y;
    parts_[0].worldTransform.rotate.y = LerpShortAngle(currentYaw, targetYaw, 0.1f); // t=0.1で補間

    // 腕の揺れのパラメータを常に更新
    armSwingParameter_ += step * armSwingSpeed_;

    // 揺れの大きさを移動量に応じて変化させる（移動時は大きく、静止時は小さめ）
    float swingAmplitude = (Length(movement) > 0.001f) ? armSwingAmplitude_ : armSwingAmplitude_ * 0.5f;

    // 移動しているかどうかで揺れ方を変える
    float leftArmSwing, rightArmSwing;
    if (Length(movement) > 0.001f) {
        // 移動時：交互に揺れる
        leftArmSwing = std::sin(armSwingParameter_) * swingAmplitude;
        rightArmSwing = std::sin(armSwingParameter_ + PI) * swingAmplitude;
    }
    else {
        // 静止時：同じ方向に揺れる
        leftArmSwing = std::sin(armSwingParameter_) * swingAmplitude;
        rightArmSwing = leftArmSwing; // 左右同じ揺れ
    }

    // 両腕の回転に適用
    parts_[1].worldTransform.rotate.x = leftArmSwing;  // 左腕
    parts_[2].worldTransform.rotate.x = rightArmSwing; // 右腕

    // 各部位の最終的な位置と回転を計算
    for (size_t i = 1; i < parts_.size(); ++i)
    {
        int parentIndex = parts_[i].parentIndex;
        if (parentIndex != -1)
        {
            const Vector3& parentRotation = parts_[parentIndex].worldTransform.rotate;
            Matrix4x4 rotationMatrix = MakeRotateYMatrix(parentRotation.y);
            Vector3 rotatedOffset = Transform(parts_[i].localOffset, rotationMatrix);

            parts_[i].worldTransform.translate = parts_[parentIndex].worldTransform.translate + rotatedOffset;
            parts_[i].worldTransform.rotate.y = parentRotation.y; // Y回転のみ親と同じにする
        }
    }

    // 各部位のオブジェクトの位置と回転を更新
    for (auto& part : parts_)
    {
        part.object3D->SetRotate(part.worldTransform.rotate);  // 回転を適用
        part.object3D->SetTranslate(part.worldTransform.translate);
        part.object3D->Update();
    }

    // プレイヤーの位置
    const Vector3& playerPosition = parts_[0].worldTransform.translate;

    // カメラ位置を線形補間でスムーズに移動
    if (camera_)
    {
        camera_->SetTargetPosition(playerPosition);
        camera_->Update();
    }
}

/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
    for (auto& part : parts_)
    {
        part.object3D->Draw();
    }
}


/// -------------------------------------------------------------
///					浮遊ギミックの初期化処理
/// -------------------------------------------------------------
void Player::InitializeFlaotingGimmick()
{
    floatingParameter_ = 0.0f;
}


/// -------------------------------------------------------------
///					 浮遊ギミックの更新処理
/// -------------------------------------------------------------
void Player::UpdateFloatingGimmick()
{
    // 浮遊の振幅<m>
    const float amplitude = 0.4f; // 浮遊の振幅
    const float floatingSpeed = 0.02f; // 浮遊の速度を遅くする

    // パラメータを1ステップ分加算
    floatingParameter_ += step;
    // ２πを超えたら0戻す
    floatingParameter_ = std::fmod(floatingParameter_, 2.0f * PI);

    parts_[0].worldTransform.translate.y = std::sin(floatingParameter_) * amplitude;
}
