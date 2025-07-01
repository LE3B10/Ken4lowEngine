#include "ParticleMesh.h"
#include <DirectXCommon.h>
#include <ResourceManager.h>
#include <numbers>

void ParticleMesh::Initialize()
{
	// 6つの頂点を定義して四角形を表現
	modelData_.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });  // 左上
	modelData_.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelData_.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下

	modelData_.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下
	modelData_.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelData_.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右下

	CreateVertexBuffer();
}

void ParticleMesh::InitializeRing()
{
	const uint32_t kSubdivision = 32;
	const float innerRadius = 1.0f;
	const float outerRadius = 0.4f;

	for (uint32_t i = 0; i < kSubdivision; ++i)
	{
		float theta1 = 2.0f * std::numbers::pi_v<float> *i / kSubdivision;
		float theta2 = 2.0f * std::numbers::pi_v<float> *(i + 1) / kSubdivision;

		float u1 = static_cast<float>(i) / kSubdivision;
		float u2 = static_cast<float>(i + 1) / kSubdivision;

		Vector4 p0 = { std::cos(theta1) * innerRadius, std::sin(theta1) * innerRadius, 0.0f, 1.0f };
		Vector4 p1 = { std::cos(theta2) * innerRadius, std::sin(theta2) * innerRadius, 0.0f, 1.0f };
		Vector4 p2 = { std::cos(theta1) * outerRadius, std::sin(theta1) * outerRadius, 0.0f, 1.0f };
		Vector4 p3 = { std::cos(theta2) * outerRadius, std::sin(theta2) * outerRadius, 0.0f, 1.0f };

		Vector3 normal = { 0.0f, 0.0f, 1.0f };

		// 三角形1（内→外→外）
		modelData_.vertices.push_back({ p0, {u1, 1.0f}, normal }); // 内側→上
		modelData_.vertices.push_back({ p2, {u1, 0.0f}, normal }); // 外側→下
		modelData_.vertices.push_back({ p3, {u2, 0.0f}, normal });

		// 三角形2（内→外→内）						 
		modelData_.vertices.push_back({ p0, {u1, 1.0f}, normal });
		modelData_.vertices.push_back({ p3, {u2, 0.0f}, normal });
		modelData_.vertices.push_back({ p1, {u2, 1.0f}, normal });
	}

	// 頂点バッファを生成
	CreateVertexBuffer();
}

void ParticleMesh::InitializeCylinder()
{
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

	CreateVertexBuffer();
}

void ParticleMesh::InitializeStar()
{
	modelData_.vertices.clear();
	modelData_.indices.clear();

	const int numRays = 8;
	const float radius = 1.0f;

	for (int i = 0; i < numRays; ++i)
	{
		float angle = (float)i / numRays * 2.0f * std::numbers::pi_v<float>;

		Vector4 center = { 0.0f, 0.0f, 0.0f, 1.0f };
		Vector4 outer = { std::cos(angle) * radius, std::sin(angle) * radius, 0.0f, 1.0f };
		Vector4 right = { std::cos(angle + 0.1f) * radius * 0.5f, std::sin(angle + 0.1f) * radius * 0.5f, 0.0f, 1.0f };
		Vector3 normal = { 0.0f, 0.0f, 1.0f };

		uint32_t startIndex = static_cast<uint32_t>(modelData_.vertices.size());

		modelData_.vertices.push_back({ center, { 0.5f, 0.5f }, normal });
		modelData_.vertices.push_back({ outer,  { 1.0f, 0.5f }, normal });
		modelData_.vertices.push_back({ right,  { 0.75f, 1.0f }, normal });

		modelData_.indices.push_back(startIndex);
		modelData_.indices.push_back(startIndex + 1);
		modelData_.indices.push_back(startIndex + 2);
	}

	CreateVertexBuffer();
}

void ParticleMesh::InitializeSmoke()
{
	modelData_.vertices.clear();
	modelData_.indices.clear();

	const float size = 1.0f;
	Vector3 normal = { 0.0f, 0.0f, 1.0f };

	// 左下三角形
	modelData_.vertices.push_back({ { -size, -size, 0.0f, 1.0f }, { 0.0f, 1.0f }, normal }); // 0 左下
	modelData_.vertices.push_back({ { -size,  size, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal }); // 1 左上
	modelData_.vertices.push_back({ {  size, -size, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal }); // 2 右下

	// 右上三角形
	modelData_.vertices.push_back({ {  size, -size, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal }); // 3 右下
	modelData_.vertices.push_back({ { -size,  size, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal }); // 4 左上
	modelData_.vertices.push_back({ {  size,  size, 0.0f, 1.0f }, { 1.0f, 0.0f }, normal }); // 5 右上

	// インデックス（頂点順そのまま）
	modelData_.indices = {
		0, 1, 2,
		3, 4, 5
	};

	CreateVertexBuffer();
}

void ParticleMesh::Draw(UINT num)
{
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
	commandList->DrawInstanced(UINT(modelData_.vertices.size()), num, 0, 0);
}

void ParticleMesh::CreateVertexBuffer()
{
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
	vertexResource_ = ResourceManager::CreateBufferResource(device, sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}
