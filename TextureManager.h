#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <DirectXTex.h>

#include "LogString.h"

// テクスチャ管理クラス
class TextureManager
{
public: // メンバ関数
	// DirectX12のTextureResourceを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr <ID3D12Device> device, const DirectX::TexMetadata& metadata);
	//データを移送するUploadTextureData関数
	void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);
	// Textureデータを読む
	DirectX::ScratchImage LoadTexture(const std::string& filePath);


private: // メンバ変数


};

