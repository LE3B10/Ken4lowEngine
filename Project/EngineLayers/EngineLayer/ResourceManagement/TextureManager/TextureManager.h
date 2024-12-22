#pragma once
#include "DX12Include.h"
#include "LogString.h"

#include <DirectXTex.h>
#include <filesystem>
#include <vector>
#include <unordered_map>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class SRVManager;

/// -------------------------------------------------------------
///					テクスチャ管理クラス
/// -------------------------------------------------------------
class TextureManager
{
private: /// ---------- テクスチャデータの構造体 ---------- ///

	// テクスチャ１枚分のデータ
	struct TextureData
	{
		DirectX::TexMetadata metaData;			  // 画像の幅や高さなどの情報
		ComPtr<ID3D12Resource> resource;		  // テクスチャリソース
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU; // SRV作成時に必要なCPUハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU; // 描画コマンドに必要なGPUハンドル
	};

public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static TextureManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SRVManager* srvManager);

	// DirectX12のTextureResourceを作る
	static ComPtr <ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

	// データを移送するUploadTextureData関数
	static ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	// Textureデータを読む
	static DirectX::ScratchImage LoadTextureData(const std::string& filePath);

	// 動的なテクスチャファイルの読み込み
	void LoadTexture(const std::string& filePath);

	// テクスチャの再リロード
	void ReloadTexture(const std::string& filePath);

public:	/// ---------- セッタ－	---------- ///

	// SRVのセット
	void SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParameter, D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU);

public: /// ---------- ゲッター ---------- ///

	// SRVのインデックスの開始番号の取得
	uint32_t GetTextureIndexByFilePath(const std::string& filePath);

	// テクスチャ番号からGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

	// メタデータを取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	SRVManager* srvManager_ = nullptr;

	// テクスチャデータ
	std::unordered_map<std::string, TextureData> textureDatas;

	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

private: /// ---------- 隠蔽 - コピー禁止 ---------- ///

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(const TextureManager&) = delete;
	const TextureManager& operator=(const TextureManager&) = delete;

};
