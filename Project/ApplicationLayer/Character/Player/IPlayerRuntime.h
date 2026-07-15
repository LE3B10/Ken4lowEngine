#pragma once

/// GamePlay側が巨大な具象Playerへ直接依存せず、段階移行中も共通して参照する最小Runtime境界。
class IPlayerRuntime
{
public:
	virtual ~IPlayerRuntime() = default;

	virtual float GetHP() const = 0;
	virtual float GetMaxHP() const = 0;
	virtual bool IsGameOverReady() const = 0;
	virtual bool ConsumeGameOverReady() = 0;
	virtual bool IsDeathActive() const = 0;
};
