#pragma once
#include "DX12Include.h"
#include "BlendModeType.h"


/// -------------------------------------------------------------
///						ブレンド管理クラス
/// -------------------------------------------------------------
class BlendStateManager
{
public: /// ---------- メンバ関数 ---------- ///

	// ブレンドを生成
	void CreateBlend(BlendMode blendMode);

public: /// ---------- ゲッター ---------- ///

	D3D12_RENDER_TARGET_BLEND_DESC& GetBlendDesc() { return blendDesc; }

private: /// ---------- メンバ変数 ---------- ///

	// BlendStateの設定
	D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
};

