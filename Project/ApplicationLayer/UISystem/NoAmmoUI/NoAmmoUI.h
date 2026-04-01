#pragma once
#include <Sprite.h>
#include <memory>

namespace K4E = ::Ken4lowEngine;

class NoAmmoUI
{
public:
	void Initialize(const std::string& texturePath = "ui/no_ammo.png");
	void Update(float deltaTime);
	void Draw();

	void SetVisible(bool v) { visible_ = v; }
	bool IsVisible() const { return visible_; }

private:
	std::unique_ptr<K4E::Sprite> sprite_;
	bool visible_ = false;
	float blinkTimer_ = 0.0f;
};