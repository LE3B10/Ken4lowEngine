#pragma once
#include "DX12Include.h"

/// -------------------------------------------------------------
///			シェーダとリソースを関連付ける管理クラス
/// -------------------------------------------------------------
class RootSignatureManager
{
public: /// ---------- メンバ関数 ---------- ///

	// ルートシグネチャを生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* device);

	/// ---------- ゲッター ---------- ///

	ID3D12RootSignature* GetRootSignature() const;

private: /// ---------- メンバ関数 ---------- ///

	Microsoft::WRL::ComPtr <ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3DBlob> errorBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;

};

