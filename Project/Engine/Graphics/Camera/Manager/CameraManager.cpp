#include "CameraManager.h"
#include "Camera.h"
#include "DebugCamera.h"

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