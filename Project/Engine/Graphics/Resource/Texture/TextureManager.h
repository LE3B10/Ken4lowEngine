#pragma once
#include "DX12Include.h"
#include "LogString.h"

#include <DirectXTex.h>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

namespace Ken4lowEngine
{

	class DirectXCommon;
	class SRVManager;

	class TextureManager
	{
	private:
		struct TextureData
		{
			DirectX::TexMetadata metaData = {};
			ComPtr<ID3D12Resource> resource;
			uint32_t srvIndex = UINT32_MAX;
			D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU{};
			D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{};
		};

	public:
		struct TextureDebugInfo
		{
			std::string loadedPath;
			std::string sourcePath;
			std::string compiledDdsPath;
			DirectX::TexMetadata metadata{};
			bool isPixelArtOrNoMip = false;
			std::string lastWriteTime;
		};
		static TextureManager* GetInstance();

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();

		static ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

		static ComPtr<ID3D12Resource> UploadTextureData(
			ID3D12Resource* texture,
			const DirectX::ScratchImage& mipImages,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* commandList);

		static DirectX::ScratchImage LoadTextureData(const std::string& filePath);

		void LoadTexture(const std::string& filePath);
		void ReloadTexture(const std::string& filePath);

		void CreateSolidColorTexture(
			const std::string& key,
			uint8_t r, uint8_t g, uint8_t b, uint8_t a,
			uint32_t width = 64, uint32_t height = 64);

		void SetGraphicsRootDescriptorTable(
			ID3D12GraphicsCommandList* commandList,
			UINT rootParameter,
			D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU);

		uint32_t GetTextureIndexByFilePath(const std::string& filePath);
		D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);
		uint32_t GetSrvIndex(const std::string& filePath);
		const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
		ID3D12Resource* GetResource(const std::string& filePath);

		// 追加
		void BuildTexturePathIndex();
		std::string FindCompiledTexturePath(const std::string& query) const;
		std::vector<TextureDebugInfo> GetAllTextureDebugInfos() const;

	private:
		std::string NormalizeTexturePath(const std::string& filePath);
		static std::string NormalizeSlashes(std::string path);
		static std::string ToLowerString(std::string s);

	private:
		DirectXCommon* dxCommon_ = nullptr;
		std::unordered_map<std::string, TextureData> textureDatas;

		// 追加
		std::unordered_map<std::string, std::vector<std::string>> texturePathIndex_;

		static uint32_t kSRVIndexTop;
		static constexpr const char* kTextureRootDir = "Resources/Textures/Compiled";

	private:
		TextureManager() = default;
		~TextureManager() = default;
		TextureManager(const TextureManager&) = delete;
		const TextureManager& operator=(const TextureManager&) = delete;
	};

} // namespace Ken4lowEngine
