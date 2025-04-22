#include "ParticleTransform.h"
#include <DirectXCommon.h>

void ParticleTransform::UpdateMatrix(const Matrix4x4& viewProjection, bool useBillboard, const Matrix4x4& billboardMatrix)
{
	// 行列構築
	if (useBillboard)
	{
		Matrix4x4 scaleMat = Matrix4x4::MakeScaleMatrix(scale_);
		Matrix4x4 transMat = Matrix4x4::MakeTranslateMatrix(translate_);
		worldMatrix_ = scaleMat * billboardMatrix * transMat;
	}
	else
	{
		worldMatrix_ = Matrix4x4::MakeAffineMatrix(scale_, rotate_, translate_);
	}

	// WVPも更新
	wvpMatrix_ = Matrix4x4::Multiply(worldMatrix_, viewProjection);
}
