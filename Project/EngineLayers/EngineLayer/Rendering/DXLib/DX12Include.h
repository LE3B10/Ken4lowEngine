#pragma once
#include <cstdint>
#include <cassert>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <dxgidebug.h>

/* ----- cpp側で書くlib一覧　-----
* #pragma comment(lib, "d3d12.lib")        // Direct3D 12用
* #pragma comment(lib, "dxgi.lib")         // DXGI (DirectX Graphics Infrastructure)用
* #pragma comment(lib, "dxguid.lib")       // DXGIやD3D12で使用するGUID定義用
* #pragma comment(lib, "dxcompiler.lib")   // DXC (DirectX Shader Compiler)用
* #pragma comment(lib, "dxguid.lib")       // DXGIデバッグ用 (dxgidebugを使用する場合)
*/

#include <wrl.h>

using namespace Microsoft::WRL;

// パイプラインの種類
enum PipelineType
{
	TypeObject3D,  // オブジェクト3D
	TypeParticle,  // パーティクル
	TypePipelineNum // パイプラインの総数（カウント用）
};

// パイプラインタイプの数
static inline const uint32_t kPipelineCount = static_cast<size_t>(PipelineType::TypePipelineNum);