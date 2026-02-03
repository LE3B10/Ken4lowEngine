#pragma once
#include "DirectXCommon.h"
#include "DX12Include.h"
#include "LogString.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				シェーダーコンパイラ専用クラス
/// -------------------------------------------------------------
class ShaderCompiler
{
public: /// ---------- メンバ関数 ---------- ///
	
	/// <summary>
	/// 指定した HLSL ファイルを DXC を使ってコンパイルします。<br/>
	/// 内部では DXCCompilerManager から取得した IDxcUtils / IDxcCompiler3 / IncludeHandler を用いて、<br/>
	/// ・HLSL ファイルの読み込み<br/>
	/// ・コンパイルオプションの設定（エントリーポイント main / プロファイル / デバッグ情報など）<br/>
	/// ・コンパイル実行とエラーチェック<br/>
	/// ・コンパイル結果(IDxcBlob)の取得<br/>
	/// を行います。コンパイルに失敗した場合は assert で停止します。
	/// </summary>
	/// <param name="filePath">コンパイルする HLSL ファイルへのパス。</param>
	/// <param name="profile">
	/// 使用するシェーダープロファイル文字列。<br/>
	/// 例：L"vs_6_0"（頂点シェーダ）、L"ps_6_0"（ピクセルシェーダ）、L"cs_6_0"（コンピュートシェーダ）など。
	/// </param>
	/// <param name="dxcManager">
	/// DXC のユーティリティ / コンパイラ / IncludeHandler を管理する DXCCompilerManager へのポインタ。<br/>
	/// nullptr でない前提で使用されます。
	/// </param>
	/// <returns>
	/// コンパイル済みシェーダーバイナリを格納した IDxcBlob の ComPtr。<br/>
	/// この Blob をそのまま D3D12 シェーダー作成（CreateGraphicsPipelineState など）の
	/// pVS / pPS / CS として渡して使用します。
	/// </returns>
	static Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile,
		DXCCompilerManager* dxcManager);

	/// <summary>
	/// シェーダーファイルへのパス文字列を生成します。<br/>
	/// 基本的に<br/>
	/// <c>L"Resources/Shaders/PostEffect/" + shaderName + extension</c><br/>
	/// という形でパスを組み立てます。<br/>
	/// extension を省略した場合は拡張子なしのパスが返されるので、呼び出し元で必要な拡張子を付けることもできます。
	/// </summary>
	/// <param name="shaderName">拡張子を含まないシェーダー名（例：L"GaussianBlur", L"Bloom" など）。</param>
	/// <param name="extension">
	/// 付加する拡張子（例：L".VS.hlsl", L".PS.hlsl", L".CS.hlsl" など）。<br/>
	/// 省略時は空文字列で、拡張子なしのパスが生成されます。
	/// </param>
	/// <returns>生成されたシェーダーファイルへのパス。</returns>
	static std::wstring GetShaderPath(const std::wstring& shaderName, const std::wstring& extension = L"");
};


} // namespace Ken4lowEngine
