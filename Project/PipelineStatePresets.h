#pragma once
#include <d3d12.h>

namespace Ken4lowEngine::PipelineStatePresets
{
	/// <summary>
	/// 通常のアルファブレンド設定を返す。
	/// </summary>
	D3D12_BLEND_DESC MakeBlendAlpha();

	/// <summary>
	/// ブレンド無効の不透明描画設定を返す。
	/// </summary>
	D3D12_BLEND_DESC MakeBlendOpaque();

	/// <summary>
	/// 背面カリングありのラスタライザ設定を返す。
	/// </summary>
	D3D12_RASTERIZER_DESC MakeRasterizerCullBack();

	/// <summary>
	/// カリングなしのラスタライザ設定を返す。
	/// </summary>
	D3D12_RASTERIZER_DESC MakeRasterizerCullNone();

	/// <summary>
	/// 深度テスト・書き込みを無効化した設定を返す。
	/// </summary>
	D3D12_DEPTH_STENCIL_DESC MakeDepthDisable();

	/// <summary>
	/// 深度テスト・書き込みの両方を有効にした設定を返す。
	/// </summary>
	D3D12_DEPTH_STENCIL_DESC MakeDepthReadWrite();

	/// <summary>
	/// 深度テストのみ有効、書き込みなしの設定を返す。
	/// </summary>
	D3D12_DEPTH_STENCIL_DESC MakeDepthReadOnly();
}