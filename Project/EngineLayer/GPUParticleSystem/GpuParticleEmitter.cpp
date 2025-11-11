#include "GpuParticleEmitter.h"
#include "GpuParticleBuffers.h"

/// -------------------------------------------------------------
///			　指定位置でパーティクルを出したいときに呼ぶ
/// -------------------------------------------------------------
void GpuParticleEmitter::Emit(GpuParticleBuffers* buffers, const Vector3& position) const
{
	(void)buffers;
	(void)position;
    //EmitterCBData data{};
    //data.translate = position;
    //data.radius = desc_.radius;
    //data.count = desc_.count;
    //data.lifeTime = desc_.lifeTime;
    //data.speed = desc_.speed;
    //data.type = desc_.type;
    //// baseColor は Emit.CS で使うならここ経由で渡す

    //buffers->SetEmitter(data);
}
