#pragma once
#include <DX12Include.h>

/// -------------------------------------------------------------
///			DirectX12のHLSLコンパイラーを管理するクラス
/// -------------------------------------------------------------
class DXCCompilerManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

public: /// ---------- ゲッター ---------- ///

	// IDxcUtilsの取得
	IDxcUtils* GetIDxcUtils() const { return dxcUtils_.Get(); }

	// IDxcCompiler3の取得
	IDxcCompiler3* GetIDxcCompiler() const { return dxcCompiler_.Get(); }

	// IDxcIncludeHandlerの取得
	IDxcIncludeHandler* GetIncludeHandler() const { return includeHandler_.Get(); }

private: /// ---------- メンバ変数 ---------- ///

	ComPtr<IDxcUtils> dxcUtils_;
	ComPtr<IDxcCompiler3> dxcCompiler_;
	ComPtr<IDxcIncludeHandler> includeHandler_;
};