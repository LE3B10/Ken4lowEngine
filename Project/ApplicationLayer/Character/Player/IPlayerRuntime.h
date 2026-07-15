#pragma once

#include <Vector3.h>

namespace Ken4lowEngine
{
	class Camera;
	class Collider;
}

/// GamePlay側が巨大な具象Playerへ直接依存せず、PlayerActorを正本として扱うRuntime境界。
class IPlayerRuntime
{
public:
	virtual ~IPlayerRuntime() = default;

	virtual float GetHP() const = 0;
	virtual float GetMaxHP() const = 0;
	virtual bool IsGameOverReady() const = 0;
	virtual bool ConsumeGameOverReady() = 0;
	virtual bool IsDeathActive() const = 0;

	virtual Ken4lowEngine::Vector3 GetWorldPosition() const = 0;
	virtual Ken4lowEngine::Collider* GetCollisionPrimitive() = 0;
	virtual const Ken4lowEngine::Collider* GetCollisionPrimitive() const = 0;
	virtual Ken4lowEngine::Camera* GetCamera() const = 0;

	virtual float ApplyRuntimeDamage(float amount) = 0;
	virtual float HealRuntime(float amount) = 0;

	virtual int AddReserveAmmo(int amount) = 0;
	virtual int GetMagazineAmmo() const = 0;
	virtual int GetMagazineCapacity() const = 0;
	virtual int GetReserveAmmo() const = 0;
	virtual int GetMaxReserveAmmo() const = 0;
	virtual bool IsReloading() const = 0;
	virtual float GetReloadTimer() const = 0;
	virtual float GetReloadDuration() const = 0;

	virtual void SetViewLookAngles(float pitch, float yaw) = 0;
};
