#pragma once
#include "DX12Include.h"


/// -------------------------------------------------------------
///						ブレンド管理クラス
/// -------------------------------------------------------------
class BlendStateManager
{
public: /// ---------- ブレンドモードの列挙型 ---------- ///
	
	enum class BlendMode
	{
		kBlendModeNone,		// ブレンドなし
		kBlendModeNormal,	// 通常αブレンド、デフォルト。Src * srcA + Dest * (1 - SrcA)
		kBlendModeAdd,		// 加算 Src * SrcA + Dest * 1
		kBlendModeSubtract,	// 減算 Dest * 1 - Src * SrcA
		kBlendModeMultiply, // 乗算 Src * 0 + Dest * Src
		kBlendModeScreen,	// スクリーン Src * (1 - Dest) + Dest * 1
		kcountOfBlendMode,	// 利用してはいけない
	};

public: /// ---------- メンバ関数 ---------- ///
	
	
	void CreateBlend(BlendMode blendMode);

public: /// ---------- ゲッター ---------- ///

	D3D12_RENDER_TARGET_BLEND_DESC& GetBlendDesc();

private: /// ---------- メンバ変数 ---------- ///

	// BlendStateの設定
	D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
};

