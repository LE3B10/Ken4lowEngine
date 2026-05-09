#pragma once
#include "DX12Include.h"
#include <ModelData.h>
#include <Mesh.h>
#include "Engine/Graphics/Culling/Frustum.h"

#include <cstddef>
#include <vector>
#include <string>

namespace Ken4lowEngine
{

	class Model
	{
	public: /// ---------- メンバ関数 ---------- ///

		void Initialize(const std::string& filePath);

		void BuildLocalBounds();

	public: /// ---------- アクセッサ ---------- ///

		const ModelData& GetModelData() const { return modelData_; }
		const std::vector<Mesh>& GetMeshes() const { return meshes_; }
		std::vector<Mesh>& GetMeshes() { return meshes_; }
		const BoundingSphere& GetLocalBounds() const { return localBounds_; }
		bool HasLocalBounds() const { return hasLocalBounds_; }
		const BoundingSphere& GetMeshLocalBounds(size_t index) const { return meshLocalBounds_.at(index); }
		bool HasMeshLocalBounds(size_t index) const { return index < meshHasLocalBounds_.size() && meshHasLocalBounds_[index]; }
		
		const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& GetMaterialSRVs() const { return materialSRVs_; }
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& GetMaterialSRVs() { return materialSRVs_; }

	private:

		ModelData modelData_;
		std::vector<Mesh> meshes_;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_;
		BoundingSphere localBounds_{};
		bool hasLocalBounds_ = false;
		std::vector<BoundingSphere> meshLocalBounds_{};
		std::vector<bool> meshHasLocalBounds_{};
	};


}
