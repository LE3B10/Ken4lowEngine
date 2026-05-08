#include "DisintegrationRenderer.h"

#include "Wireframe.h"

void DisintegrationRenderer::Draw(const std::vector<DisintegrationParticle>& particles) const
{
	auto* wireframe = K4E::Wireframe::GetInstance();
	if (!wireframe) { return; }

	for (const auto& particle : particles)
	{
		if (!particle.alive || particle.alpha <= 0.0f) { continue; }

		K4E::Vector4 color = particle.color;
		color.w *= particle.alpha;

		const float s = particle.size * 0.5f;
		const K4E::Vector3& p = particle.position;

		// 点群らしい細かな塵に見せるため、立方体ではなく短い交差線だけを描く。
		wireframe->DrawLine({ p.x - s, p.y, p.z }, { p.x + s, p.y, p.z }, color);
		wireframe->DrawLine({ p.x, p.y - s, p.z }, { p.x, p.y + s, p.z }, color);
	}
}
