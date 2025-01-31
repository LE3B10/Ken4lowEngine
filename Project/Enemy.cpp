#include "Enemy.h"
#include <MatrixMath.h>
#include <VectorMath.h>


/// -------------------------------------------------------------
///						　	初期化処理
/// -------------------------------------------------------------
void Enemy::Initialize(Object3DCommon* object3DCommon, Camera* camera)
{
	BaseCharacter::Initialize(object3DCommon, camera);

	// 円運動の中心位置を決定
	centerPosition_ = { 0.0f, 0.0f, 100.0f }; // 原点を中心にする

	// モデルの配置
	parts_ = {
		{ {{1.0f, 1.0f, 1.0f}, {radius_, 0.0f, 0.0f}}, nullptr, "Enemy/enemy.gltf", -1 }, // 初期位置を円周上に
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.5f, 0.0f}}, nullptr, "Enemy/enemy_LArm.gltf", 0 },
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.5f, 0.0f}}, nullptr, "Enemy/enemy_RArm.gltf", 0 },
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
}


/// -------------------------------------------------------------
///						　	更新処理
/// -------------------------------------------------------------
void Enemy::Update()
{
	BaseCharacter::Update();

    // **円運動の計算**
    angle_ += speed_;  // 角度を増加
    if (angle_ > 2.0f * PI) { angle_ -= 2.0f * PI; } // 2πを超えたらリセット

    // **円軌道の座標を計算**
    parts_[0].worldTransform.translate.x = centerPosition_.x + radius_ * std::cos(angle_);
    parts_[0].worldTransform.translate.z = centerPosition_.z + radius_ * std::sin(angle_);

    // 進行方向に応じて回転させる
    parts_[0].worldTransform.rotate.y = atan2f(std::sin(angle_), std::cos(angle_));

    // **子オブジェクトの最終的な位置と回転を計算**
    for (size_t i = 1; i < parts_.size(); ++i)
    {
        int parentIndex = parts_[i].parentIndex;
        if (parentIndex != -1)
        {
            const Vector3& parentTranslation = parts_[parentIndex].worldTransform.translate;
            const Vector3& parentRotation = parts_[parentIndex].worldTransform.rotate;

            // 親の回転行列を作成
            Matrix4x4 rotationMatrix = MakeRotateYMatrix(parentRotation.y);

            // ローカルオフセットを親の回転に合わせて適用
            Vector3 rotatedOffset = Transform(parts_[i].localOffset, rotationMatrix);

            // 計算結果を適用
            parts_[i].worldTransform.translate = parentTranslation + rotatedOffset;
            parts_[i].worldTransform.rotate.y = parentRotation.y;
        }
    }

    // 各部位のオブジェクトの位置を更新
    for (auto& part : parts_) {
        part.object3D->SetTranslate(part.worldTransform.translate);
        part.object3D->SetRotate(part.worldTransform.rotate);
        part.object3D->Update();
    }
}


/// -------------------------------------------------------------
///						　	描画処理
/// -------------------------------------------------------------
void Enemy::Draw()
{
	for (auto& part : parts_)
	{
		part.object3D->Draw();
	}
}
