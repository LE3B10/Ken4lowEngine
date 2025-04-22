#include "Mesh.h"
#include "ResourceManager.h"
#include "DirectXCommon.h"


/// -------------------------------------------------------------
///				　		 初期化処理
/// -------------------------------------------------------------
void Mesh::Initialize(const std::vector<VertexData>& modelVertices)
{
	vertices = modelVertices;

	// バッファリソース作成
	auto device = DirectXCommon::GetInstance()->GetDevice();
	vertexResource = ResourceManager::CreateBufferResource(device, sizeof(VertexData) * vertices.size());

	// マップして書き込む
	VertexData* mapped = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	std::memcpy(mapped, vertices.data(), sizeof(VertexData) * vertices.size());
	vertexResource->Unmap(0, nullptr);

	// ビュー設定
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void Mesh::Draw()
{
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandList();

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
}
