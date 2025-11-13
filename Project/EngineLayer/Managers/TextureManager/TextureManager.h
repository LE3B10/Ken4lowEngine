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
		DirectX::TexMetadata metaData = {};			// 画像の幅や高さなどの情報
		ComPtr<ID3D12Resource> resource;		    // テクスチャリソース
		uint32_t srvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU{}; // SRV作成時に必要なCPUハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{}; // 描画コマンドに必要なGPUハンドル
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// TextureManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>TextureManager の唯一のインスタンス</returns>
	static TextureManager* GetInstance();

	/// <summary>
	/// TextureManager を初期化します。
	/// DirectXCommon を保持し、デバイスやコマンドリスト取得に使用します。
	/// </summary>
	/// <param name="dxCommon">DirectX12 の共通クラスへのポインタ</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// DirectX12 のテクスチャ用リソースを作成します。
	/// TexMetadata をもとに幅・高さ・ミップ数などを設定した 2D テクスチャリソースを生成します。
	/// </summary>
	/// <param name="device">リソースを作成する ID3D12Device</param>
	/// <param name="metadata">テクスチャのメタデータ</param>
	/// <returns>作成されたテクスチャリソース</returns>
	static ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

	/// <summary>
	/// ScratchImage に格納されたミップ付きテクスチャデータを GPU リソースへアップロードします。
	/// 中間バッファ（Upload ヒープ）を作成し、UpdateSubresources で転送します。
	/// 転送後はテクスチャの ResourceState を COPY_DEST から GENERIC_READ に遷移させます。
	/// </summary>
	/// <param name="texture">アップロード先のテクスチャリソース</param>
	/// <param name="mipImages">ミップマップ付き画像データ</param>
	/// <param name="device">デバイス</param>
	/// <param name="commandList">コピーコマンドを発行するコマンドリスト</param>
	/// <returns>中間バッファリソース（Upload ヒープ）</returns>
	static ComPtr<ID3D12Resource> UploadTextureData(
		ID3D12Resource* texture,
		const DirectX::ScratchImage& mipImages,
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList);

	/// <summary>
	/// 指定ファイルからテクスチャデータを読み込み、ミップマップ付きの ScratchImage を生成します。
	/// 主に静的な読み込み処理やテスト用途のヘルパーとして使用します。
	/// </summary>
	/// <param name="filePath">読み込むテクスチャファイルのパス</param>
	/// <returns>ミップマップ付きの ScratchImage</returns>
	static DirectX::ScratchImage LoadTextureData(const std::string& filePath);

	/// <summary>
	/// ファイルパスを指定してテクスチャを読み込み、GPU リソースと SRV を生成します。
	/// すでに同じパスのテクスチャがロード済みの場合は何もせずに帰ります。
	/// DDS 形式（.dds）の場合は DirectX::LoadFromDDSFile を使用し、
	/// それ以外（png / jpg 等）は WIC 経由で読み込みます。
	/// </summary>
	/// <param name="filePath">
	/// 読み込むテクスチャファイルのパス
	/// "Resources/Textures/" を付けない相対パスでも指定可能です。
	/// </param>
	void LoadTexture(const std::string& filePath);

	/// <summary>
	/// テクスチャを再読み込みします。
	/// 既に存在する場合は古い SRV とリソースを解放し、新たに LoadTexture を行います。
	/// 開発中のテクスチャ差し替えなどに利用します。
	/// </summary>
	/// <param name="filePath">再読み込みするテクスチャファイルのパス</param>
	void ReloadTexture(const std::string& filePath);

public:	/// ---------- セッタ－	---------- ///

	/// <summary>
	/// 指定した GPU ハンドルを、ルートパラメータのディスクリプタテーブルとして設定します。
	/// 内部で SRVManager::PreDraw() を呼び出し、使用する SRV ヒープをコマンドリストへセットします。
	/// </summary>
	/// <param name="commandList">描画コマンドを発行するコマンドリスト。</param>
	/// <param name="rootParameter">ルートパラメータインデックス</param>
	/// <param name="textureSRVHandleGPU">テクスチャ SRV の GPU ハンドル</param>
	void SetGraphicsRootDescriptorTable(
		ID3D12GraphicsCommandList* commandList,
		UINT rootParameter,
		D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU);

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 指定したファイルパスに対応するテクスチャの SRV インデックスを取得します。
	/// 見つからない場合は例外を投げます。
	/// </summary>
	/// <param name="filePath">テクスチャファイルのパス</param>
	/// <returns>SRV のインデックス</returns>
	uint32_t GetTextureIndexByFilePath(const std::string& filePath);

	/// <summary>
	/// ファイルパスからテクスチャの SRV GPU ハンドルを取得します。
	/// 描画時に SetGraphicsRootDescriptorTable へ渡すために使用します。
	/// </summary>
	/// <param name="filePath">テクスチャファイルのパス</param>
	/// <returns>SRV の GPU ハンドル</returns>
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

	/// <summary>
	/// ファイルパスからテクスチャの SRV インデックスを取得します。
	/// </summary>
	/// <param name="filePath">テクスチャファイルのパス</param>
	/// <returns>SRV のインデックス</returns>
	uint32_t GetSrvIndex(const std::string& filePath);

	/// <summary>
	/// ファイルパスからテクスチャのメタデータを取得します。
	/// 幅・高さ・フォーマットなどの情報を参照したいときに使用します。
	/// </summary>
	/// <param name="filePath">テクスチャファイルのパス</param>
	/// <returns>TexMetadata への const 参照</returns>
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

	/// <summary>
	/// ファイルパスからテクスチャの ID3D12Resource* を取得します。
	/// マスクテクスチャ用に別の SRV を作り直すなど、リソース本体が必要な場合に使用します。
	/// </summary>
	/// <param name="filePath">テクスチャファイルのパス</param>
	/// <returns>テクスチャリソースのポインタ</returns>
	ID3D12Resource* GetResource(const std::string& filePath);

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// テクスチャ用のファイルパスを正規化します。
	/// "Resources/Textures/" で始まっていない場合、自動的に付与して返します。
	/// </summary>
	/// <param name="filePath">元のファイルパス</param>
	/// <returns>正規化されたファイルパス</returns>
	std::string NormalizeTexturePath(const std::string& filePath)
	{
		if (filePath.starts_with("Resources/Textures/")) return filePath;
		return "Resources/Textures/" + filePath;
	}

private: /// ---------- メンバ変数 ---------- ///

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// テクスチャデータ
	std::unordered_map<std::string, TextureData> textureDatas;

	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

private: /// ---------- 隠蔽 - コピー禁止 ---------- ///

	/// <summary>
	/// 外部からの生成を禁止するためのプライベートコンストラクタ。
	/// シングルトンパターンとして使用します。
	/// </summary>
	TextureManager() = default;

	/// <summary>
	/// デフォルトデストラクタ。
	/// </summary>
	~TextureManager() = default;

	/// <summary>
	/// コピーコンストラクタは使用禁止です。
	/// </summary>
	TextureManager(const TextureManager&) = delete;

	/// <summary>
	/// 代入演算子は使用禁止です。
	/// </summary>
	const TextureManager& operator=(const TextureManager&) = delete;
};
