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