#pragma once
#include "DX12Include.h"
#include "LightManager.h"
#include "Camera.h"
#include <BlendModeType.h>

#include <memory>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///				　	オブジェクト3Dの共通クラス
/// -------------------------------------------------------------
class Object3DCommon
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static Object3DCommon* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// 更新処理
	void Update();

	// ImGui
	void DrawImGui();

public: /// ---------- 設定 ---------- ///

	// 共通描画設定
	void SetRenderSetting();

	// デフォルトカメラを取得
	void SetDefaultCamera(Camera* defaultCamera) { defaultCamera_ = defaultCamera; }

	// デバッグカメラ有効かどうか
	void SetDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

public:	/// ---------- 取得 ---------- ///

	// デフォルトカメラを取得
	Camera* GetDefaultCamera() const { return defaultCamera_; }

	// デバッグカメラを取得
	bool GetDebugCamera() { return isDebugCamera_; }

private: /// ---------- メンバ変数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// PSOを生成
	void CreatePSO();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	
	// デフォルトカメラ
	Camera* defaultCamera_ = nullptr;

	BlendMode blendMode_ = BlendMode::kBlendModeNone;

	ComPtr <ID3D12PipelineState> graphicsPipelineState_;
	ComPtr <ID3DBlob> signatureBlob_;
	ComPtr <ID3DBlob> errorBlob_;
	ComPtr <ID3D12RootSignature> rootSignature_;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_{};

	// ライトマネージャ
	std::unique_ptr<LightManager> lightManager_;

	// ビュー射影行列
	Matrix4x4 viewProjectionMatrix_;
	Matrix4x4 debugViewProjectionMatrix_;

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;

private: /// ---------- コピー禁止 ---------- ///

	Object3DCommon() = default;
	~Object3DCommon() = default;
	Object3DCommon(const Object3DCommon&) = delete;
	Object3DCommon& operator=(const Object3DCommon&) = delete;
};

