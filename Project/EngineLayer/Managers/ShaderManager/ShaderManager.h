#pragma once
#include "DirectXCommon.h"
#include "DX12Include.h"
#include "LogString.h"

/// -------------------------------------------------------------
///				シェーダーコンパイラ専用クラス
/// -------------------------------------------------------------
class ShaderManager
{
public: /// ---------- メンバ関数 ---------- ///
	
	// CompilerShader関数
	static Microsoft::WRL::ComPtr <IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile,
		//初期化で生成したものを3つ
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* includeHandler);

	// Shaderファイルへのパスを生成する
	static std::wstring GetShaderPath(const std::wstring& shaderName, const std::wstring& extension = L"");
};

