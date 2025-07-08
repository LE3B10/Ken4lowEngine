#include "SkinCluster.h"
#include "Skeleton.h"
#include "ResourceManager.h"
#include "SRVManager.h"
#include "DirectXCommon.h"

#include <cassert>
#include <cstring>

SkinCluster::~SkinCluster()
{
	if (paletteSrvIndex_ != UINT32_MAX) {
		SRVManager::GetInstance()->Free(paletteSrvIndex_);
	}
}

void SkinCluster::Initialize(const ModelData& modelData, Skeleton& skeleton)
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();
	auto& joints = skeleton.GetJoints();

	// palette Resource
	paletteResource_ = ResourceManager::CreateBufferResource(device, sizeof(WellForGPU) * joints.size());
	WellForGPU* mappedPalette = nullptr;
	paletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	mappedPalette_ = { mappedPalette, joints.size() }; // spanを使ってアクセスするようにする

	// SRVのインデックスを確保
	paletteSrvIndex_ = SRVManager::GetInstance()->Allocate();
	paletteSrvHandle_.first = SRVManager::GetInstance()->GetCPUDescriptorHandle(paletteSrvIndex_);
	paletteSrvHandle_.second = SRVManager::GetInstance()->GetGPUDescriptorHandle(paletteSrvIndex_);

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

	// --- inverseBindPose 配列をスケルトン数に合わせて用意 -----------
	inverseBindPoseMatrices_.resize(joints.size(), Matrix4x4::MakeIdentity());

	// --- Influence 書き込み & 範囲チェック --------------------------
	const auto& jointMap = skeleton.GetJointMap();

	for (const auto& [jName, jWeightData] : modelData.skinClusterData)
	{
		auto it = jointMap.find(jName);
		if (it == jointMap.end()) continue;             // ★ スケルトンに無いボーンは無視

		uint32_t jIdx = it->second;
		inverseBindPoseMatrices_[jIdx] = jWeightData.inverseBindPoseMatrix;

		for (const auto& vw : jWeightData.vertexWeights)
		{
			if (vw.vertexIndex >= mappedInfluenceData_.size()) continue; // ★ 範囲外防御

			auto& inf = mappedInfluenceData_[vw.vertexIndex];
			for (uint32_t i = 0; i < kNumMaxInfluence; ++i)
			{
				if (inf.weights[i] == 0.0f)
				{
					inf.weights[i] = vw.weight;
					inf.jointIndices[i] = jIdx;
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
