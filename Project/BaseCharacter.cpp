#include "BaseCharacter.h"

void BaseCharacter::UpdateHierarchy()
{
	for (size_t i = 0; i < parts_.size(); ++i)
	{
		auto& p = parts_[i];

		//ローカル行列 → 親に合成
		p.localMatrix = Matrix4x4::MakeAffineMatrix(p.scale, p.rotate, p.traslate);
		if (p.parentIndex >= 0)
		{
			p.worldMatrix = Matrix4x4::Multiply(parts_[p.parentIndex].worldMatrix, p.localMatrix);
		}
		else
		{
			p.worldMatrix = p.localMatrix;
		}

		// Object3D には「ワールドS/R/T」を渡す（可視に関係なく毎フレーム）
		if (p.object)
		{
			Vector3 worldT{ p.worldMatrix.m[3][0], p.worldMatrix.m[3][1], p.worldMatrix.m[3][2] };
			p.object->SetTranslate(worldT);
			p.object->Update();
		}
	}
}

Vector3 BaseCharacter::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f, 1.0f, 0.0f };
	Vector3 worldPosition = parts_.
	return Vector3();
}
