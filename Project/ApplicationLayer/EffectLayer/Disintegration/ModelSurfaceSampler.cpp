#define NOMINMAX
#include "ModelSurfaceSampler.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
	K4E::Vector3 ToVector3(const K4E::Vector4& v)
	{
		return { v.x, v.y, v.z };
	}

	float TriangleArea(const K4E::Vector3& a, const K4E::Vector3& b, const K4E::Vector3& c)
	{
		return K4E::Vector3::Length(K4E::Vector3::Cross(b - a, c - a)) * 0.5f;
	}

	K4E::Vector3 TransformDirection(const K4E::Vector3& direction, const K4E::Matrix4x4& matrix)
	{
		return {
			direction.x * matrix.m[0][0] + direction.y * matrix.m[1][0] + direction.z * matrix.m[2][0],
			direction.x * matrix.m[0][1] + direction.y * matrix.m[1][1] + direction.z * matrix.m[2][1],
			direction.x * matrix.m[0][2] + direction.y * matrix.m[1][2] + direction.z * matrix.m[2][2],
		};
	}
}

std::vector<DisintegrationSamplePoint> ModelSurfaceSampler::SampleFromModel(
	const K4E::ModelData& modelData,
	const K4E::Matrix4x4& worldMatrix,
	int sampleCount,
	bool surfaceSampling,
	float vertexJitterRadius,
	DisintegrationPlacementMode placementMode,
	uint32_t placementSeed)
{
	rng_.seed(placementSeed);
	std::vector<TriangleSample> triangles;
	std::vector<VertexSample> vertices;
	triangles.reserve(1024);
	vertices.reserve(1024);

	float totalArea = 0.0f;
	for (const auto& subMesh : modelData.subMeshes)
	{
		for (const auto& vertex : subMesh.vertices)
		{
			vertices.push_back({ ToVector3(vertex.position), K4E::Vector3::NormalizeSafe(vertex.normal, { 0.0f, 1.0f, 0.0f }) });
		}

		if (subMesh.indices.size() >= 3)
		{
			for (size_t i = 0; i + 2 < subMesh.indices.size(); i += 3)
			{
				const uint32_t ia = subMesh.indices[i + 0];
				const uint32_t ib = subMesh.indices[i + 1];
				const uint32_t ic = subMesh.indices[i + 2];
				if (ia >= subMesh.vertices.size() || ib >= subMesh.vertices.size() || ic >= subMesh.vertices.size()) { continue; }

				const K4E::Vector3 a = ToVector3(subMesh.vertices[ia].position);
				const K4E::Vector3 b = ToVector3(subMesh.vertices[ib].position);
				const K4E::Vector3 c = ToVector3(subMesh.vertices[ic].position);
				const float area = TriangleArea(a, b, c);
				if (area <= 0.000001f) { continue; }

				totalArea += area;
				triangles.push_back({ a, b, c, K4E::Vector3::NormalizeSafe(K4E::Vector3::Cross(b - a, c - a), { 0.0f, 1.0f, 0.0f }), totalArea });
			}
		}
		else if (subMesh.vertices.size() >= 3)
		{
			for (size_t i = 0; i + 2 < subMesh.vertices.size(); i += 3)
			{
				const K4E::Vector3 a = ToVector3(subMesh.vertices[i + 0].position);
				const K4E::Vector3 b = ToVector3(subMesh.vertices[i + 1].position);
				const K4E::Vector3 c = ToVector3(subMesh.vertices[i + 2].position);
				const float area = TriangleArea(a, b, c);
				if (area <= 0.000001f) { continue; }

				// Boundsではなく三角形表面の面積重みによってモデル形状を復元する点を選ぶ。
				totalArea += area;
				triangles.push_back({ a, b, c, K4E::Vector3::NormalizeSafe(K4E::Vector3::Cross(b - a, c - a), { 0.0f, 1.0f, 0.0f }), totalArea });
			}
		}
	}

	std::vector<DisintegrationSamplePoint> samples;
	samples.reserve(static_cast<size_t>(std::max(0, sampleCount)));
	if ((triangles.empty() && vertices.empty()) || sampleCount <= 0) { return samples; }

	const bool useUniformSurface = placementMode == DisintegrationPlacementMode::UniformSurface && !triangles.empty();
	const bool useTriangleSurface = (surfaceSampling || useUniformSurface) && !triangles.empty();
	for (int i = 0; i < sampleCount; ++i)
	{
		K4E::Vector3 local{};
		K4E::Vector3 localNormal{ 0.0f, 1.0f, 0.0f };

		if (useTriangleSurface)
		{
			const float areaPick = useUniformSurface
				? totalArea * (static_cast<float>(i) + 0.5f) / static_cast<float>(sampleCount)
				: RandomRange(0.0f, totalArea);
			auto it = std::lower_bound(
				triangles.begin(), triangles.end(), areaPick,
				[](const TriangleSample& tri, float value) { return tri.cumulativeArea < value; });
			if (it == triangles.end()) { it = triangles.end() - 1; }

			float u = Random01();
			float v = Random01();
			if (useUniformSurface)
			{
				// 面積で層化した三角形上に低差異点を置き、毎回同じ均一表面配置にする。
				u = (static_cast<float>((static_cast<uint32_t>(i) * 2654435761u + placementSeed) & 0x00FFFFFFu) + 0.5f) / static_cast<float>(0x01000000u);
				v = VanDerCorput(static_cast<uint32_t>(i) + (placementSeed | 1u));
				const float sqrtU = std::sqrt(u);
				const float baryB = sqrtU * (1.0f - v);
				const float baryC = sqrtU * v;
				local = it->a + (it->b - it->a) * baryB + (it->c - it->a) * baryC;
			}
			else
			{
				if (u + v > 1.0f)
				{
					u = 1.0f - u;
					v = 1.0f - v;
				}
				local = it->a + (it->b - it->a) * u + (it->c - it->a) * v;
			}
			localNormal = it->normal;
		}
		else
		{
			const bool useUniformVertexFallback = placementMode == DisintegrationPlacementMode::UniformSurface;
			const size_t sampleIndex = useUniformVertexFallback
				? (static_cast<size_t>(i) * vertices.size()) / static_cast<size_t>(sampleCount)
				: std::min(static_cast<size_t>(Random01() * static_cast<float>(vertices.size())), vertices.size() - 1);
			const VertexSample& sample = vertices[std::min(sampleIndex, vertices.size() - 1)];
			local = useUniformVertexFallback
				? sample.position
				: sample.position + RandomUnitVector() * RandomRange(0.0f, vertexJitterRadius);
			localNormal = sample.normal;
		}

		DisintegrationSamplePoint sample{};
		sample.position = K4E::Vector3::Transform(local, worldMatrix);
		sample.normal = K4E::Vector3::NormalizeSafe(TransformDirection(localNormal, worldMatrix), { 0.0f, 1.0f, 0.0f });
		samples.push_back(sample);
	}

	return samples;
}

float ModelSurfaceSampler::Random01()
{
	return unitDist_(rng_);
}

float ModelSurfaceSampler::RandomRange(float minValue, float maxValue)
{
	return minValue + (maxValue - minValue) * Random01();
}

K4E::Vector3 ModelSurfaceSampler::RandomUnitVector()
{
	K4E::Vector3 v{};
	do
	{
		v = { RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f) };
	} while (K4E::Vector3::LengthSquared(v) <= 0.000001f);

	return K4E::Vector3::Normalize(v);
}


float ModelSurfaceSampler::VanDerCorput(uint32_t value) const
{
	value = (value << 16u) | (value >> 16u);
	value = ((value & 0x55555555u) << 1u) | ((value & 0xAAAAAAAAu) >> 1u);
	value = ((value & 0x33333333u) << 2u) | ((value & 0xCCCCCCCCu) >> 2u);
	value = ((value & 0x0F0F0F0Fu) << 4u) | ((value & 0xF0F0F0F0u) >> 4u);
	value = ((value & 0x00FF00FFu) << 8u) | ((value & 0xFF00FF00u) >> 8u);
	return static_cast<float>(value) * 2.3283064365386963e-10f;
}
