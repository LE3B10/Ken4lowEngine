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

	void Wireframe::CalcSphereVertexData()
	{
		const float pi = std::numbers::pi_v<float>;
		const uint32_t kSubdivision = 8; // 分割数
		const float kLonEvery = 2.0f * pi / float(kSubdivision); // 経度の1分割の角度
		const float kLatEvery = pi / float(kSubdivision); // 緯度の1分割の角度
		// 緯度方向
		for (uint32_t latIndex = 0; latIndex < kSubdivision; latIndex++)
		{
			float lat = -pi / 2.0f + kLatEvery * float(latIndex);
			// 経度方向
			for (uint32_t lonIndex = 0; lonIndex < kSubdivision; lonIndex++)
			{
				float lon = kLonEvery * float(lonIndex);
				// 球の表面上の点を求める
				Vector3 a, b, c;
				a.x = 0.0f + 1.0f * cos(lat) * cos(lon);
				a.y = 0.0f + 1.0f * sin(lat);
				a.z = 0.0f + 1.0f * cos(lat) * sin(lon);
				b.x = 0.0f + 1.0f * cos(lat + kLatEvery) * cos(lon);
				b.y = 0.0f + 1.0f * sin(lat + kLatEvery);
				b.z = 0.0f + 1.0f * cos(lat + kLatEvery) * sin(lon);
				c.x = 0.0f + 1.0f * cos(lat) * cos(lon + kLonEvery);
				c.y = 0.0f + 1.0f * sin(lat);
				c.z = 0.0f + 1.0f * cos(lat) * sin(lon + kLonEvery);
				// 座標を保存
				spheres_.push_back(a);
				spheres_.push_back(b);
				spheres_.push_back(c);
			}
		}
	}

} // namespace Ken4lowEngine
