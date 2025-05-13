#include "ParticleMaterial.h"
#include "ResourceManager.h"
#include "DirectXCommon.h"

void ParticleMaterial::Initialize()
{
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

	//マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource_ = ResourceManager::CreateBufferResource(device, sizeof(MaterialCBData));
	//書き込むためのアドレスを取得
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	//今回は赤を書き込んでみる
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	//UVTramsform行列を単位行列で初期化
	materialData_->uvTransform = Matrix4x4::MakeIdentity();
}

void ParticleMaterial::Update()
{
	if (materialData_)
	{
		materialData_->color = this->materialData_->color;			   // 色
		materialData_->uvTransform = this->materialData_->uvTransform; // UV変換行列
	}
}

void ParticleMaterial::SetPipeline(UINT rootParameterIndex) const
{
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();

	if (materialResource_)
	{
		commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, materialResource_->GetGPUVirtualAddress());
	}
}

