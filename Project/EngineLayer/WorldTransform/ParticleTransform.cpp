#include "ParticleTransform.h"
#include <DirectXCommon.h>

void ParticleTransform::UpdateMatrix(const Matrix4x4& viewProjection, bool useBillboard, const Matrix4x4& billboardMatrix)
{
	// 行列構築
    if (useBillboard)
    {
        // Z軸回転のみ行列を作成
        Matrix4x4 rotZMat = Matrix4x4::MakeRotateZMatrix(rotate_.z);

        // スケール
        Matrix4x4 scaleMat = Matrix4x4::MakeScaleMatrix(scale_);

        // 平面に向けたビルボードの基底を作成
        Matrix4x4 facingMat = billboardMatrix;

        // Z回転は facingMat の上に乗せる（ローカル回転として合成）
        Matrix4x4 combinedRot = Matrix4x4::Multiply(rotZMat, facingMat); // ←ここが重要

        // 平行移動
        Matrix4x4 transMat = Matrix4x4::MakeTranslateMatrix(translate_);

        // 合成：scale → rotation → translation
        worldMatrix_ = Matrix4x4::Multiply(Matrix4x4::Multiply(scaleMat, combinedRot), transMat);
    }
	else
	{
		worldMatrix_ = Matrix4x4::MakeAffineMatrix(scale_, rotate_, translate_);
	}

	// WVP更新
	wvpMatrix_ = Matrix4x4::Multiply(worldMatrix_, viewProjection);
}
