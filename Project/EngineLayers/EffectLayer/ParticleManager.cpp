#include "ParticleManager.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <MatrixMath.h>
#include <LogString.h>
#include <SRVManager.h>
#include <TextureManager.h>
#include "ShaderManager.h"
#include "Camera.h"

/// -------------------------------------------------------------
///				    シングルトンインスタンス
/// -------------------------------------------------------------
ParticleManager* ParticleManager::GetInstance()
{
	static ParticleManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///				           初期化処理
/// -------------------------------------------------------------
void ParticleManager::Initialize(DirectXCommon* dxCommon, Camera* camera)
{
	shaderManager = new ShaderManager();

	// 引数でDirectXCommonとSRVマネージャーのポインタを受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;
	camera_ = camera;


	srvManager_ = SRVManager::GetInstance();
//	srvManager_->Initialize(dxCommon);


	// ランダムエンジンの初期化
	randomEngin.seed(seedGeneral());

	// パイプライン生成
	CreatePSO();

	// 頂点データの初期化
	InitializeVertexData();

	// マテリアルデータの初期化
	InitializeMaterialData();
}


/// -------------------------------------------------------------
///				    パーティクルグループの生成
/// -------------------------------------------------------------
void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
	// 登録済みの名前かチェックしてassert
	assert(particleGroups.find(name) == particleGroups.end() && "Particle group alread exests!");

	// 新たな空のパーティクルグループを作成し、コンテナに登録
	ParticleGroup group{};
	group.materialData.textureFilePath = textureFilePath;
	group.materialData.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath);

	// インスタンスバッファ作成
	group.instancebuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kNumMaxInstance);
	group.instancebuffer->Map(0, nullptr, reinterpret_cast<void**>(&group.mappedData));

	// 初期化
	for (uint32_t i = 0; i < kNumMaxInstance; ++i)
	{
		group.mappedData[i].WVP = MakeIdentity();
		group.mappedData[i].World = MakeIdentity();
	}

	// インスタンシング用SRVの生成
	group.srvIndex = srvManager_->Allocate();
	srvManager_->CreateSRVForStructureBuffer(group.srvIndex, group.instancebuffer.Get(), kNumMaxInstance, sizeof(ParticleForGPU));

	particleGroups.emplace(name, group);
}


/// -------------------------------------------------------------
///				           　更新処理
/// -------------------------------------------------------------
void ParticleManager::Update()
{
	// ビュー行列とプロジェクション行列をカメラから取得
	Matrix4x4 cameraMatrix = MakeAffineMatrix({ 1.0f,1.0f,1.0f }, camera_->GetRotate(), camera_->GetTranslate());
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

	// Y軸でπ/2回転させる
	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
	Matrix4x4 billboardMatrix{};

	// ビルボード行列が有効なとき
	if (useBillboard)
	{
		// ビルボード行列の計算
		billboardMatrix = Multiply(backToFrontMatrix, cameraMatrix);
		billboardMatrix.m[3][0] = 0.0f; // 平行移動成分はいらない
		billboardMatrix.m[3][1] = 0.0f;
		billboardMatrix.m[3][2] = 0.0f;
	}
	else
	{
		billboardMatrix = MakeIdentity();
	}

	// グループ内のすべてのパーティクルについて処理する
	for (auto& group : particleGroups)
	{
		// 板ポリ
		for (std::list<Particle>::iterator particleIterator = group.second.particles.begin(); particleIterator != group.second.particles.end(); )
		{
			if ((*particleIterator).lifeTime <= (*particleIterator).currentTime)
			{
				// 生存時間を過ぎていたら更新せず描画対象にしない
				particleIterator = group.second.particles.erase(particleIterator);
				continue;
			}

			if (group.second.numParticles < kNumMaxInstance)
			{
				// 速度を適用
				(*particleIterator).transform.translate += (*particleIterator).velocity * kDeltaTime;
				(*particleIterator).currentTime += kDeltaTime; // 経過時間を足す

				// worldMatrixを求める
				Matrix4x4 worldMatrix = MakeAffineMatrix((*particleIterator).transform.scale, (*particleIterator).transform.rotate, (*particleIterator).transform.translate);
				Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);

				group.second.mappedData[group.second.numParticles].WVP = worldViewProjectionMatrix;
				group.second.mappedData[group.second.numParticles].World = worldMatrix;
				group.second.mappedData[group.second.numParticles].color = (*particleIterator).color;

				float alpha = 1.0f - ((*particleIterator).currentTime / (*particleIterator).lifeTime);

				group.second.mappedData[group.second.numParticles].color.w = alpha;
				++group.second.numParticles; // 生きているParticleの数を1つカウントする
			}

			++particleIterator; // 次のパーティクルに進める
		}
	}
}


/// -------------------------------------------------------------
///				           　描画処理
/// -------------------------------------------------------------
void ParticleManager::Draw()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// ルートシグネチャを設定
	commandList->SetGraphicsRootSignature(rootSignature.Get());

	// パイプラインステートオブジェクト (PSO) を設定
	commandList->SetPipelineState(graphicsPipelineState.Get());

	//プリミティブトポロジを設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// VBVを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

	// すべてのパーティクルグループについて処理する
	for (auto& group : particleGroups)
	{
		if (group.second.numParticles == 0) continue;

		// マテリアルCBVを設定
		commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

		// テクスチャのSRVのデスクリプタテーブルを設定
		commandList->SetGraphicsRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(group.second.srvIndex));

		// インスタンシングデータのSRVのデスクリプタテーブルを設定
		commandList->SetGraphicsRootDescriptorTable(2, group.second.materialData.gpuHandle);

		// インスタンシング描画
		commandList->DrawInstanced(UINT(modelData.vertices.size()), group.second.numParticles, 0, 0);

		// インスタンス数をリセット
		group.second.numParticles = 0;
	}
}


/// -------------------------------------------------------------
///					　		解放処理
/// -------------------------------------------------------------
void ParticleManager::Finalize()
{
	// particleGroups 内のリソースを解放
	for (auto& [key, group] : particleGroups)
	{
		group.instancebuffer.Reset(); // ComPtr の解放
		group.mappedData = nullptr;  // ポインタを無効化
	}
	particleGroups.clear();
}


/// -------------------------------------------------------------
///					　パーティクル射出処理
/// -------------------------------------------------------------
void ParticleManager::Emit(const std::string name, const Vector3 position, uint32_t count)
{
	// パーティクルグループが存在するかどうか
	assert(particleGroups.find(name) != particleGroups.end() && "Particle Group is not found");

	// パーティクルグループを取得
	ParticleGroup& particleGroup = particleGroups[name];

	// 最大数に達している場合
	if (particleGroup.particles.size() >= count) return;

	// パーティクルの生成
	for (uint32_t index = 0; index < count; ++index)
	{
		// パーティクルの生成と追加
		particleGroup.particles.push_back(MakeNewParticle(randomEngin, position));
	}
}


/// -------------------------------------------------------------
///					　ルートシグネチャの生成処理
/// -------------------------------------------------------------
void ParticleManager::CreateRootSignature()
{
	HRESULT hr{};

	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Particle用のRootSignature
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRangeForInstancing[0].NumDescriptors = 1;		// 数は1つ
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;								// CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								// PixelShaderを使う
	rootParameters[0].Descriptor.ShaderRegister = 0;												// レジスタ番号０とバインド

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;					// DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;							// VertexShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;				// Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); // Tableで利用する

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;					// DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								// PixelxShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;				// Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); // Tableで利用する

	descriptionRootSignature.pParameters = rootParameters;											//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);								//配列の長さ

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;										//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;									//0～1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;									//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;													//ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;															//レジスタ番号０を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr <ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///			　パイプラインシグネイチャーの生成処理
/// -------------------------------------------------------------
void ParticleManager::CreatePSO()
{
	HRESULT hr{};

	// ルートシグネチャの生成
	CreateRootSignature();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//BlendStateの設定
	D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// 各ブレンドモードの設定を行う
	switch (cuurenttype)
	{
		// ブレンドモードなし
	case BlendMode::kBlendModeNone:

		blendDesc.BlendEnable = false;
		break;

		// 通常αブレンドモード
	case BlendMode::kBlendModeNormal:

		// ノーマル
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
		break;

		// 加算ブレンドモード
	case BlendMode::kBlendModeAdd:

		// 加算
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlend = D3D12_BLEND_ONE;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;		  // アルファのソースはそのまま
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	  // アルファの加算操作
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;	  // アルファのデスティネーションは無視
		break;

		// 減算ブレンドモード
	case BlendMode::kBlendModeSubtract:

		// 減算
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		blendDesc.DestBlend = D3D12_BLEND_ONE;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;		 // アルファのソースはそのまま
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	 // アルファの加算操作
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;	 // アルファのデスティネーションは無
		break;

		// 乗算ブレンドモード
	case BlendMode::kBlendModeMultiply:

		// 乗算
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_ZERO;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlend = D3D12_BLEND_SRC_COLOR;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;		 // アルファのソースはそのまま
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	 // アルファの加算操作
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;	 // アルファのデスティネーションは無視
		break;

		// スクリーンブレンドモード
	case BlendMode::kBlendModeScreen:

		// スクリーン
		blendDesc.BlendEnable = true;
		blendDesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.DestBlend = D3D12_BLEND_ONE;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;		 // アルファのソースはそのまま
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	 // アルファの加算操作
		blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;	 // アルファのデスティネーションは無視
		break;

		// 無効なブレンドモード
	default:
		// 無効なモードの処理
		assert(false && "Invalid Blend Mode");
		break;
	}

	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//Shaderをコンパイルする
	Microsoft::WRL::ComPtr <IDxcBlob> vertexShaderBlob = shaderManager->CompileShader(L"Resources/Shaders/Particle.VS.hlsl", L"vs_6_0", dxCommon_->GetIDxcUtils(), dxCommon_->GetIDxcCompiler(), dxCommon_->GetIncludeHandler());
	assert(vertexShaderBlob != nullptr);

	//Pixelをコンパイルする
	Microsoft::WRL::ComPtr <IDxcBlob> pixelShaderBlob = shaderManager->CompileShader(L"Resources/Shaders/Particle.PS.hlsl", L"ps_6_0", dxCommon_->GetIDxcUtils(), dxCommon_->GetIDxcCompiler(), dxCommon_->GetIncludeHandler());
	assert(pixelShaderBlob != nullptr);

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//Depthを描くのをやめる
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};												// パイプラインステートディスクリプタの初期化
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();												// RootSgnature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;													// InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),vertexShaderBlob->GetBufferSize() };	// VertexDhader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() };	// PixelShader
	graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blendDesc;															// BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;													// RasterizeerState

	//レンダーターゲットの設定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	//利用するトポロジー（形態）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;					// プリミティブトポロジーの設定

	// サンプルマスクとサンプル記述子の設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilステートの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// パイプラインステートオブジェクトの生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///					　頂点データの生成処理
/// -------------------------------------------------------------
void ParticleManager::InitializeVertexData()
{
	// 6つの頂点を定義して四角形を表現
	modelData.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });  // 左上
	modelData.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelData.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下

	modelData.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下
	modelData.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelData.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右下

	// バッファリソースの作成
	vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * modelData.vertices.size());

	// 頂点バッファビュー（VBV）作成
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();									 // リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());	 // 使用するリソースのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);														 // 1頂点あたりのサイズ

	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));										 // 書き込むためのアドレスを取得

	// モデルデータの頂点データをコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
}


/// -------------------------------------------------------------
///					マテリアルデータの生成処理
/// -------------------------------------------------------------
void ParticleManager::InitializeMaterialData()
{
	//マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
	//マテリアルにデータを書き込む
	Material* materialData = nullptr;
	//書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	//今回は赤を書き込んでみる
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = false;
	//UVTramsform行列を単位行列で初期化
	materialData->uvTransform = MakeIdentity();
}


/// -------------------------------------------------------------
///						パーティクル生成処理
/// -------------------------------------------------------------
Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate)
{
	Particle particle;

	// 一様分布生成期を使って乱数を生成
	std::uniform_real_distribution<float> distribution(-1.0, 1.0f);
	std::uniform_real_distribution<float> distColor(0.0, 1.0f);
	std::uniform_real_distribution<float> distTime(1.0, 3.0f);

	// 位置と速度を[-1, 1]でランダムに初期化
	particle.transform.scale = { 1.0f, 1.0f, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
	particle.transform.translate = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };

	// 発生場所を計算
	Vector3 randomTranslate{ distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.transform.translate = translate + randomTranslate;

	// 色を[0, 1]でランダムに初期化
	particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };

	// パーティクル生成時にランダムに1秒～3秒の間生存
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0;
	particle.velocity = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };

	return particle;
}

