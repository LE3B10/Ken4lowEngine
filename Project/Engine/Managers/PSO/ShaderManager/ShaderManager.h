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
	Microsoft::WRL::ComPtr <IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile,
		//初期化で生成したものを3つ
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* includeHandler);

	// 3Dシェーダーをコンパイルする
	void ShaderCompile(DirectXCommon* dxCommon, PipelineType pipelineType);

public: /// ---------- ゲッター ---------- ///

	IDxcBlob* GetVertexShaderBlob(PipelineType pipelineType) { return vertexShaderBlob[pipelineType].Get(); }
	IDxcBlob* GetPixelShaderBlob(PipelineType pipelineType) { return pixelShaderBlob[pipelineType].Get(); }

private: /// ---------- メンバ変数 ---------- ///

	std::array<ComPtr<IDxcBlob>, pipelineNum> vertexShaderBlob;
	std::array<ComPtr<IDxcBlob>, pipelineNum> pixelShaderBlob;
};

