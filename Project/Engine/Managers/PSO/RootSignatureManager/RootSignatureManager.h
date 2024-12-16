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
	ComPtr<ID3D12RootSignature> CreateRootSignature(DirectXCommon* dxCommon);

	/// ---------- ゲッター ---------- ///

	ID3D12RootSignature* GetRootSignature() const { return rootSignature.Get(); }

private: /// ---------- メンバ関数 ---------- ///

	ComPtr <ID3DBlob> signatureBlob = nullptr;
	ComPtr <ID3DBlob> errorBlob = nullptr;
	ComPtr <ID3D12RootSignature> rootSignature = nullptr;

};

