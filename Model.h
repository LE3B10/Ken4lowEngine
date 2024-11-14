#pragma once
#include "DX12Include.h"
#include "VertexData.h"
#include "ModelData.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "Transform.h"
#include "TransformationMatrix.h"
#include "DirectionalLight.h"
#include "Material.h"

class DirectXCommon;

/// -------------------------------------------------------------
///						　3Dモデルクラス
/// -------------------------------------------------------------
class Model
{
public: /// ---------- 構造体 ---------- ///

	

public: /// ---------- メンバ関数 ---------- ///

	void Initialize();

	// 頂点データの初期化処理
	void InitializeMaterial(DirectXCommon* dxCommon);

	// マテリアルの初期化処理
	void InitializeTransfomation(DirectXCommon* dxCommon);

	// 平行光源の初期化処理
	void ParalllelLightSorce(DirectXCommon* dxCommon);
	
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

