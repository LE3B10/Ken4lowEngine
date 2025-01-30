#include "Player.h"
#include "Object3DCommon.h"
#include "VectorMath.h"
#include "MatrixMath.h"


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize(Object3DCommon* object3DCommon, Camera* camera)
{
	input_ = Input::GetInstance();
	camera_ = camera;

	parts_ = {
		// 胴体（親なし）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}}, nullptr, "body.gltf", -1 },

		// 左腕（親は胴体）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {-0.5f, 4.5f, 0.0f}}, nullptr, "L_Arm.gltf", 0 },

		// 右腕（親は胴体）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 4.5f, 0.0f}}, nullptr, "R_Arm.gltf", 0 },

		// 頭（親は胴体）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 5.0f, 0.0f}}, nullptr, "Head.gltf", 0 }
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
		parts_[0].worldTransform.rotate.y = cameraYaw + (PI / 2.0f); // カメラ基準で補正
	}
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
    // 移動量を初期化
    Vector3 movement = { 0.0f, 0.0f, 0.0f };

    // 入力取得 (移動)
    if (input_->PushKey(DIK_W)) { movement.z += 1.0f; }
    if (input_->PushKey(DIK_S)) { movement.z -= 1.0f; }
    if (input_->PushKey(DIK_A)) { movement.x -= 1.0f; }
    if (input_->PushKey(DIK_D)) { movement.x += 1.0f; }

    // カメラの向きに基づいて移動方向を計算
    if (camera_)
    {
        Vector3 forward = camera_->GetForwardDirection();
        forward.y = 0.0f; // 水平移動のためY成分をゼロにする
        forward = Normalize(forward);

        Vector3 right = Normalize(Cross(Vector3(0.0f, 1.0f, 0.0f), forward)); // 水平方向の右ベクトル

        Vector3 adjustedMovement = -forward * movement.z + -right * movement.x;
        adjustedMovement.y = 0.0f;

        // 胴体（親）の移動を適用
        parts_[0].worldTransform.translate += adjustedMovement * 0.3f; // 移動速度を調整

        // 移動している場合のみ向きを調整
        if (Length(adjustedMovement) > 0.001f)
        {
            Vector3 forwardDirection = Normalize(adjustedMovement);
            parts_[0].worldTransform.rotate.y = atan2f(-forwardDirection.x, forwardDirection.z); // 軸の順序を調整
        }
        else
        {
            float cameraYaw = camera_->GetYaw();
            parts_[0].worldTransform.rotate.y = cameraYaw + (PI / 2.0f); // カメラ基準で補正
        }
    }

    // 親の位置と回転を考慮して各部位の最終的な位置と回転を計算
    for (size_t i = 1; i < parts_.size(); ++i)
    {
        int parentIndex = parts_[i].parentIndex;
        if (parentIndex != -1)
        {
            // 親の回転を適用してローカルオフセットを回転
            const Vector3& parentRotation = parts_[parentIndex].worldTransform.rotate;
            Matrix4x4 rotationMatrix = MakeRotateYMatrix(parentRotation.y);
            Vector3 rotatedOffset = Transform(parts_[i].localOffset, rotationMatrix);

            // 親の位置と回転を反映
            parts_[i].worldTransform.translate = parts_[parentIndex].worldTransform.translate + rotatedOffset;
            parts_[i].worldTransform.rotate = parts_[parentIndex].worldTransform.rotate; // 親と同じ回転を適用
        }
    }

    // 各部位のオブジェクトの位置と回転を更新
    for (auto& part : parts_)
    {
        part.object3D->SetRotate(part.worldTransform.rotate);
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
