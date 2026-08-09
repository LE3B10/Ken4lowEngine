#pragma once
#include "DX12Include.h"
#include "LogString.h"

#include <DirectXTex.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;
	class SRVManager;

	/// -------------------------------------------------------------
	///						テクスチャ管理クラス
	/// -------------------------------------------------------------
	class TextureManager
	{
	private: /// ---------- 構造体 ---------- ///

		// テクスチャデータ構造体
		struct TextureData
		{
			DirectX::TexMetadata metaData = {};			// テクスチャのメタデータ（幅、高さ、ミップレベル数など）
			ComPtr<ID3D12Resource> resource;			// テクスチャリソース
			uint32_t srvIndex = UINT32_MAX;				// SRVヒープ上のインデックス。UINT32_MAXは未割り当てを示す。
			D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU{}; // CPU側のSRVハンドル
			D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{}; // GPU側のSRVハンドル
		};

	public: /// ---------- 構造体 ---------- ///

		struct TextureMemoryStats
		{
			std::size_t textureCount = 0;
			std::size_t descriptorCount = 0;
			uint64_t estimatedGpuBytes = 0;
		};

	public: /// ---------- メンバ関数 ---------- ///

		// シングルトンインスタンスを取得
		static TextureManager* GetInstance();

		// 初期化処理
		void Initialize(DirectXCommon* dxCommon);

		// 終了処理
		void Finalize();

		// テクスチャリソースの作成
		static ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

		// テクスチャデータのアップロード
		static ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

		// テクスチャデータの読み込み
		static DirectX::ScratchImage LoadTextureData(const std::string& filePath);

		// テクスチャの読み込みとSRV作成
		void LoadTexture(const std::string& filePath);

		// テクスチャの再読み込み
		void ReloadTexture(const std::string& filePath);

		// テクスチャの削除
		void CreateSolidColorTexture(const std::string& key, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t width = 64, uint32_t height = 64);

		// ルートパラメータにSRVテーブルをセット
		void SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParameter, D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU);

	public: /// ---------- アクセッサ ---------- ///

		// テクスチャのSRVインデックスを取得
		uint32_t GetTextureIndexByFilePath(const std::string& filePath);

		// テクスチャのSRVハンドルを取得
		D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

		// テクスチャのSRVインデックスを取得
		uint32_t GetSrvIndex(const std::string& filePath);

		// テクスチャのメタデータを取得
		const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

		// テクスチャリソースを取得
		ID3D12Resource* GetResource(const std::string& filePath);

		TextureMemoryStats GetMemoryStats() const
		{
			TextureMemoryStats stats{};
			stats.textureCount = textureDatas.size();
			for (const auto& [path, texture] : textureDatas)
			{
				(void)path;
				if (texture.srvIndex != UINT32_MAX) ++stats.descriptorCount;

				const DirectX::TexMetadata& meta = texture.metaData;
				for (std::size_t mip = 0; mip < (std::max<std::size_t>)(1, meta.mipLevels); ++mip)
				{
					const std::size_t width = (std::max<std::size_t>)(1, meta.width >> mip);
					const std::size_t height = (std::max<std::size_t>)(1, meta.height >> mip);
					const std::size_t depth = (std::max<std::size_t>)(1, meta.depth >> mip);
					std::size_t rowPitch = 0;
					std::size_t slicePitch = 0;
					if (SUCCEEDED(DirectX::ComputePitch(meta.format, width, height, rowPitch, slicePitch)))
					{
						stats.estimatedGpuBytes += static_cast<uint64_t>(slicePitch) * depth * (std::max<std::size_t>)(1, meta.arraySize);
					}
				}
			}
			return stats; // Texture payloadのみを概算し、D3D12 Heap alignmentやdriver residencyは含めない。
		}

		// テクスチャパスのインデックスを構築
		void BuildTexturePathIndex();

		// クエリに対して、インデックスに登録されたテクスチャパスの中から最も類似するものを返す
		std::string FindCompiledTexturePath(const std::string& query) const;

	private: /// ---------- ヘルパー関数 ---------- ///

		// テクスチャパスを正規化する関数（例: 大文字小文字の統一、スラッシュの統一など）
		std::string NormalizeTexturePath(const std::string& filePath);

		// クエリとテクスチャパスの類似度を計算する関数（例: レーベンシュタイン距離など）
		static std::string NormalizeSlashes(std::string path);

		// 文字列を小文字に変換する関数
		static std::string ToLowerString(std::string s);

	private: /// ---------- メンバ変数 ---------- ///

		// DirectXコモン
		DirectXCommon* dxCommon_ = nullptr;

		// SRVマネージャー
		std::unordered_map<std::string, TextureData> textureDatas;

		// テクスチャパスのインデックス（正規化されたパスをキー、元のパスのリストを値とする）
		std::unordered_map<std::string, std::vector<std::string>> texturePathIndex_;

		// SRVインデックスの割り当て開始位置。これ以降のインデックスはテクスチャ用に割り当てる。
		static uint32_t kSRVIndexTop;

		// コンパイル済みテクスチャのルートディレクトリ
		static constexpr const char* kTextureRootDir = "Resources/Textures/Compiled";

	private: /// ---------- コンストラクタ / デストラクタ / コピー禁止 ---------- ///

		TextureManager() = default;
		~TextureManager() = default;
		TextureManager(const TextureManager&) = delete;
		const TextureManager& operator=(const TextureManager&) = delete;
	};

} // namespace Ken4lowEngine