#include "TextureManager.h"

#include "DirectXCommon.h"
#include <d3dx12.h>
#include "ResourceManager.h"

#pragma comment(lib, "d3d12.lib")        // Direct3D 12用
#pragma comment(lib, "dxgi.lib")         // DXGI (DirectX Graphics Infrastructure)用
#pragma comment(lib, "dxguid.lib")       // DXGIやD3D12で使用するGUID定義用


/// -------------------------------------------------------------
///					シングルトンインスタンス
/// -------------------------------------------------------------
TextureManager* TextureManager::GetInstance()
{
	static TextureManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///				　テクスチャの読み込みとSRVの設定
/// -------------------------------------------------------------
//TextureManager::TextureData* TextureManager::LoadTextureWithSRV(const std::string& filePath)
//{
//	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
//
//	// 1. テクスチャが既にロード済みかをチェック
//	for (auto& textureData : textureDatas) {
//		if (textureData.filePath == filePath) {
//			return &textureData;
//		}
//	}
//
//	// 2. 新しいテクスチャデータの準備
//	DirectX::ScratchImage mipImages = LoadTexture(filePath);
//	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
//
//	// 3. テクスチャリソースの作成
//	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = CreateTextureResource(dxCommon->GetDevice(), metadata);
//
//	// 4. テクスチャデータをアップロード
//	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(
//		textureResource, mipImages, dxCommon->GetDevice(), dxCommon->GetCommandList());
//
//	// 5. SRV設定
//	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
//	srvDesc.Format = metadata.format;
//	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
//	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
//
//	// 6. デスクリプタハンドル取得
//	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = dxCommon->GetDescriptorHeap()->GetCPUDescriptorHandle(
//		dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorHeap()->GetDescriptorSizeSRV(), static_cast<uint32_t>(textureDatas.size()));
//	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU = dxCommon->GetDescriptorHeap()->GetGPUDescriptorHandle(
//		dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorHeap()->GetDescriptorSizeSRV(), static_cast<uint32_t>(textureDatas.size()));
//
//	// 7. SRVの作成
//	dxCommon->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, srvHandleCPU);
//
//	// 8. 新しいテクスチャデータをリストに追加
//	textureDatas.push_back({ filePath, metadata, textureResource, srvHandleCPU, srvHandleGPU });
//
//	return &textureDatas.back();
//}


/// -------------------------------------------------------------
///					リソースを作成する関数
/// -------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata)
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
Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = ResourceManager::CreateBufferResource(device, intermediateSize);
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

void TextureManager::SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParameter, D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU)
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// ディスクリプタヒープの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon->GetSRVDescriptorHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// ディスクリプタテーブルの設定
	commandList->SetGraphicsRootDescriptorTable(rootParameter, textureSRVHandleGPU);
}
