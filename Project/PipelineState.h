#pragma once
#include "DX12Include.h"
#include "DirectXCommon.h"
#include "BlendModeType.h"


/// -------------------------------------------------------------
///				パイプラインの状態を管理する基底クラス
/// -------------------------------------------------------------
class PipelineState
{
public: /// ---------- メンバ関数 ---------- ///

    // デストラクタ
    virtual ~PipelineState() = default;

    // パイプラインの初期化（継承先で実装）
    virtual void Initialize() = 0;

    // パイプラインの適用（レンダリング時に設定）
    virtual void Render() = 0;

    // パイプラインステートとルートシグネチャの取得
    virtual ID3D12PipelineState* GetPipelineState() const = 0;
    virtual ID3D12RootSignature* GetRootSignature() const = 0;

protected: /// ---------- メンバ変数 ---------- ///

    BlendMode blendMode = BlendMode::kBlendModeNone;

    ComPtr<ID3D12PipelineState> graphicsPipelineState_;
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr <ID3DBlob> signatureBlob;
    ComPtr <ID3DBlob> errorBlob;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
};

