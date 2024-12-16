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
	ComPtr<ID3D12RootSignature> CreateRootSignature(DirectXCommon* dxCommon, PipelineType pipelineType);

	/// ---------- ゲッター ---------- ///

	ID3D12RootSignature* GetRootSignature(PipelineType pipelineType) const { return rootSignature[pipelineType].Get(); }

private: /// ---------- メンバ関数 ---------- ///

	std::array<ComPtr <ID3D12RootSignature>, pipelineNum> rootSignature = { nullptr,nullptr };
	ComPtr <ID3DBlob> signatureBlob = nullptr;
	ComPtr <ID3DBlob> errorBlob = nullptr;
};

