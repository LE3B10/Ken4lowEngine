#include "TextureManager.h"

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata& metadata)
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
	Microsoft::WRL::ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,														// Heapの設定
		D3D12_HEAP_FLAG_NONE,													// Heapの特殊な設定。特になし。
		&resourceDesc,															// /Resourceの設定
		D3D12_RESOURCE_STATE_GENERIC_READ,										// 初回のResourceState。Textureは基本読むだけ
		nullptr,																// Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));												// 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));

	return resource;
}

void TextureManager::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages)
{
	//Meta情報を取得
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	//全MipMapについて
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel)
	{
		//MipMapLevelを指定して各Imageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		//Textureに転送
		HRESULT hr = texture->WriteToSubresource(
			UINT(mipLevel),
			nullptr,				//全領域へコピー
			img->pixels,			//元データアドレス
			UINT(img->rowPitch),	//1ラインサイズ
			UINT(img->slicePitch)	//1枚サイズ
		);
		assert(SUCCEEDED(hr));
	}
}

DirectX::ScratchImage TextureManager::LoadTexture(const std::string& filePath)
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
