#pragma once
#include "DX12Include.h"
#include <ModelData.h>
#include <Mesh.h>

#include <vector>
#include <string>

namespace Ken4lowEngine
{

	class Model
	{
	public: /// ---------- メンバ関数 ---------- ///

		void Initialize(const std::string& filePath);

	public: /// ---------- アクセッサ ---------- ///

		const ModelData& GetModelData() const { return modelData_; }
		const std::vector<Mesh>& GetMeshes() const { return meshes_; }
		std::vector<Mesh>& GetMeshes() { return meshes_; }
		
		const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& GetMaterialSRVs() const { return materialSRVs_; }
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& GetMaterialSRVs() { return materialSRVs_; }

	private:

		ModelData modelData_;
		std::vector<Mesh> meshes_;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_;
	};


}