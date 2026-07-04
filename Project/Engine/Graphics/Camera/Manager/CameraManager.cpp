#include "CameraManager.h"
#include "Camera.h"
#include "DebugCamera.h"

#include <cmath>

namespace Ken4lowEngine
{
	CameraManager* CameraManager::GetInstance()
	{
		static CameraManager instance;
		return &instance;
	}

	void CameraManager::Initialize()
	{
		debugCamera_ = DebugCamera::GetInstance();
		useDebugCamera_ = false;
	}

	void CameraManager::Finalize()
	{
		mainCamera_ = nullptr;
		debugCamera_ = nullptr;
		useDebugCamera_ = false;
	}

	void CameraManager::Update()
	{
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_)
		{
			debugCamera_->Update();
		}
#endif
	}

	void CameraManager::UpdateAudioListener(float deltaTime)
	{
		const Vector3 position = GetActiveCameraPosition();
		Vector3 forward{ 0.0f, 0.0f, 1.0f };

#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_)
		{
			forward = Vector3::Transform({ 0.0f, 0.0f, 1.0f }, Matrix4x4::MakeRotateMatrix(debugCamera_->GetRotate()));
		}
		else
#endif
			if (mainCamera_)
			{
				forward = mainCamera_->GetForward();
			}

		forward = Vector3::NormalizeSafe(forward, { 0.0f, 0.0f, 1.0f });
		Vector3 right = Vector3::NormalizeSafe(Vector3::Cross({ 0.0f, 1.0f, 0.0f }, forward), { 1.0f, 0.0f, 0.0f });
		Vector3 up = Vector3::NormalizeSafe(Vector3::Cross(forward, right), { 0.0f, 1.0f, 0.0f });
		audioListener_.UpdateFromTransform(position, forward, right, up, deltaTime);
	}

	void CameraManager::SetMainCamera(Camera* camera)
	{
		mainCamera_ = camera;
	}

	void CameraManager::SetUseDebugCamera(bool useDebugCamera)
	{
		useDebugCamera_ = useDebugCamera;
	}

	Matrix4x4 CameraManager::GetActiveViewMatrix() const
	{
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_)
		{
			return debugCamera_->GetViewMatrix();
		}
#endif
		if (mainCamera_)
		{
			return mainCamera_->GetViewMatrix();
		}
		return Matrix4x4::MakeIdentity();
	}

	Matrix4x4 CameraManager::GetActiveProjectionMatrix() const
	{
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_)
		{
			return debugCamera_->GetProjectionMatrix();
		}
#endif
		if (mainCamera_)
		{
			return mainCamera_->GetProjectionMatrix();
		}
		return Matrix4x4::MakeIdentity();
	}

	Matrix4x4 CameraManager::GetActiveViewProjectionMatrix() const
	{
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_)
		{
			return debugCamera_->GetViewProjectionMatrix();
		}
#endif
		if (mainCamera_)
		{
			return mainCamera_->GetViewProjectionMatrix();
		}
		return Matrix4x4::MakeIdentity();
	}

	Vector3 CameraManager::GetActiveCameraPosition() const
	{
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_)
		{
			return debugCamera_->GetTranslate();
		}
#endif
		if (mainCamera_)
		{
			return mainCamera_->GetTranslate();
		}
		return { 0.0f, 0.0f, 0.0f };
	}
}
