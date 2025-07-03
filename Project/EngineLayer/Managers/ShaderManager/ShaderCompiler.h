#pragma once
#include "DirectXCommon.h"
#include "DX12Include.h"
#include "LogString.h"

/// -------------------------------------------------------------
///				シェーダーコンパイラ専用クラス
/// -------------------------------------------------------------
class ShaderCompiler
{
public: /// ---------- メンバ関数 ---------- ///
	
	// CompilerShader関数
	static Microsoft::WRL::ComPtr <IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile,
		DXCCompilerManager* dxcManager);

	// Shaderファイルへのパスを生成する
	static std::wstring GetShaderPath(const std::wstring& shaderName, const std::wstring& extension = L"");
};

