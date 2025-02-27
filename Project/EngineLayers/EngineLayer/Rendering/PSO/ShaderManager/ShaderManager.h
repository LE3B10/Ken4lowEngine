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

	// 3Dシェーダーをコンパイルする
	void ShaderCompileObject3D(DirectXCommon* dxCommon);

public: /// ---------- ゲッター ---------- ///

	IDxcBlob* GetVertexShaderBlob();
	IDxcBlob* GetPixelShaderBlob();

private: /// ---------- メンバ変数 ---------- ///

	Microsoft::WRL::ComPtr <IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr <IDxcBlob> pixelShaderBlob;
};

