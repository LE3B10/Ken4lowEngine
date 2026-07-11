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
		aspectRatio_ = GameViewportConstants::Aspect;
		nearClip_ = 0.1f;
		farClip_ = 1000.0f;
		editorLookCaptureInitialized_ = false;
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
		editorLookCaptureInitialized_ = false;
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
		Input* input = Input::GetInstance();
		if (!input || !input->GetLockCursor())
		{
			editorLookCaptureInitialized_ = false; // RMB解除後は次回クリックの初回差分を再び無効化する。
		}
		Move();
		UpdateViewProjection();
	}

	void DebugCamera::RefreshViewProjection()
	{
		UpdateViewProjection();
	}

	void DebugCamera::ApplyEditorNavigation(const Vector3& localMove, float pitchDelta, float yawDelta)
	{
		Input* input = Input::GetInstance();
		if (input && input->GetLockCursor())
		{
			if (!editorLookCaptureInitialized_)
			{
				pitchDelta = 0.0f;
				yawDelta = 0.0f;
				editorLookCaptureInitialized_ = true; // 右クリック開始位置から中央へ飛ぶ差分は回転へ使わない。
			}
			else
			{
				const float rawX = static_cast<float>(input->GetMouseMoveX());
				const float rawY = static_cast<float>(input->GetMouseMoveY());
				pitchDelta = std::clamp(rawY * 0.003f, -0.15f, 0.15f);
				yawDelta = std::clamp(-rawX * 0.003f, -0.15f, 0.15f);
			}
		}

		worldTransform_.rotate_.x = std::clamp(worldTransform_.rotate_.x + pitchDelta, -1.5533f, 1.5533f);
		worldTransform_.rotate_.y += yawDelta;

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
