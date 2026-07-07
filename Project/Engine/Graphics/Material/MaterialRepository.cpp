#include "MaterialRepository.h"

namespace Ken4lowEngine
{
	void MaterialRepository::InitializeDefaults()
	{
		Clear();
		Register(MaterialAsset::CreateDefault(kDefaultMaterialId, "Default Material"));
	}

	void MaterialRepository::Clear()
	{
		materialsById_.clear();
		idByName_.clear();
	}

	bool MaterialRepository::Register(const MaterialAsset& asset)
	{
		return Register(std::make_shared<MaterialAsset>(asset));
	}

	bool MaterialRepository::Register(const std::shared_ptr<MaterialAsset>& asset)
	{
		if (!asset || asset->GetId().empty())
		{
			return false;
		}

		// RepositoryはCPU側Assetの登録だけを行い、TextureロードやMaterialCBData更新は将来の接続層へ任せる。
		materialsById_[asset->GetId()] = asset;
		if (!asset->GetName().empty())
		{
			idByName_[asset->GetName()] = asset->GetId();
		}
		return true;
	}

	std::shared_ptr<MaterialAsset> MaterialRepository::CreateOrReplace(const std::string& id, const MaterialDesc& desc, const std::string& name)
	{
		if (id.empty())
		{
			return nullptr;
		}

		const std::string displayName = name.empty() ? id : name;
		auto asset = std::make_shared<MaterialAsset>(id, displayName, desc);
		Register(asset);
		return asset;
	}

	std::shared_ptr<MaterialAsset> MaterialRepository::FindById(const std::string& id) const
	{
		const auto it = materialsById_.find(id);
		return it != materialsById_.end() ? it->second : nullptr;
	}

	std::shared_ptr<MaterialAsset> MaterialRepository::FindByName(const std::string& name) const
	{
		const auto nameIt = idByName_.find(name);
		if (nameIt == idByName_.end())
		{
			return nullptr;
		}
		return FindById(nameIt->second);
	}

	std::shared_ptr<MaterialAsset> MaterialRepository::GetDefaultMaterial() const
	{
		return FindById(kDefaultMaterialId);
	}

	bool MaterialRepository::Contains(const std::string& id) const
	{
		return materialsById_.find(id) != materialsById_.end();
	}

	std::vector<std::string> MaterialRepository::GetRegisteredIds() const
	{
		std::vector<std::string> ids;
		ids.reserve(materialsById_.size());
		for (const auto& [id, material] : materialsById_)
		{
			(void)material;
			ids.push_back(id);
		}
		return ids;
	}
}
