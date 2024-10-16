#pragma once
#include "DirectXInclude.h"

/// -------------------------------------------------------------
///			ラスタライザに対する設定を管理するクラス
/// -------------------------------------------------------------
class RasterizerStateManager
{
public:
	/// ---------- メンバ関数 ---------- ///

	// 設定処理
	void SettingProcess();

	// ゲッター
	D3D12_RASTERIZER_DESC GetRasterizerDesc() const;

private:
	/// ---------- メンバ変数 ---------- ///

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};

};

