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
		const K4E::Vector3 axis = K4E::Vector3::NormalizeSafe(particle.renderAxis, { 0.0f, 1.0f, 0.0f }) * s;

		// 軸固定の十字線をやめ、極小の1ストロークだけにして砂・灰の点群として見せる。
		wireframe->DrawLine(p - axis, p + axis, color);
	}
}
