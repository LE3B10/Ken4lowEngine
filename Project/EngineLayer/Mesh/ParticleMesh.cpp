#include "ParticleMesh.h"
#include <DirectXCommon.h>
#include <ResourceManager.h>

void ParticleMesh::Initialize()
{
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

	// 6つの頂点を定義して四角形を表現
	modelData_.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });  // 左上
	modelData_.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelData_.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下

	modelData_.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下
	modelData_.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelData_.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右下

	// バッファリソースの作成
	vertexResource_ = ResourceManager::CreateBufferResource(device, sizeof(VertexData) * modelData_.vertices.size());

	// 頂点バッファビュー（VBV）作成
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();									 // リソースの先頭のアドレスから使う
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());	 // 使用するリソースのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);														 // 1頂点あたりのサイズ

	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));										 // 書き込むためのアドレスを取得

	// モデルデータの頂点データをコピー
	std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

void ParticleMesh::Draw(UINT num)
{
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandList();
	commandList->DrawInstanced(UINT(modelData_.vertices.size()), num, 0, 0);
}
