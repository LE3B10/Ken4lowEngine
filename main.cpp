#include <format>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>

#include "WinApp.h"
#include "Input.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "D3DResourceLeakChecker.h"
#include "LogString.h"
#include "PipelineStateManager.h"
#include "TextureManager.h"
#include "Sprite.h"
#include "ResourceManager.h"
#include "Object3D.h"

#include "ResourceObject.h"

#include "Vector2.h"
#include "Vector4.h"
#include "Matrix4x4.h"
//#include "VectorMath.h"
#include "MatrixMath.h"
#include "Transform.h"
#include "VertexData.h"
#include "Material.h"
#include "TransformationMatrix.h"
#include "DirectionalLight.h"

#pragma comment(lib, "d3d12.lib")        // Direct3D 12用
#pragma comment(lib, "dxgi.lib")         // DXGI (DirectX Graphics Infrastructure)用
#pragma comment(lib, "dxguid.lib")       // DXGIやD3D12で使用するGUID定義用
#pragma comment(lib, "dxcompiler.lib")   // DXC (DirectX Shader Compiler)用
#pragma comment(lib, "dxguid.lib")       // DXGIデバッグ用 (dxgidebugを使用する場合)

D3DResourceLeakChecker resourceLeakCheck;

// クライアント領域サイズ
static const uint32_t kClientWidth = 1280;
static const uint32_t kClientHeight = 720;

// 円周率
#define pi 3.141592653589793238462643383279502884197169399375105820974944f


// DirectX12のTextureResourceを作る
Microsoft::WRL::ComPtr <ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr <ID3D12Device> device, const DirectX::TexMetadata& metadata)
{
	//1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);									// Textureの幅
	resourceDesc.Height = UINT(metadata.height);								// Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);						// mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);					// 奥行 or 配列Textureの配列行数
	resourceDesc.Format = metadata.format;										// TextureのFormat
	resourceDesc.SampleDesc.Count = 1;											// サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);		// Textureの次元数。普段使っているのは二次元

	//2. 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;								// 細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;		// WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;					// プロセッサの近くに配膳

	//3. Resourceを生成する
	Microsoft::WRL::ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,														// Heapの設定
		D3D12_HEAP_FLAG_NONE,													// Heapの特殊な設定。特になし。
		&resourceDesc,															// /Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,											// データ転送される設定
		nullptr,																// Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));												// 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	/// ---------- シングルトンインスタンス ---------- ///
	WinApp* winApp = WinApp::GetInstance();
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	Input* input = Input::GetInstance();
	ImGuiManager* imguiManager = ImGuiManager::GetInstance();
	TextureManager* textureManager = TextureManager::GetInstance();

	/// ---------- WindowsAPIのウィンドウ作成 ---------- ///
	winApp->CreateMainWindow(kClientWidth, kClientHeight);

	/// ---------- 入力の初期化 ---------- ///
	input->Initialize(winApp);

	/// ---------- DirectXの初期化 ----------///
	dxCommon->Initialize(winApp, kClientWidth, kClientHeight);

	/// ---------- ImGuiManagerの初期化 ---------- ///
	imguiManager->Initialize(winApp, dxCommon);

	/// ---------- PipelineStateManagerの初期化 ---------- ///
	std::unique_ptr<PipelineStateManager> pipelineStateManager = std::make_unique<PipelineStateManager>();
	pipelineStateManager->Initialize(dxCommon);

	// テクスチャのパスをリストで管理
	std::vector<std::string> texturePaths = {
		"Resources/uvChecker.png",
		"Resources/monsterBall.png",
		"Resources/uvChecker.png",
		"Resources/monsterBall.png",
		"Resources/uvChecker.png"
	};

	/// ---------- Spriteの初期化 ---------- ///
	std::vector<std::unique_ptr<Sprite>> sprites;
	for (uint32_t i = 0; i < 5; i++)
	{
		sprites.push_back(std::make_unique<Sprite>());
		sprites[i]->Initialize();
		sprites[i]->SetPosition(Vector2(100.0f * i, 100.0f * i));
	}

	/// ---------- TextureManagerの初期化 ----------///


	/// ---------- Object3Dの初期化 ----------///
	std::unique_ptr<Object3D> object3D = std::make_unique<Object3D>();
	object3D->Initilize();



#pragma region テクスチャファイルを読み込みテクスチャリソースを作成しそれに対してSRVを設定してこれらをデスクリプタヒープにバインド
	// モデルの読み込み
	Object3D::ModelData modelData = Object3D::LoadObjFile("Resources", "axis.obj");

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = TextureManager::LoadTexture("Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr <ID3D12Resource> textureResource = CreateTextureResource(dxCommon->GetDevice(), metadata);
	Microsoft::WRL::ComPtr <ID3D12Resource> intermediateResouece1 = TextureManager::UploadTextureData(textureResource.Get(), mipImages, dxCommon->GetDevice(), dxCommon->GetCommandList());

	// 1つ目のテクスチャのSRV設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;				//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// 1つ目のテクスチャのSRVのデスクリプタヒープへのバインド
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetDescriptorHeap()->GetCPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorHeap()->GetDescriptorSizeSRV(), 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetDescriptorHeap()->GetGPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorHeap()->GetDescriptorSizeSRV(), 1);
	textureSrvHandleCPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);


	// 2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = TextureManager::LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr <ID3D12Resource> textureResource2 = CreateTextureResource(dxCommon->GetDevice(), metadata2);
	Microsoft::WRL::ComPtr <ID3D12Resource> intermediateResouece2 = TextureManager::UploadTextureData(textureResource2.Get(), mipImages2, dxCommon->GetDevice(), dxCommon->GetCommandList());

	// 2つ目のテクスチャのSRV設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;				//2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// 2つ目のテクスチャのSRVのデスクリプタヒープへのバインド
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetDescriptorHeap()->GetCPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorHeap()->GetDescriptorSizeSRV(), 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetDescriptorHeap()->GetGPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorHeap()->GetDescriptorSizeSRV(), 2);
	textureSrvHandleCPU2.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU2.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);
#pragma endregion


#pragma region 球体の頂点データを格納するためのバッファリソースを生成
	// 分割数
	uint32_t kSubdivision = 20;
	// 緯度・経度の分割数に応じた角度の計算
	float kLatEvery = pi / float(kSubdivision);
	float kLonEvery = 2.0f * pi / float(kSubdivision);
	// 球体の頂点数の計算
	uint32_t TotalVertexCount = kSubdivision * kSubdivision * 6;

	// バッファリソースの作成
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(VertexData) * (modelData.vertices.size() + TotalVertexCount));
#pragma endregion


#pragma region 頂点バッファデータの開始位置サイズおよび各頂点のデータ構造を指定
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};																 // 頂点バッファビューを作成する
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();									 // リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * (modelData.vertices.size() + TotalVertexCount));	 // 使用するリソースのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);														 // 1頂点あたりのサイズ
#pragma endregion


#pragma region 球体の頂点位置テクスチャ座標および法線ベクトルを計算し頂点バッファに書き込む
	VertexData* vertexData = nullptr;																			 // 頂点リソースにデータを書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));										 // 書き込むためのアドレスを取得

	// モデルデータの頂点データをコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
	////左下
	//vertexData[0].position = { -0.5f,-0.5f,0.0f,1.0f };
	//vertexData[0].texcoord = { 0.0f,1.0f };
	////上
	//vertexData[1].position = { 0.0f,0.5f,0.0f,1.0f };
	//vertexData[1].texcoord = { 0.5f,0.0f };
	////右下
	//vertexData[2].position = { 0.5f,-0.5f,0.0f,1.0f };
	//vertexData[2].texcoord = { 1.0f,1.0f };

	////左下2
	//vertexData[3].position = { -0.5f,-0.5f,0.5f,1.0f };
	//vertexData[3].texcoord = { 0.0f,1.0f };
	////上2
	//vertexData[4].position = { 0.0f,0.0f,0.0f,1.0f };
	//vertexData[4].texcoord = { 0.5f,0.0f };
	////右下2
	//vertexData[5].position = { 0.5f,-0.5f,-0.5f,1.0f };
	//vertexData[5].texcoord = { 1.0f,1.0f };
	// Resourceにデータを書き込む・頂点データの更新

	// 球体の頂点データをコピー
	VertexData* sphereVertexData = vertexData + modelData.vertices.size();
	auto calculateVertex = [](float lat, float lon, float u, float v) {
		VertexData vertex;
		vertex.position = { cos(lat) * cos(lon), sin(lat), cos(lat) * sin(lon), 1.0f };
		vertex.texcoord = { u, v };
		vertex.normal = { vertex.position.x, vertex.position.y, vertex.position.z };
		return vertex;
		};

	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		float lat = -pi / 2.0f + kLatEvery * latIndex; // θ
		float nextLat = lat + kLatEvery;

		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			float u = float(lonIndex) / float(kSubdivision);
			float v = 1.0f - float(latIndex) / float(kSubdivision);
			float lon = lonIndex * kLonEvery; // Φ
			float nextLon = lon + kLonEvery;

			uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;

			// 6つの頂点を計算
			sphereVertexData[start + 0] = calculateVertex(lat, lon, u, v);
			sphereVertexData[start + 1] = calculateVertex(nextLat, lon, u, v - 1.0f / float(kSubdivision));
			sphereVertexData[start + 2] = calculateVertex(lat, nextLon, u + 1.0f / float(kSubdivision), v);
			sphereVertexData[start + 3] = calculateVertex(nextLat, nextLon, u + 1.0f / float(kSubdivision), v - 1.0f / float(kSubdivision));
			sphereVertexData[start + 4] = calculateVertex(lat, nextLon, u + 1.0f / float(kSubdivision), v);
			sphereVertexData[start + 5] = calculateVertex(nextLat, lon, u, v - 1.0f / float(kSubdivision));
		}
	}

	// アンマップ
	vertexResource->Unmap(0, nullptr);
#pragma endregion

	bool useMonsterBall = true;

	//ウィンドウのｘボタンが押されるまでループ
	while (!winApp->ProcessMessage())
	{
		// 入力の更新
		input->Update();

		if (input->TriggerKey(DIK_0))
		{
			OutputDebugStringA("Hit 0 \n");
		}

		/// ---------- ImGuiフレーム開始 ---------- ///
		imguiManager->BeginFrame();

		// 開発用のUIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();

		ImGui::Begin("Test Window");

		object3D->DrawImGui();

		for (uint32_t i = 0; i < sprites.size(); i++)
		{
			ImGui::PushID(i); // スプライトごとに異なるIDを設定
			if (ImGui::TreeNode(("Sprite" + std::to_string(i)).c_str()))
			{
				Vector2 position = sprites[i]->GetPosition();
				ImGui::DragFloat2("Position", &position.x, 1.0f);
				sprites[i]->SetPosition(position);

				float rotation = sprites[i]->GetRotation();
				ImGui::SliderAngle("Rotation", &rotation);
				sprites[i]->SetRotation(rotation);

				Vector2 size = sprites[i]->GetSize();
				ImGui::DragFloat2("Size", &size.x, 1.0f);
				sprites[i]->SetSize(size);

				ImGui::TreePop();
			}
			ImGui::PopID(); // IDを元に戻す
		}

		ImGui::Checkbox("useMonsterBall", &useMonsterBall);

		ImGui::End();

		/// ---------- ImGuiフレーム終了 ---------- ///
		imguiManager->EndFrame();

		// 3Dオブジェクトの更新処理
		object3D->Update();

		// スプライトの更新処理
		for (auto& sprite : sprites)
		{
			sprite->Update();
		}

		// 描画開始処理
		dxCommon->BeginDraw();

		// ディスクリプタヒープの設定
		textureManager->SetGraphicsRootDescriptorTable(dxCommon->GetCommandList());

		/*-----シーン（モデル）の描画設定と描画-----*/
		// ルートシグネチャとパイプラインステートの設定
		pipelineStateManager->SetGraphicsPipeline(dxCommon->GetCommandList());


		// 頂点バッファの設定とプリミティブトポロジの設定
		dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // モデル用VBV

		// 3Dオブジェクトのバッファーデータを設定
		object3D->SetObject3DBufferData(dxCommon->GetCommandList());

		// ディスクリプタテーブルの設定
		dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2);

		// モデルの描画
		dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

		// 形状を設定。PSOに設定るものとはまた別。同じものを設定すると考える
		dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		///*-----スプライトの描画設定と描画-----*/
		for (auto& sprite : sprites)
		{
			sprite->SetSpriteBufferData(dxCommon->GetCommandList());
			dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			sprite->DrawCall(dxCommon->GetCommandList());
		}

		/*-----ImGuiの描画-----*/
		// ImGui描画のコマンドを積む
		imguiManager->Draw();

		// 描画終了処理
		dxCommon->EndDraw();
	}

	winApp->Finalize();
	dxCommon->Finalize();
	imguiManager->Finalize();

	return 0;
}