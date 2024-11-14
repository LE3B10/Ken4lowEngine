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
#include "ModelManager.h"

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

D3DResourceLeakChecker resourceLeakCheck;

// クライアント領域サイズ
static const uint32_t kClientWidth = 1280;
static const uint32_t kClientHeight = 720;

// 円周率
#define pi 3.141592653589793238462643383279502884197169399375105820974944f

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	/// ---------- シングルトンインスタンス ---------- ///
	WinApp* winApp = WinApp::GetInstance();
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	Input* input = Input::GetInstance();
	ImGuiManager* imguiManager = ImGuiManager::GetInstance();
	TextureManager* textureManager = TextureManager::GetInstance();
	ModelManager* modelManager = ModelManager::GetInstance();

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
	};

	/// ---------- TextureManagerの初期化 ----------///
	for (auto& texture : texturePaths)
	{
		textureManager->LoadTexture(texture);
	}

	/// ---------- Spriteの初期化 ---------- ///
	std::vector<std::unique_ptr<Sprite>> sprites;
	for (uint32_t i = 0; i < 1; i++)
	{
		sprites.push_back(std::make_unique<Sprite>());
		sprites[i]->Initialize(texturePaths[i % 2]);
		sprites[i]->SetPosition(Vector2(100.0f * i, 100.0f * i));
	}

	/// ---------- Object3Dの初期化 ----------///
	std::unique_ptr<Object3D> object3D = std::make_unique<Object3D>();
	object3D->Initialize();

#pragma region テクスチャファイルを読み込みテクスチャリソースを作成しそれに対してSRVを設定してこれらをデスクリプタヒープにバインド

	/// ---------- モデル用 ---------- ///

	// モデルの読み込み
	ModelData modelData = modelManager->LoadObjFile("Resources", "plane.obj");

	// 2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = TextureManager::LoadTextureData(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr <ID3D12Resource> textureResource2 = TextureManager::CreateTextureResource(dxCommon->GetDevice(), metadata2);
	Microsoft::WRL::ComPtr <ID3D12Resource> intermediateResouece2 = TextureManager::UploadTextureData(textureResource2.Get(), mipImages2, dxCommon->GetDevice(), dxCommon->GetCommandList());

	// 2つ目のテクスチャのSRV設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;				//2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// 2つ目のテクスチャのSRVのデスクリプタヒープへのバインド
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetDescriptorHeap()->GetCPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorHeap()->GetDescriptorSizeSRV(), 3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetDescriptorHeap()->GetGPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), dxCommon->GetDescriptorHeap()->GetDescriptorSizeSRV(), 3);
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
		ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon->GetSRVDescriptorHeap() };
		dxCommon->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);

		/*-----シーン（モデル）の描画設定と描画-----*/
		// ルートシグネチャとパイプラインステートの設定
		pipelineStateManager->SetGraphicsPipeline(dxCommon->GetCommandList());


		// 頂点バッファの設定とプリミティブトポロジの設定
		dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // モデル用VBV

		// 3Dオブジェクトデータ設定
		{
			object3D->SetObject3DBufferData(dxCommon->GetCommandList());
			object3D->DrawCall(dxCommon->GetCommandList(), 2, textureSrvHandleGPU2);
		}

		///*-----スプライトの描画設定と描画-----*/
		for (auto& sprite : sprites)
		{
			sprite->SetSpriteBufferData(dxCommon->GetCommandList());
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