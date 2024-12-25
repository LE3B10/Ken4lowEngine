#include "CameraManager.h"



void CameraManager::Update(float deltaTime)
{
	if (activeCamera_)
	{
		activeCamera_->Update();
	}
}



void CameraManager::AddCamera(const std::string& name, std::unique_ptr<Camera> camera)
{
	if (cameras_.find(name) == cameras_.end())
	{
		cameras_[name] = std::move(camera);
	}
}



void CameraManager::SetActiveCamera(const std::string& name)
{
	auto it = cameras_.find(name);
	if (it != cameras_.end())
	{
		activeCamera_ = it->second.get();
	}
}



Camera* CameraManager::GetCamera(const std::string& name) const
{
	auto it = cameras_.find(name);
	return (it != cameras_.end()) ? it->second.get() : nullptr;
}
