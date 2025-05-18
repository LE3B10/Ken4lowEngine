#include "ParticleMesh.h"
#include <DirectXCommon.h>
#include <ResourceManager.h>
#include <numbers>

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

void ParticleMesh::CreateVertexData()
{
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

	const uint32_t kSubdivision = 32;
	const float outerRadius = 1.0f;
	const float innerRadius = 0.4f;

	for (uint32_t i = 0; i < kSubdivision; ++i) {
		float theta1 = 2.0f * std::numbers::pi_v<float> *i / kSubdivision;
		float theta2 = 2.0f * std::numbers::pi_v<float> *(i + 1) / kSubdivision;

		float u1 = static_cast<float>(i) / kSubdivision;
		float u2 = static_cast<float>(i + 1) / kSubdivision;

		// 修正後（巻き方向を反転させる：反時計回り → 時計回り）
		Vector4 p0 = { cos(theta1) * innerRadius, sin(theta1) * innerRadius, 0.0f, 1.0f };
		Vector4 p1 = { cos(theta2) * innerRadius, sin(theta2) * innerRadius, 0.0f, 1.0f };
		Vector4 p2 = { cos(theta1) * outerRadius, sin(theta1) * outerRadius, 0.0f, 1.0f };
		Vector4 p3 = { cos(theta2) * outerRadius, sin(theta2) * outerRadius, 0.0f, 1.0f };

		// ここからの三角形構成を「逆順」にします（裏向きに）

		// 三角形1（p3 → p2 → p0）
		modelData_.vertices.push_back({ p3, {u2, 0.0f}, {0, 0, -1} });
		modelData_.vertices.push_back({ p2, {u1, 0.0f}, {0, 0, -1} });
		modelData_.vertices.push_back({ p0, {u1, 1.0f}, {0, 0, -1} });

		// 三角形2（p1 → p3 → p0）
		modelData_.vertices.push_back({ p1, {u2, 1.0f}, {0, 0, -1} });
		modelData_.vertices.push_back({ p3, {u2, 0.0f}, {0, 0, -1} });
		modelData_.vertices.push_back({ p0, {u1, 1.0f}, {0, 0, -1} });
	}

	// 以下は既存のコード（Resource作成、VBV設定）
	vertexResource_ = ResourceManager::CreateBufferResource(device, sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

void ParticleMesh::InitializeCylinder()
{
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

	const uint32_t kCylinderDivide = 32;
	const float kTopRadius = 1.0f;
	const float kBottomRadius = 1.0f;
	const float kHeight = 3.0f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kCylinderDivide);

	for (uint32_t index = 0; index < kCylinderDivide; ++index) {
		float sin0 = std::sin(index * radianPerDivide);
		float cos0 = std::cos(index * radianPerDivide);
		float sin1 = std::sin((index + 1) * radianPerDivide);
		float cos1 = std::cos((index + 1) * radianPerDivide);

		float u0 = float(index) / kCylinderDivide;
		float u1 = float(index + 1) / kCylinderDivide;

		// 頂点ポジションとUV、法線（X,Z方向の外向き）
		Vector4 top0 = { -sin0 * kTopRadius, kHeight, cos0 * kTopRadius, 1.0f };
		Vector4 top1 = { -sin1 * kTopRadius, kHeight, cos1 * kTopRadius, 1.0f };
		Vector4 bottom0 = { -sin0 * kBottomRadius, 0.0f, cos0 * kBottomRadius, 1.0f };
		Vector4 bottom1 = { -sin1 * kBottomRadius, 0.0f, cos1 * kBottomRadius, 1.0f };

		Vector3 normal0 = { -sin0, 0.0f, cos0 };
		Vector3 normal1 = { -sin1, 0.0f, cos1 };

		// 三角形1（top0 → bottom0 → bottom1）
		modelData_.vertices.push_back({ top0, {u0, 0.0f}, normal0 });
		modelData_.vertices.push_back({ bottom0, {u0, 1.0f}, normal0 });
		modelData_.vertices.push_back({ bottom1, {u1, 1.0f}, normal1 });

		// 三角形2（top0 → bottom1 → top1）
		modelData_.vertices.push_back({ top0, {u0, 0.0f}, normal0 });
		modelData_.vertices.push_back({ bottom1, {u1, 1.0f}, normal1 });
		modelData_.vertices.push_back({ top1, {u1, 0.0f}, normal1 });
	}

	vertexResource_ = ResourceManager::CreateBufferResource(device, sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());

}

void ParticleMesh::Draw(UINT num)
{
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
	commandList->DrawInstanced(UINT(modelData_.vertices.size()), num, 0, 0);
}
