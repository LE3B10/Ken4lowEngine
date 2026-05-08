#include "DisintegrationEmitter.h"

#include <algorithm>
#include <cmath>

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

	K4E::Vector4 ClampColor(const K4E::Vector4& color)
	{
		return {
			std::clamp(color.x, 0.0f, 1.0f),
			std::clamp(color.y, 0.0f, 1.0f),
			std::clamp(color.z, 0.0f, 1.0f),
			std::clamp(color.w, 0.0f, 1.0f),
		};
	}
}

std::vector<DisintegrationParticle> DisintegrationEmitter::EmitFromModel(
	const K4E::ModelData& modelData,
	const K4E::Matrix4x4& worldMatrix,
	const Settings& settings)
{
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
				TriangleSample tri{};
				tri.a = a;
				tri.b = b;
				tri.c = c;
				tri.normal = K4E::Vector3::NormalizeSafe(K4E::Vector3::Cross(b - a, c - a), { 0.0f, 1.0f, 0.0f });
				tri.cumulativeArea = totalArea;
				triangles.push_back(tri);
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

				// Boundsではなく三角形表面の面積重みによって初期粒子位置を選ぶ。
				totalArea += area;
				TriangleSample tri{};
				tri.a = a;
				tri.b = b;
				tri.c = c;
				tri.normal = K4E::Vector3::NormalizeSafe(K4E::Vector3::Cross(b - a, c - a), { 0.0f, 1.0f, 0.0f });
				tri.cumulativeArea = totalArea;
				triangles.push_back(tri);
			}
		}
	}

	std::vector<DisintegrationParticle> particles;
	particles.reserve(static_cast<size_t>(std::max(0, settings.particleCount)));
	if ((triangles.empty() && vertices.empty()) || settings.particleCount <= 0) { return particles; }

	const K4E::Vector3 center = worldMatrix.GetTranslation();

	for (int i = 0; i < settings.particleCount; ++i)
	{
		K4E::Vector3 local{};
		K4E::Vector3 localNormal{ 0.0f, 1.0f, 0.0f };

		if (!triangles.empty())
		{
			const float areaPick = RandomRange(0.0f, totalArea);
			auto it = std::lower_bound(
				triangles.begin(), triangles.end(), areaPick,
				[](const TriangleSample& tri, float value) { return tri.cumulativeArea < value; });
			if (it == triangles.end()) { it = triangles.end() - 1; }

			float u = Random01();
			float v = Random01();
			if (u + v > 1.0f)
			{
				u = 1.0f - u;
				v = 1.0f - v;
			}

			local = it->a + (it->b - it->a) * u + (it->c - it->a) * v;
			localNormal = it->normal;
		}
		else
		{
			const size_t sampleIndex = std::min(static_cast<size_t>(Random01() * static_cast<float>(vertices.size())), vertices.size() - 1);
			const VertexSample& sample = vertices[sampleIndex];
			local = sample.position + RandomUnitVector() * RandomRange(0.0f, settings.particleSize * 1.5f);
			localNormal = sample.normal;
		}

		const K4E::Vector3 world = K4E::Vector3::Transform(local, worldMatrix);
		const K4E::Vector3 worldNormal = K4E::Vector3::NormalizeSafe(TransformDirection(localNormal, worldMatrix), { 0.0f, 1.0f, 0.0f });
		K4E::Vector3 outward = K4E::Vector3::NormalizeSafe(world - center, worldNormal);
		outward = K4E::Vector3::NormalizeSafe(outward * 0.70f + worldNormal * 0.25f + RandomUnitVector() * 0.20f, worldNormal);

		const float brightness = RandomRange(1.0f - settings.colorVariation, 1.0f + settings.colorVariation);
		K4E::Vector4 color = ClampColor({
			settings.baseColor.x * brightness + RandomRange(-settings.colorVariation, settings.colorVariation) * 0.25f,
			settings.baseColor.y * brightness + RandomRange(-settings.colorVariation, settings.colorVariation) * 0.25f,
			settings.baseColor.z * brightness + RandomRange(-settings.colorVariation, settings.colorVariation) * 0.25f,
			settings.baseColor.w,
		});

		DisintegrationParticle particle{};
		particle.initialPosition = world;
		particle.position = world;
		particle.outward = outward;
		particle.renderAxis = RandomUnitVector();
		particle.velocity = outward * RandomRange(settings.spreadPower * 0.15f, settings.spreadPower * 0.55f);
		particle.velocity.y += RandomRange(-settings.upwardPower * 0.25f, settings.upwardPower);
		particle.life = RandomRange(settings.lifeTime * 0.85f, settings.lifeTime * 1.20f);
		particle.startDelay = RandomRange(0.0f, settings.startDelay);
		particle.size = settings.particleSize * RandomRange(0.45f, 1.05f);
		particle.color = color;
		particle.alpha = 1.0f;
		particle.alive = true;
		particles.push_back(particle);
	}

	return particles;
}

float DisintegrationEmitter::Random01()
{
	return unitDist_(rng_);
}

float DisintegrationEmitter::RandomRange(float minValue, float maxValue)
{
	return minValue + (maxValue - minValue) * Random01();
}

K4E::Vector3 DisintegrationEmitter::RandomUnitVector()
{
	K4E::Vector3 v{};
	do
	{
		v = { RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f) };
	} while (K4E::Vector3::LengthSquared(v) <= 0.000001f);

	return K4E::Vector3::Normalize(v);
}
