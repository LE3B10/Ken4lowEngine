#pragma once
// Wireframeの最大描画数などの設定値。

#include <cstdint>

namespace Ken4lowEngine
{
	inline constexpr uint32_t kWireframeTriangleMaxCount = 30096;
	inline constexpr uint32_t kWireframeTriangleVertexCount = 3;

	inline constexpr uint32_t kWireframeBoxMaxCount = 30096;
	inline constexpr uint32_t kWireframeBoxIndexCount = 6;
	inline constexpr uint32_t kWireframeBoxVertexCount = 4;

	inline constexpr uint32_t kWireframeLineMaxCount = 1000000;
	inline constexpr uint32_t kWireframeLineVertexCount = 2;

	// AABBは8頂点・12辺の共有メッシュを使い、ここで指定した個数まで一括描画する。
	inline constexpr uint32_t kWireframeAABBVertexCount = 8;
	inline constexpr uint32_t kWireframeAABBIndexCount = 24;
	inline constexpr uint32_t kWireframeAABBMaxInstanceCount = 100000;
}
