#pragma once
#include "DirectXCommon.h"
#include "DirectXInclude.h"
#include "LogString.h"

// シェーダーコンパイラ専用クラス
class ShaderManager
{
public: // メンバ関数
	// CompilerShader関数
	IDxcBlob* CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile,
		//初期化で生成したものを3つ
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* includeHandler);

	void ShaderCompileObject3D(DirectXCommon* dxCommon);

private: // メンバ変数
	Microsoft::WRL::ComPtr <IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr <IDxcBlob> pixelShaderBlob;
};

