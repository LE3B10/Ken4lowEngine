#pragma once
#include "DX12Include.h"

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///			シェーダとリソースを関連付ける管理クラス
/// -------------------------------------------------------------
class RootSignatureManager
{
public: /// ---------- メンバ関数 ---------- ///

	// ルートシグネチャを生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(DirectXCommon* dxCommon);
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignatureForParticle(DirectXCommon* dxCommon);

	/// ---------- ゲッター ---------- ///

	ID3D12RootSignature* GetRootSignature() const;
	ID3D12RootSignature* GetRootSignatureParticle() const;

private: /// ---------- メンバ関数 ---------- ///

	HRESULT hr;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

	Microsoft::WRL::ComPtr <ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3DBlob> errorBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignatureParticle = nullptr;

};

