#include "WorldTransformEx.h"


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void WorldTransformEx::Update()
{
	// ローカル変換行列を作成
	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(scale_, rotate_, translate_);

	// 親オブジェクトがあれば親のワールド行列を掛ける
	if (parent_) worldMatrix = Matrix4x4::Multiply(worldMatrix, parent_->worldMatrix_);

	// 親の回転を引き継ぐ
	worldRotate_ = parent_ ? parent_->worldRotate_ + rotate_ : rotate_;

	// ワールド座標を取得
	worldTranslate_ = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };

	// ワールド行列を保存
	worldMatrix_ = worldMatrix;
}

/// -------------------------------------------------------------
///		拡張更新処理（親付き、オフセット・回転・加算あり）
/// -------------------------------------------------------------
void WorldTransformEx::Update(const WorldTransformEx* parent, const Vector3& offset, float preRotateX, const Vector3& selfAdd)
{
    if (!parent) return;

    // 親のYaw/Pitchのみ継承（順序は既存コードに合わせて Rx→Ry）
    Matrix4x4 Rx = Matrix4x4::MakeRotateX(parent->rotate_.x);
    Matrix4x4 Ry = Matrix4x4::MakeRotateY(parent->rotate_.y);
    Matrix4x4 R = Matrix4x4::Multiply(Rx, Ry);

    // ローカルオフセットを 事前X回転 → 親のYaw/Pitch でワールド化
    Matrix4x4 RxFix = Matrix4x4::MakeRotateX(preRotateX);
    Vector3 ofsLocalFixed = Matrix4x4::Transform(offset, RxFix);
    Vector3 ofsWorld = Matrix4x4::Transform(ofsLocalFixed, R);

    // 位置
    translate_ = parent->translate_ + ofsWorld;

    // 回転は親の回転を継承してから任意の微調整を加算
    rotate_ = parent->rotate_;
    rotate_.x += selfAdd.x;
    rotate_.y += selfAdd.y;
    rotate_.z += selfAdd.z;
}
