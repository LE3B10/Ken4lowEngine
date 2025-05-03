#include "Model.h"


void Model::Initialize(const std::string& fileName)
{
	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize(fileName);
}

void Model::Update()
{
	object3D_->Update();
}

void Model::Draw()
{
	object3D_->Draw();
}
