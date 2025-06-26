#pragma once
#include "AnimationModel.h"
#include <Collider.h>

#include <memory>

class DummyModel : public Collider
{
public:

	void Initialize();
	void Update();
	void Draw();
	void DrawImGui();

private:

	std::unique_ptr<AnimationModel> model_;
};

