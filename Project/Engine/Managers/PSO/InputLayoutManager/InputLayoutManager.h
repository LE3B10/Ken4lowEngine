#pragma once
#include "DX12Include.h"


/// -------------------------------------------------------------
///					頂点データを管理するクラス
/// -------------------------------------------------------------
class InputLayoutManager
{
public: /// ---------- メンバ関数 ---------- ///

	// レイアウトの設定を行う関数
	void Initialize(PipelineType pipelineType);

public: /// ---------- ゲッター ---------- ///

	D3D12_INPUT_LAYOUT_DESC GetInputLayoutDesc(PipelineType pipelineType) { return inputLayoutDesc[pipelineType]; }

private: /// ---------- メンバ変数 ---------- ///

	std::array<D3D12_INPUT_LAYOUT_DESC, pipelineNum> inputLayoutDesc{};
};

