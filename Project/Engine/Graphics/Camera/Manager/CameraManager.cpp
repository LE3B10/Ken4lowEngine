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
		gameCamera_ = nullptr;
		renderCamera_ = nullptr;
		useDebugCamera_ = false;
	}

	void CameraManager::Finalize()
	{
		mainCamera_ = nullptr;
		gameCamera_ = nullptr;
		renderCamera_ = nullptr;
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
		if (!gameCamera_) { gameCamera_ = camera; }
		if (!renderCamera_) { renderCamera_ = camera; }
	}

	void CameraManager::SetRenderCamera(Camera* camera)
	{
		renderCamera_ = camera;
	}

	void CameraManager::SetGameCamera(Camera* camera)
	{
		gameCamera_ = camera;
	}

	void CameraManager::SetDebugCamera(DebugCamera* camera)
	{
		debugCamera_ = camera;
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
		if (GetRenderCamera())
		{
			return GetRenderCamera()->GetViewMatrix();
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
		if (GetRenderCamera())
		{
			return GetRenderCamera()->GetProjectionMatrix();
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
		if (GetRenderCamera())
		{
			return GetRenderCamera()->GetViewProjectionMatrix();
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
		if (GetRenderCamera())
		{
			return GetRenderCamera()->GetTranslate();
		}
		return { 0.0f, 0.0f, 0.0f };
	}
}