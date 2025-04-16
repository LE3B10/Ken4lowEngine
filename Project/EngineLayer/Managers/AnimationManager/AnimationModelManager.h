#pragma once
#include "DX12Include.h"
#include <BlendModeType.h>
#include "Vector3.h"
#include "Quaternion.h"
#include <ModelData.h>
#include <TransformationMatrix.h>

#include <array>
#include <string>
#include <vector>
#include <numbers>
#include <map>


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Camera;
class Wireframe;


/// -------------------------------------------------------------
///				　アニメーションを管理するクラス
/// -------------------------------------------------------------
class AnimationModelManager
{
private: /// ---------- 構造体 ---------- ///

	// シェーダー側のカメラ構造体
	struct CameraForGPU
	{
		Vector3 worldPosition;
	};

public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static AnimationModelManager* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// 初期化処理
	void Initialize(const std::string& fileName, bool isAnimation = false, bool hasSkeleton = false);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

private: /// ---------- メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// パイプラインの生成
	void CreatePSO();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;
	Wireframe* wireframe_ = nullptr;

	// ブレンドモード
	BlendMode cuurenttype = BlendMode::kBlendModeNone;

	ComPtr <ID3D12RootSignature> rootSignature;
	ComPtr <ID3D12PipelineState> graphicsPipelineState;
	ComPtr <ID3D12Resource> materialResource;
	ComPtr <ID3D12Resource> vertexResource;
	ComPtr <ID3D12Resource> cameraResource;
	ComPtr <ID3D12Resource> indexResource;

	// バッファリソースの作成
	TransformationMatrix* wvpData = nullptr;
	CameraForGPU* cameraData = nullptr;

	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};


private: /// ---------- コピー禁止 ---------- ///
	
	AnimationModelManager() = default;
	~AnimationModelManager() = default;
	AnimationModelManager(const AnimationModelManager&) = delete;
	const AnimationModelManager& operator=(const AnimationModelManager&) = delete;
};

