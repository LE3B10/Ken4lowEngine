#pragma once
#include "DX12Include.h"

#include <array>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///		パイプラインステートオブジェクトマネージャークラス
/// -------------------------------------------------------------
class PipelineStateManager
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~PipelineStateManager() = default;

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

private: /// ---------- メンバ関数 ---------- ///


};

