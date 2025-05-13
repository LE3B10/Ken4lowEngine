#include "Mesh.h"
#include "ResourceManager.h"
#include "DirectXCommon.h"
#include <span>


/// -------------------------------------------------------------
///				　		 初期化処理
/// -------------------------------------------------------------
void Mesh::Initialize(const std::vector<VertexData>& modelVertices, const std::vector<uint32_t>& modelIndices)
{
	vertices = modelVertices;
	indices = modelIndices;

	auto* device = DirectXCommon::GetInstance()->GetDevice();

	// 頂点バッファビューを作成する
	vertexResource = ResourceManager::CreateBufferResource(device, sizeof(VertexData) * vertices.size());
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress(); // リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * vertices.size()); // 使用するリソースのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData); // 1頂点あたりのサイズ

	// 書き込むためのアドレスを取得
	VertexData* vertexDataRaw = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataRaw));

	// GPU側の書き込み先をspanで安全に囲む
	std::span<VertexData> vertexSpan{ vertexDataRaw, vertices.size() };

	// modelDataの中身をGPUバッファへコピー
	std::copy(vertices.begin(), vertices.end(), vertexSpan.begin());

	// インデックスバッファビューを作成する
	indexResource = ResourceManager::CreateBufferResource(device, sizeof(uint32_t) * indices.size());
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress(); // リソースの先頭のアドレスから使う
	indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * indices.size()); // 使用するリソースのサイズ
	indexBufferView.Format = DXGI_FORMAT_R32_UINT; // インデックスのフォーマット

	// 書き込むためのアドレスを取得
	uint32_t* indexDataRaw = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexDataRaw));

	// GPU側の書き込み先をspanで安全に囲む
	std::span<uint32_t> indexSpan{ indexDataRaw, indices.size() };

	// modelDataの中身をGPUバッファへコピー
	std::copy(indices.begin(), indices.end(), indexSpan.begin());
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void Mesh::Draw()
{
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawIndexedInstanced(static_cast<UINT>(indices.size()), 1, 0, 0, 0);
}
