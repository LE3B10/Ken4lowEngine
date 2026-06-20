// Wireframe の描画バッファ生成と事前計算をまとめる。
#include "Wireframe.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include <array>
#include <cmath>
#include <numbers>

namespace Ken4lowEngine
{

	void Wireframe::CreateTriangleVertexData(WireframeTriangleData* triangleData)
	{
		UINT vertexBufferSize = sizeof(WireframeVertexData) * kWireframeTriangleVertexCount * kWireframeTriangleMaxCount;
		// 頂点リソースを生成
		triangleData->vertexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), vertexBufferSize);
		// 頂点バッファビューを生成
		triangleData->vertexBufferView.BufferLocation = triangleData->vertexBuffer->GetGPUVirtualAddress();
		triangleData->vertexBufferView.SizeInBytes = vertexBufferSize;
		triangleData->vertexBufferView.StrideInBytes = sizeof(WireframeVertexData);
		// 頂点リソースをマップ
		triangleData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&triangleData->vertexData));
	}

	void Wireframe::CreateBoxVertexData(WireframeBoxData* boxData)
	{
		UINT vertexBufferSize = sizeof(WireframeVertexData) * kWireframeBoxVertexCount * kWireframeBoxMaxCount;
		UINT indexBufferSize = sizeof(uint32_t) * kWireframeBoxIndexCount * kWireframeBoxMaxCount;
		// 頂点リソースを生成
		boxData->vertexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), vertexBufferSize);
		// 頂点バッファビューを生成
		boxData->vertexBufferView.BufferLocation = boxData->vertexBuffer->GetGPUVirtualAddress();
		boxData->vertexBufferView.SizeInBytes = vertexBufferSize;
		boxData->vertexBufferView.StrideInBytes = sizeof(WireframeVertexData);
		// 頂点リソースをマップ
		boxData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&boxData->vertexData));
		// インデックスリソースを生成
		boxData->indexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), indexBufferSize);
		// インデックスバッファビューを生成
		boxData->indexBufferView.BufferLocation = boxData->indexBuffer->GetGPUVirtualAddress();
		boxData->indexBufferView.SizeInBytes = indexBufferSize;
		boxData->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		// 頂点リソースをマップ
		boxData->indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&boxData->indexData));
	}

	void Wireframe::CreateLineVertexData(WireframeLineData* lineData)
	{
		UINT vertexBufferSize = sizeof(WireframeVertexData) * kWireframeLineVertexCount * kWireframeLineMaxCount;
		// 頂点バッファビューを作成
		lineData->vertexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), vertexBufferSize);
		// 頂点バッファビューを生成
		lineData_->vertexBufferView.BufferLocation = lineData_->vertexBuffer->GetGPUVirtualAddress();
		lineData_->vertexBufferView.SizeInBytes = vertexBufferSize;
		lineData_->vertexBufferView.StrideInBytes = sizeof(WireframeVertexData);
		// 頂点リソースをマップ
		lineData_->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&lineData_->vertexData));
	}

	void Wireframe::CreateBoxWireInstancedData(WireframeBoxInstancedData* boxWireData)
	{
		// [-0.5, 0.5] の単位キューブを全AABB / OBBで共有し、形状固有の情報はinstanceBufferだけへ書く。
		const std::array<WireframeBoxVertexData, kWireframeBoxWireVertexCount> kVertices = {
			WireframeBoxVertexData{ { -0.5f, -0.5f, -0.5f } },
			WireframeBoxVertexData{ {  0.5f, -0.5f, -0.5f } },
			WireframeBoxVertexData{ {  0.5f,  0.5f, -0.5f } },
			WireframeBoxVertexData{ { -0.5f,  0.5f, -0.5f } },
			WireframeBoxVertexData{ { -0.5f, -0.5f,  0.5f } },
			WireframeBoxVertexData{ {  0.5f, -0.5f,  0.5f } },
			WireframeBoxVertexData{ {  0.5f,  0.5f,  0.5f } },
			WireframeBoxVertexData{ { -0.5f,  0.5f,  0.5f } },
		};
		constexpr std::array<uint32_t, kWireframeBoxWireIndexCount> kIndices = {
			0, 1, 1, 2, 2, 3, 3, 0,
			4, 5, 5, 6, 6, 7, 7, 4,
			0, 4, 1, 5, 2, 6, 3, 7,
		};

		const UINT baseVertexBufferSize = sizeof(WireframeBoxVertexData) * kWireframeBoxWireVertexCount;
		const UINT indexBufferSize = sizeof(uint32_t) * kWireframeBoxWireIndexCount;
		const UINT instanceBufferSize = sizeof(WireframeBoxInstanceData) * kWireframeBoxWireMaxInstanceCount;

		boxWireData->baseVertexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), baseVertexBufferSize);
		boxWireData->baseVertexBufferView = {
			boxWireData->baseVertexBuffer->GetGPUVirtualAddress(), baseVertexBufferSize, sizeof(WireframeBoxVertexData)
		};
		// AABB / OBB共有単位キューブの頂点バッファをMapして初期データを書き込む。
		boxWireData->baseVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&boxWireData->baseVertexData));
		for (uint32_t i = 0; i < kWireframeBoxWireVertexCount; ++i) {
			boxWireData->baseVertexData[i] = kVertices[i];
		}

		boxWireData->indexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), indexBufferSize);
		boxWireData->indexBufferView.BufferLocation = boxWireData->indexBuffer->GetGPUVirtualAddress();
		boxWireData->indexBufferView.SizeInBytes = indexBufferSize;
		boxWireData->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		// AABB / OBBの12辺を表す24 indexの共有バッファをMapして初期データを書き込む。
		boxWireData->indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&boxWireData->indexData));
		for (uint32_t i = 0; i < kWireframeBoxWireIndexCount; ++i) {
			boxWireData->indexData[i] = kIndices[i];
		}

		boxWireData->instanceBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), instanceBufferSize);
		boxWireData->instanceBufferView = {
			boxWireData->instanceBuffer->GetGPUVirtualAddress(), instanceBufferSize, sizeof(WireframeBoxInstanceData)
		};
		// AABB / OBBごとのworld行列と色を毎フレーム追記する共通インスタンスバッファを永続Mapする。
		boxWireData->instanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&boxWireData->instanceData));
	}

	void Wireframe::CreateTransformationMatrix()
	{
		// 座標変換リソースを生成
		transformationMatrixBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(WireframeTransformationMatrix));
		// 座標変換行列リソースをマップ
		transformationMatrixBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
		transformationMatrixData_->WVP = Matrix4x4::Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix());
	}

	void Wireframe::CreateSphereInstancedData(WireframeSphereInstancedData* sphereData)
	{
		// Sphereの基本リングメッシュを初期化時に1回だけ作り、各Sphereはインスタンスデータで描画する。
		const UINT baseVertexBufferSize = sizeof(WireframeSphereVertexData) * kWireframeSphereBaseVertexCount;
		const UINT indexBufferSize = sizeof(uint32_t) * kWireframeSphereIndexCount;
		const UINT instanceBufferSize = sizeof(WireframeSphereInstanceData) * kWireframeSphereMaxInstanceCount;

		sphereData->baseVertexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), baseVertexBufferSize);
		sphereData->baseVertexBufferView = {
			sphereData->baseVertexBuffer->GetGPUVirtualAddress(), baseVertexBufferSize, sizeof(WireframeSphereVertexData)
		};
		// SphereのXY / XZ / YZ共有リング頂点バッファをMapして初期データを書き込む。
		sphereData->baseVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&sphereData->baseVertexData));

		const float angleStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(kWireframeSphereRingSegmentCount);
		for (uint32_t segment = 0; segment < kWireframeSphereRingSegmentCount; ++segment)
		{
			const float angle = angleStep * static_cast<float>(segment);
			const float cosine = std::cos(angle);
			const float sine = std::sin(angle);
			sphereData->baseVertexData[segment].position = { cosine, sine, 0.0f }; // XYリング
			sphereData->baseVertexData[kWireframeSphereRingSegmentCount + segment].position = { cosine, 0.0f, sine }; // XZリング
			sphereData->baseVertexData[kWireframeSphereRingSegmentCount * 2 + segment].position = { 0.0f, cosine, sine }; // YZリング
		}

		sphereData->indexBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), indexBufferSize);
		sphereData->indexBufferView.BufferLocation = sphereData->indexBuffer->GetGPUVirtualAddress();
		sphereData->indexBufferView.SizeInBytes = indexBufferSize;
		sphereData->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		// 各リングの隣接頂点をLINELISTで結ぶSphere共有indexバッファをMapする。
		sphereData->indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&sphereData->indexData));
		for (uint32_t ring = 0; ring < kWireframeSphereRingCount; ++ring)
		{
			const uint32_t vertexOffset = ring * kWireframeSphereRingSegmentCount;
			const uint32_t indexOffset = ring * kWireframeSphereRingSegmentCount * 2;
			for (uint32_t segment = 0; segment < kWireframeSphereRingSegmentCount; ++segment)
			{
				sphereData->indexData[indexOffset + segment * 2] = vertexOffset + segment;
				sphereData->indexData[indexOffset + segment * 2 + 1] = vertexOffset + (segment + 1) % kWireframeSphereRingSegmentCount;
			}
		}

		sphereData->instanceBuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), instanceBufferSize);
		sphereData->instanceBufferView = {
			sphereData->instanceBuffer->GetGPUVirtualAddress(), instanceBufferSize, sizeof(WireframeSphereInstanceData)
		};
		// Sphereごとのworld行列と色を毎フレーム追記するインスタンスバッファを永続Mapする。
		sphereData->instanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&sphereData->instanceData));
	}

} // namespace Ken4lowEngine
