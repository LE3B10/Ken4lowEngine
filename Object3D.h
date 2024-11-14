#pragma once
#include "DX12Include.h"
#include "DirectionalLight.h"
#include "Transform.h"
#include "TransformationMatrix.h"
#include "TextureManager.h"
#include "Material.h"
#include "VertexData.h"
#include "ModelData.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// 円周率
#define pi 3.141592653589793238462643383279502884197169399375105820974944f

/// -------------------------------------------------------------
///						オブジェクト3Dクラス
/// -------------------------------------------------------------
class Object3D
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// ImGui
	void DrawImGui();

	// ドローコール
	void DrawCall(ID3D12GraphicsCommandList* commandList, UINT rootParameter, D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU);

	// 共通描画設定
	void SetObject3DBufferData(ID3D12GraphicsCommandList* commandList);

private: /// ---------- メンバ変数 ---------- ///

	Transform transform;
	Transform cameraTransform;

	//データを書き込む
	TransformationMatrix* wvpData = nullptr;

	// OBJファイルのデータ
	ModelData modelData;
	// バッファリソースの作成
	ComPtr <ID3D12Resource> vertexResource;
	// 頂点リソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	DirectionalLight* directionalLightData = nullptr;

	ComPtr <ID3D12Resource> materialResource;
	ComPtr <ID3D12Resource> wvpResource;
	ComPtr <ID3D12Resource> directionalLightResource;

};

