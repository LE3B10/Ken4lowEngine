#include "SkinCluster.h"
#include "Skeleton.h"
#include "ResourceManager.h"
#include "SRVManager.h"
#include "DirectXCommon.h"

#include <cassert>
#include <cstring>

void SkinCluster::Initialize(const ModelData& modelData, Skeleton& skeleton, uint32_t descriptorSize)
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();
	auto& joints = skeleton.GetJoints();

	// palette Resource
	paletteResource_ = ResourceManager::CreateBufferResource(device, sizeof(WellForGPU) * joints.size());
	WellForGPU* mappedPalette = nullptr;
	paletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	mappedPalette_ = { mappedPalette, joints.size() }; // spanを使ってアクセスするようにする

	paletteSrvHandle_.first = SRVManager::GetInstance()->GetCPUDescriptorHandle(descriptorSize);
	paletteSrvHandle_.second = SRVManager::GetInstance()->GetGPUDescriptorHandle(descriptorSize);

	// SRVの作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Buffer.NumElements = UINT(joints.size());
	srvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
	device->CreateShaderResourceView(paletteResource_.Get(), &srvDesc, paletteSrvHandle_.first);

	// influence Resourceの作成
	influenceResource_ = ResourceManager::CreateBufferResource(device, sizeof(VertexInfluence) * modelData.vertices.size());
	VertexInfluence* mappedInfluence = nullptr;
	influenceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size()); // 0埋め、weightを0にする
	mappedInfluenceData_ = { mappedInfluence, modelData.vertices.size() };

	// VBV
	influenceBufferView_.BufferLocation = influenceResource_->GetGPUVirtualAddress();
	influenceBufferView_.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
	influenceBufferView_.StrideInBytes = sizeof(VertexInfluence);

	// inverse bind pose
	inverseBindPoseMatrices_.resize(joints.size());
	std::generate(inverseBindPoseMatrices_.begin(), inverseBindPoseMatrices_.end(), Matrix4x4::MakeIdentity);

	const auto& jointMap = skeleton.GetJointMap();
	for (const auto& [jointName, jointWeightData] : modelData.skinClusterData)
	{
		auto it = jointMap.find(jointName);
		if (it == jointMap.end()) continue;

		uint32_t jointIndex = it->second;
		inverseBindPoseMatrices_[jointIndex] = jointWeightData.inverseBindPoseMatrix;

		for (const auto& vw : jointWeightData.vertexWeights)
		{
			auto& influence = mappedInfluenceData_[vw.vertexIndex];
			for (uint32_t i = 0; i < kNumMaxInfluence; ++i)
			{
				if (influence.weights[i] == 0.0f)
				{
					influence.weights[i] = vw.weight;
					influence.jointIndices[i] = jointIndex;
					break;
				}
			}
		}
	}
}

void SkinCluster::UpdatePaletteMatrix(Skeleton& skeleton)
{
	auto& joints = skeleton.GetJoints();
	for (size_t jointIndex = 0; jointIndex < joints.size(); ++jointIndex)
	{
		assert(jointIndex < inverseBindPoseMatrices_.size());
		mappedPalette_[jointIndex].skeletonSpaceMatrix = inverseBindPoseMatrices_[jointIndex] * joints[jointIndex].skeletonSpaceMatrix;
		mappedPalette_[jointIndex].skeletonSpaceInverceTransposeMatrix = Matrix4x4::Transpose(Matrix4x4::Inverse(mappedPalette_[jointIndex].skeletonSpaceMatrix));
	}
}
