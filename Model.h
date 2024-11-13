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

/// -------------------------------------------------------------
///						　3Dモデルクラス
/// -------------------------------------------------------------
class Model
{
public: /// ---------- 構造体 ---------- ///

	

public: /// ---------- メンバ関数 ---------- ///

	void Initialize();
	
private: /// ---------- メンバ変数 ---------- ///

	Transform transform;
	Transform cameraTransform;

	//データを書き込む
	TransformationMatrix* wvpData = nullptr;

	// OBJファイルのデータ
	ModelData modelData;
	// バッファリソースの作成
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource;
	// 頂点リソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	DirectionalLight* directionalLightData = nullptr;

	Microsoft::WRL::ComPtr <ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr <ID3D12Resource> wvpResource;
	Microsoft::WRL::ComPtr <ID3D12Resource> directionalLightResource;

};

