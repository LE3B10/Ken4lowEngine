#pragma once
#include "DX12Include.h"

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///			シェーダとリソースを関連付ける管理クラス
/// -------------------------------------------------------------
class RootSignatureManager
{
public:
	/// ---------- メンバ関数 ---------- ///

	// ルートシグネチャを生成
	void CreateRootSignature(DirectXCommon* dxCommon);

	// サンプラーの設定
	void SettingSampler();

	/// ---------- メンバ関数 ---------- ///

	ID3D12RootSignature* GetRootSignature() const;

private:
	/// ---------- メンバ関数 ---------- ///

	Microsoft::WRL::ComPtr <ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3DBlob> errorBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};

};

