#pragma once
// Wireframe内部で使う頂点/バッファ関連の型定義。

#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

#include <cstdint>

namespace Ken4lowEngine
{
	// ワイヤー描画の頂点データ。
	struct WireframeVertexData
	{
		Vector3 position;
		Vector4 color;
	};

	// ワイヤー描画用の座標変換行列。
	struct WireframeTransformationMatrix
	{
		Matrix4x4 WVP;
	};

	// 三角形描画用の頂点バッファ情報。
	struct WireframeTriangleData
	{
		WireframeVertexData* vertexData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	};

	// 矩形描画用の頂点/インデックスバッファ情報。
	struct WireframeBoxData
	{
		WireframeVertexData* vertexData = nullptr;
		uint32_t* indexData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	};

	// 線分描画用の頂点バッファ情報。
	struct WireframeLineData
	{
		WireframeVertexData* vertexData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	};

	// 球体デバッグ形状用の基本データ。
	struct WireframeSphereData
	{
		Vector3 center;
		float radius;
	};
}
