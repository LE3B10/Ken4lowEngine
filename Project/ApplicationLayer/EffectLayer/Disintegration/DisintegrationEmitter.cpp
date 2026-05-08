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
}

std::vector<DisintegrationParticle> DisintegrationEmitter::EmitFromModel(
	const K4E::ModelData& modelData,
	const K4E::Matrix4x4& worldMatrix,
	const Settings& settings)
{
	std::vector<TriangleSample> triangles;
	triangles.reserve(1024);

	float totalArea = 0.0f;
	for (const auto& subMesh : modelData.subMeshes)
	{
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
	}

	std::vector<DisintegrationParticle> particles;
	particles.reserve(static_cast<size_t>(std::max(0, settings.particleCount)));
	if (triangles.empty() || settings.particleCount <= 0) { return particles; }

	const K4E::Vector3 center = worldMatrix.GetTranslation();

	for (int i = 0; i < settings.particleCount; ++i)
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

		const K4E::Vector3 local = it->a + (it->b - it->a) * u + (it->c - it->a) * v;
		const K4E::Vector3 world = K4E::Vector3::Transform(local, worldMatrix);
		K4E::Vector3 outward = K4E::Vector3::NormalizeSafe(world - center, it->normal);
		outward = K4E::Vector3::NormalizeSafe(outward + RandomUnitVector() * 0.35f, it->normal);

		DisintegrationParticle particle{};
		particle.initialPosition = world;
		particle.position = world;
		particle.outward = outward;
		particle.velocity = outward * RandomRange(settings.spreadPower * 0.35f, settings.spreadPower);
		particle.velocity.y += RandomRange(-settings.upwardPower * 0.35f, settings.upwardPower);
		particle.life = RandomRange(settings.lifeTime * 0.75f, settings.lifeTime * 1.15f);
		particle.startDelay = RandomRange(0.0f, settings.startDelay);
		particle.size = settings.particleSize * RandomRange(0.55f, 1.35f);
		particle.color = { RandomRange(0.55f, 0.78f), RandomRange(0.52f, 0.68f), RandomRange(0.46f, 0.58f), 1.0f };
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
