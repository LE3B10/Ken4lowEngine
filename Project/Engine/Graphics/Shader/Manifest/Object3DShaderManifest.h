#pragma once
#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///              Object3D 用 Shader ID
	/// -------------------------------------------------------------
	enum class Object3DShaderId : uint32_t
	{
		Object3DVS,
		Object3DInstancingVS,
		Object3DInstancingShadowVS,
		Object3DPS,
		ShadowMapVS,
	};

	/// -------------------------------------------------------------
	///            Object3D 用 Shader Manifest
	/// -------------------------------------------------------------
	class Object3DShaderManifest
	{
	public:
		static const ShaderDescriptor& Get(Object3DShaderId id)
		{
			switch (id)
			{
			case Object3DShaderId::Object3DVS:
			{
				static const ShaderDescriptor desc{
					L"Object3DVS",
					L"Resources/Shaders/Object3D/Object3D.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::Object3D
				};
				return desc;
			}
			case Object3DShaderId::Object3DPS:
			{
				static const ShaderDescriptor desc{
					L"Object3DPS",
					L"Resources/Shaders/Object3D/Object3D.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::Object3D
				};
				return desc;
			}
			case Object3DShaderId::Object3DInstancingVS:
			{
				static const ShaderDescriptor desc{
					L"Object3DInstancingVS",
					L"Resources/Shaders/Object3D/Object3dInstancing.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::Object3D
				};
				return desc;
			}
			case Object3DShaderId::Object3DInstancingShadowVS:
			{
				static const ShaderDescriptor desc{
					L"Object3DInstancingShadowVS",
					L"Resources/Shaders/Object3D/Object3dInstancingShadow.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::ShadowMap
				};
				return desc;
			}
			case Object3DShaderId::ShadowMapVS:
			{
				static const ShaderDescriptor desc{
					L"ShadowMapVS",
					L"Resources/Shaders/Object3D/ShadowMap.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::ShadowMap
				};
				return desc;
			}
			default:
				throw std::runtime_error("Object3DShaderManifest::Get - Unknown id");
			}
		}
	};

} // namespace Ken4lowEngine
