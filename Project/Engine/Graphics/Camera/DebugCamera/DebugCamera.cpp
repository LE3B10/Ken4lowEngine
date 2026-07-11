#define NOMINMAX
#include "DebugCamera.h"
#include "WinApp.h"
#include "GameViewportConstants.h"
#include "Input.h"
#include "ParameterManager.h"

#include <algorithm>

namespace Ken4lowEngine
{

	DebugCamera* DebugCamera::GetInstance()
	{
		static DebugCamera instance;
		return &instance;
	}

	void DebugCamera::Initialize()
	{
		worldTransform_.Initialize();
		worldTransform_.translate_ = { 0.0f, 0.0f, -20.0f };

		fovY_ = 1.0f;
		aspectRatio_ = GameViewportConstants::Aspect; // Editor上のProjectionは固定内部解像度16:9へ統一する。
		nearClip_ = 0.1f;
		farClip_ = 1000.0f;

		UpdateViewProjection();
	}

	void DebugCamera::Finalize()
	{
		worldTransform_ = {};
		worldMatrix_ = {};
		rotateMatrix_ = {};
		viewMatrix_ = {};
		projectionMatrix_ = {};
		viewProjectionMatrix_ = {};
		rotation_ = {};

		fovY_ = 0.0f;
		aspectRatio_ = 0.0f;
		nearClip_ = 0.0f;
		farClip_ = 0.0f;
	}

	void DebugCamera::SetAspectRatio(float aspectRatio)
	{
		if (aspectRatio <= 0.0f)
		{
			return;
		}

		aspectRatio_ = aspectRatio;
		UpdateViewProjection();
	}

	void DebugCamera::Update()
	{
		Move();
		UpdateViewProjection();
	}

	void DebugCamera::RefreshViewProjection()
	{
		UpdateViewProjection(); // Draw中のEditor Camera操作を次フレーム待ちにせず行列へ反映する。
	}

	void DebugCamera::ApplyEditorNavigation(const Vector3& localMove, float pitchDelta, float yawDelta)
	{
		worldTransform_.rotate_.x = std::clamp(worldTransform_.rotate_.x + pitchDelta, -1.5533f, 1.5533f);
		worldTransform_.rotate_.y += yawDelta;

		// 移動量は更新後のカメラ回転を基準にワールド空間へ変換する。
		const Matrix4x4 cameraRotation = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);
		worldTransform_.translate_ += Vector3::Transform(localMove, cameraRotation);
		UpdateViewProjection();
	}

	void DebugCamera::Move()
	{
		Vector3 move = { 0.0f, 0.0f, 0.0f };

		if (Input::GetInstance()->PushKey(DIK_W)) { move.z += 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_S)) { move.z -= 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_A)) { move.x -= 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_D)) { move.x += 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_SPACE)) { move.y += 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_LSHIFT)) { move.y -= 0.2f; }

		if (Input::GetInstance()->PushKey(DIK_UP)) { worldTransform_.rotate_.x -= 0.02f; }
		if (Input::GetInstance()->PushKey(DIK_DOWN)) { worldTransform_.rotate_.x += 0.02f; }
		if (Input::GetInstance()->PushKey(DIK_LEFT)) { worldTransform_.rotate_.y += 0.02f; }
		if (Input::GetInstance()->PushKey(DIK_RIGHT)) { worldTransform_.rotate_.y -= 0.02f; }

		move = Vector3::Transform(move, rotateMatrix_);
		worldTransform_.translate_ += move;
	}

	void DebugCamera::UpdateViewProjection()
	{
		rotateMatrix_ = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);
		worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);
		viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);
		projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
		viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);
	}

} // namespace Ken4lowEngine
