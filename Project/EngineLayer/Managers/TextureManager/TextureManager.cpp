#define NOMINMAX
#include "TextureManager.h"

#include "DirectXCommon.h"
#include "SRVManager.h"
#include "ResourceManager.h"

#include <d3dx12.h>

#pragma comment(lib, "d3d12.lib")        // Direct3D 12用
#pragma comment(lib, "dxgi.lib")         // DXGI (DirectX Graphics Infrastructure)用
#pragma comment(lib, "dxguid.lib")       // DXGIやD3D12で使用するGUID定義用

uint32_t TextureManager::kSRVIndexTop = 1;


/// -------------------------------------------------------------
///					シングルトンインスタンス
/// -------------------------------------------------------------
TextureManager* TextureManager::GetInstance()
{
	static TextureManager instance;
	return &instance;
}

void TextureManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
}


/// -------------------------------------------------------------
///					リソースを作成する関数
/// -------------------------------------------------------------
ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata)
{
	//1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);									// Textureの幅
	resourceDesc.Height = UINT(metadata.height);								// Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);						// mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);					// 奥行 or 配列Textureの配列行数
	resourceDesc.Format = metadata.format;										// TextureのFormat
	resourceDesc.SampleDesc.Count = 1;											// サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);		// Textureの次元数。普段使っているのは二次元

	//2. 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;								// 細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;		// WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;					// プロセッサの近くに配膳

	//3. Resourceを生成する
	ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = S_FALSE;
	hr = device->CreateCommittedResource(
		&heapProperties,														// Heapの設定
		D3D12_HEAP_FLAG_NONE,													// Heapの特殊な設定。特になし。
		&resourceDesc,															// /Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,											// 初回のResourceState。Textureは基本読むだけ
		nullptr,																// Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));												// 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));

	return resource;
}


/// -------------------------------------------------------------
///					データを転送する関数
/// -------------------------------------------------------------
[[nodiscard]]
ComPtr<ID3D12Resource> TextureManager::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
	ComPtr<ID3D12Resource> intermediateResource = ResourceManager::CreateBufferResource(device, intermediateSize);
	UpdateSubresources(commandList, texture, intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

	// Textureへの転送後は利用できるよう、D3D12_RESOUCE_STATE_COPY_DESTからD3D12RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}


/// -------------------------------------------------------------
///				テクスチャデータを読み込む関数
/// -------------------------------------------------------------
DirectX::ScratchImage TextureManager::LoadTextureData(const std::string& filePath)
{
	//テクスチャファイルを呼んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミップマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//ミップマップ付きのデータを返す
	return mipImages;
}


/// -------------------------------------------------------------
///			動的なテクスチャファイルを読み込む関数
/// -------------------------------------------------------------
void TextureManager::LoadTexture(const std::string& filePath)
{
	HRESULT hr{};

	std::string filePathStr = NormalizeTexturePath(filePath);

	// 読み込み済みテクスチャを検索
	if (textureDatas.contains(filePathStr)) return;

	// テクスチャファイルを読み込んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePathStr);

	// DDSの読み込み
	if (filePathW.ends_with(L".dds"))
	{
		// .ddsで終わっていたらDDSとして読み込む
		hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
		assert(SUCCEEDED(hr));
	}
	else
	{
		// WIC形式（pngやjpgなど）として読み込む
		hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
		assert(SUCCEEDED(hr));
	}

	// ミップマップの作成（ScratchImageに保持する）
	DirectX::ScratchImage mipImages{};
	//const auto& meta = image.GetMetadata(); // 読み込んだ画像のメタデータを取得
	const auto& meta0 = image.GetMetadata(); // 最初の画像のメタデータを取得

	// 旧スキン系：幅が高さの2倍 (64x32, 128x64, 256x128 ...)
	const bool isLegacyTall = (meta0.width == meta0.height * 2);

	// 正方ではないテクスチャなら、縦を倍にした空キャンパスへコピーして正方化
	DirectX::ScratchImage normalized; // 正方化した画像を保持するScratchImage
	if (isLegacyTall)
	{
		DirectX::TexMetadata metaData0 = meta0; // 最初の画像のメタデータを取得
		metaData0.height = meta0.width; // 高さを幅に合わせて正方化
		metaData0.mipLevels = 1; // ミップレベルは1に設定
		metaData0.arraySize = 1; // 配列サイズは元のまま

		// 64x64 128x128 ... の正方キャンパスを作成
		hr = normalized.Initialize2D(metaData0.format, metaData0.width, metaData0.height, metaData0.arraySize, metaData0.mipLevels);
		assert(SUCCEEDED(hr)); // 正方キャンパスの初期化に成功

		// 元画像を正方キャンパスの上半分にコピー
		const DirectX::Image* srcImage = image.GetImage(0, 0, 0); // 元画像の最初のイメージを取得
		const DirectX::Image* destImage = normalized.GetImage(0, 0, 0); // 正方キャンパスの最初のイメージを取得

		DirectX::Rect srcRect = { 0, 0, static_cast<size_t>(srcImage->width), static_cast<size_t>(srcImage->height) }; // 元画像のコピー元矩形

		// コピー先のYオフセットを計算（正方キャンパスの上半分に配置）
		hr = DirectX::CopyRectangle(*srcImage, srcRect, *destImage, DirectX::TEX_FILTER_DEFAULT, 0, UINT(srcImage->height)); // 元画像を正方キャンパスにコピー
		assert(SUCCEEDED(hr)); // コピーに成功
	}

	DirectX::ScratchImage& baseImage = isLegacyTall ? normalized : image; // 正方化した画像か元画像かを選択
	const auto& baseMeta = baseImage.GetMetadata(); // ベース画像のメタデータを取得

	// ミップレベル数を画像サイズから自動算出
	size_t maxMips = static_cast<size_t>(std::log2(std::max(baseMeta.width, baseMeta.height))) + 1;
	size_t mipLevels = std::min(maxMips, size_t(4)); // 最大4ミップまで生成

	if (DirectX::IsCompressed(baseMeta.format))
	{
		// 圧縮フォーマットならミップ生成せずそのまま使う
		mipImages = std::move(baseImage);
	}
	else if (baseMeta.width > 1 && baseMeta.height > 1)
	{
		// 非圧縮かつ2x2以上ならミップマップを生成
		hr = DirectX::GenerateMipMaps(baseImage.GetImages(), baseImage.GetImageCount(), baseMeta, DirectX::TEX_FILTER_SRGB, mipLevels, mipImages);
		assert(SUCCEEDED(hr));
	}
	else
	{
		// 1x1などミップマップを生成できないサイズのときは、そのまま1レベルの画像として初期化
		hr = mipImages.InitializeFromImage(*baseImage.GetImages()); // ← ここで参照渡し
		assert(SUCCEEDED(hr));
	}

	// 追加したテクスチャデータの参照を取得
	TextureData& textureData = textureDatas[filePathStr];

	// テクスチャリソースの生成
	textureData.metaData = mipImages.GetMetadata();
	textureData.resource = CreateTextureResource(dxCommon_->GetDevice(), textureData.metaData);

	// 中間リソースにデータを転送
	ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(textureData.resource.Get(), mipImages, dxCommon_->GetDevice(), dxCommon_->GetCommandManager()->GetCommandList());

	// コマンドを実行し完了まで待機
	dxCommon_->GetCommandManager()->ExecuteAndWait();

	// SRV（Shader Resource View）の確保
	textureData.srvIndex = SRVManager::GetInstance()->Allocate();
	textureData.srvHandleCPU = SRVManager::GetInstance()->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = SRVManager::GetInstance()->GetGPUDescriptorHandle(textureData.srvIndex);

	// SRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metaData.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (textureData.metaData.IsCubemap())
	{
		// キューブマップとして扱う場合
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT_MAX;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		// 通常の2Dテクスチャ
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(textureData.metaData.mipLevels);
	}

	// SRVの生成
	dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);
}


/// -------------------------------------------------------------
///					テクスチャの再読み込み
/// -------------------------------------------------------------
void TextureManager::ReloadTexture(const std::string& filePath)
{
	// 既存テクスチャが存在する場合、古いリソースを解放
	auto it = textureDatas.find(filePath);
	if (it != textureDatas.end())
	{
		SRVManager::GetInstance()->Free(it->second.srvIndex);
		it->second.resource.Reset();
	}

	// 新しいテクスチャを読み込む
	LoadTexture(filePath);
}


/// -------------------------------------------------------------
///					デスクリプタテーブルの設定
/// -------------------------------------------------------------
void TextureManager::SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParameter, D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU)
{
	// ディスクリプタヒープの設定
	SRVManager::GetInstance()->PreDraw();

	// ディスクリプタテーブルの設定
	commandList->SetGraphicsRootDescriptorTable(rootParameter, textureSRVHandleGPU);
}


/// -------------------------------------------------------------
///						SRVの開始番号の取得
/// -------------------------------------------------------------
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
	auto it = textureDatas.find(filePath);
	if (it != textureDatas.end()) {
		return it->second.srvIndex;
	}

	throw std::runtime_error("Texture not found: " + filePath); // 例外をスロー
}


/// -------------------------------------------------------------
///						GPUハンドルの取得
/// -------------------------------------------------------------
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
	std::string filePathStr = NormalizeTexturePath(filePath);

	// 範囲外指定違反チェック
	assert(textureDatas.find(filePathStr) != textureDatas.end()); // テクスチャ番号が正常範囲内である

	// テクスチャデータの参照を取得
	TextureData& textureData = textureDatas[filePathStr];

	// GPUハンドルを返す
	return textureData.srvHandleGPU;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
{
	std::string filePathStr = NormalizeTexturePath(filePath);

	// 範囲外指定違反チェック
	assert(textureDatas.find(filePathStr) != textureDatas.end()); // テクスチャ番号が正常範囲内である

	// テクスチャデータの参照を取得
	TextureData& textureData = textureDatas[filePathStr];

	// GPUハンドルを返す
	return textureData.srvIndex;
}


/// -------------------------------------------------------------
///						メタデータを取得
/// -------------------------------------------------------------
const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
	std::string filePathStr = NormalizeTexturePath(filePath);

	// 範囲外指定違反チェック
	assert(textureDatas.find(filePathStr) != textureDatas.end()); // テクスチャ番号が正常範囲内である

	// テクスチャデータの参照を取得
	TextureData& textureData = textureDatas[filePathStr];

	// GPUハンドルを返す
	return textureData.metaData;
}

ID3D12Resource* TextureManager::GetResource(const std::string& filePath)
{
	std::string filePathStr = NormalizeTexturePath(filePath);
	auto it = textureDatas.find(filePathStr);
	assert(it != textureDatas.end());                 // 事前に LoadTexture 済みであること
	return it->second.resource.Get();                 // ID3D12Resource* を返す
}

