#pragma once
#include "DX12Include.h"
#include "VertexData.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

/// -------------------------------------------------------------
///						　3Dモデルクラス
/// -------------------------------------------------------------
class Model
{
public: /// ---------- 構造体 ---------- ///

	

public: /// ---------- メンバ関数 ---------- ///


	
private: /// ---------- メンバ変数 ---------- ///

	// OBJファイルのデータ
	//ModelData modelData;
	// バッファリソースの作成
	ComPtr <ID3D12Resource> vertexResource;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// 頂点リソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;

	ComPtr <ID3D12Resource> materialResource;
	ComPtr <ID3D12Resource> wvpResource;
	ComPtr <ID3D12Resource> directionalLightResource;
};

