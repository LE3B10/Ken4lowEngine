#pragma once
#include "DX12Include.h"
#include "BlendModeType.h"

#include <array>
#include <unordered_map>
#include <string>


/// -------------------------------------------------------------
///			ブレンドステートを生成するファクトリークラス
/// -------------------------------------------------------------
class BlendStateFactory
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス取得（必要なら）
	static BlendStateFactory* GetInstance();

	// 初期化処理（全ブレンド設定を生成）
	void Initialize();

	// BlendMode から RenderTarget[0] 用の BlendDesc を取得
	const D3D12_RENDER_TARGET_BLEND_DESC& GetBlendDesc(BlendMode blendMode) const;

	// カスタムブレンドの登録と取得（オプション）
	void RegisterCustomBlend(const std::string& name, const D3D12_RENDER_TARGET_BLEND_DESC& desc);
	const D3D12_RENDER_TARGET_BLEND_DESC* GetCustomBlend(const std::string& name) const;

private: /// ---------- メンバ変数 ---------- ///

	std::array<D3D12_RENDER_TARGET_BLEND_DESC, blendModeNum> blendDescs_;
	std::unordered_map<std::string, D3D12_RENDER_TARGET_BLEND_DESC> customBlends_;

private: /// ---------- コピー禁止 ---------- ///

	BlendStateFactory() = default;
	~BlendStateFactory() = default;
	BlendStateFactory(const BlendStateFactory&) = delete;
	BlendStateFactory& operator=(const BlendStateFactory&) = delete;
};

