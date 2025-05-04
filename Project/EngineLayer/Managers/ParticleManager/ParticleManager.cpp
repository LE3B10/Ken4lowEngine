#include "ParticleManager.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <LogString.h>
#include <SRVManager.h>
#include <TextureManager.h>
#include "ShaderManager.h"
#include "Camera.h"
#include <ImGuiManager.h>
#include <DebugCamera.h>

std::vector<ParticleManager::WindZone> windZones = {
	{ { {-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f} }, {0.1f, 0.0f, 0.0f} },
	{ { {10.0f, -5.0f, -5.0f}, {15.0f, 5.0f, 5.0f} }, {0.0f, 0.0f, 0.1f} }
};

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
	// 引数でDirectXCommonとSRVマネージャーのポインタを受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;
	camera_ = camera;

	srvManager_ = SRVManager::GetInstance();

	// ランダムエンジンの初期化
	randomEngin.seed(seedGeneral());

	accelerationField.acceleration = { 15.0f, 0.0f, 0.0f };
	accelerationField.area.min = { -10.0f, -10.0f, -30.0f };
	accelerationField.area.max = { 10.0f, 10.0f, 30.0f };

	// パイプライン生成
	CreatePSO();

	// 頂点データの初期化
	//mesh_.Initialize();

	//// リングの頂点データを生成
	//mesh_.CreateVertexData();

	// シリンダーの頂点データを生成
	mesh_.InitializeCylinder();

	// マテリアルデータの初期化
	material_.Initialize();
}


/// -------------------------------------------------------------
///				    パーティクルグループの生成
/// -------------------------------------------------------------
void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
	const std::string FilePath = "Resources/" + textureFilePath;

	TextureManager::GetInstance()->LoadTexture(FilePath);

	// 登録済みの名前かチェックしてassert
	assert(particleGroups.find(name) == particleGroups.end() && "Particle group alread exests!");

	// 新たな空のパーティクルグループを作成し、コンテナに登録
	ParticleGroup group{};
	group.materialData.textureFilePath = FilePath;
	group.materialData.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(FilePath);

	// インスタンスバッファ作成
	group.instancebuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kNumMaxInstance);
	group.instancebuffer->Map(0, nullptr, reinterpret_cast<void**>(&group.mappedData));

	// 初期化
	for (uint32_t i = 0; i < kNumMaxInstance; ++i)
	{
		group.mappedData[i].WVP = Matrix4x4::MakeIdentity();
		group.mappedData[i].World = Matrix4x4::MakeIdentity();
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
	Matrix4x4 cameraMatrix = Matrix4x4::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, camera_->GetRotate(), camera_->GetTranslate());
	Matrix4x4 viewMatrix = Matrix4x4::Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Matrix4x4::Multiply(viewMatrix, projectionMatrix);

	// デバッグカメラが有効ならそちらを使う
	if (isDebugCamera_)
	{
#ifdef _DEBUG
		debugViewProjectionMatrix_ = DebugCamera::GetInstance()->GetViewProjectionMatrix();
		viewProjectionMatrix = debugViewProjectionMatrix_;
#endif
	}

	// ビルボード用行列（回転行列のみ）
	Matrix4x4 backToFrontMatrix = Matrix4x4::MakeIdentity(); // 必要ならY軸回転行列に置換
	Matrix4x4 billboardMatrix = Matrix4x4::Multiply(backToFrontMatrix, cameraMatrix);
	billboardMatrix.m[3][0] = billboardMatrix.m[3][1] = billboardMatrix.m[3][2] = 0.0f;

	// パーティクルグループごとに更新処理
	for (auto& group : particleGroups)
	{
		group.second.numParticles = 0;

		auto& particles = group.second.particles;
		for (auto particleIt = particles.begin(); particleIt != particles.end(); )
		{
			auto& particle = *particleIt;

			// 寿命切れパーティクルを削除
			if (particle.currentTime >= particle.lifeTime)
			{
				particleIt = particles.erase(particleIt);
				continue;
			}

			// 更新対象としてカウント
			if (group.second.numParticles < kNumMaxInstance)
			{
				// 経過割合
				float t = particle.currentTime / particle.lifeTime;

				// スケール補間
				particle.transform.scale_ = Vector3::Lerp(particle.startScale, particle.endScale, t);

				// 位置更新
				particle.transform.translate_ += particle.velocity * kDeltaTime;
				particle.currentTime += kDeltaTime;

				// リングを回転させるための処理
				//particle.transform.rotate_.z += 1.5f * kDeltaTime; // 秒間約86度の回転

				// シリンダーを回転させるための処理
				particle.transform.rotate_.y += 1.5f * kDeltaTime; // 秒間約86度の回転

				// 行列更新（transformに任せる）
				particle.transform.UpdateMatrix(viewProjectionMatrix, useBillboard, billboardMatrix);

				// 書き込み
				auto& instance = group.second.mappedData[group.second.numParticles];
				instance.WVP = particle.transform.GetWVPMatrix();
				instance.World = particle.transform.GetWorldMatrix();

				// 色とアルファ
				instance.color = particle.color;
				instance.color.w = 1.0f - t;

				// 風の影響（必要なら）
				if (isWind)
				{
					for (const auto& zone : windZones)
					{
						if (IsCollision(accelerationField.area, particle.transform.translate_))
						{
							particle.velocity += zone.strength;
						}
					}
				}

				++group.second.numParticles;
			}

			++particleIt;
		}
	}

	// マテリアル更新
	material_.Update();
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
	commandList->IASetVertexBuffers(0, 1, &mesh_.GetVertexBufferView());

	// すべてのパーティクルグループについて処理する
	for (auto& group : particleGroups)
	{
		if (group.second.numParticles == 0) continue;

		// マテリアルCBVを設定
		material_.SetPipeline();

		// テクスチャのSRVのデスクリプタテーブルを設定
		commandList->SetGraphicsRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(group.second.srvIndex));

		// インスタンシングデータのSRVのデスクリプタテーブルを設定
		commandList->SetGraphicsRootDescriptorTable(2, group.second.materialData.gpuHandle);

		// インスタンシング描画
		mesh_.Draw(group.second.numParticles);

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
void ParticleManager::Emit(const std::string name, const Vector3 position, uint32_t count, ParticleEffectType type)
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
		particleGroup.particles.push_back(MakeNewParticle(randomEngin, position, type));
	}
}


/// -------------------------------------------------------------
///					　パーティクルImGui描画
/// -------------------------------------------------------------
void ParticleManager::DrawImGui()
{
	// ImGuiでuseBillboardの切り替えボタンを追加
	ImGui::Begin("Particle Manager"); // ウィンドウの開始
	if (ImGui::Button(useBillboard ? "Disable Billboard" : "Enable Billboard"))
	{
		// ボタンが押されたらuseBillboardの値を切り替える
		useBillboard = !useBillboard;
	}

	if (ImGui::Button(isWind ? "Disable Wind" : "Enable Wind"))
	{
		isWind = !isWind;
	}

	ImGui::End(); // ウィンドウの終了
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
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
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
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };


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
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//Shaderをコンパイルする
	Microsoft::WRL::ComPtr <IDxcBlob> vertexShaderBlob = ShaderManager::CompileShader(L"Resources/Shaders/Particle.VS.hlsl", L"vs_6_0", dxCommon_->GetIDxcUtils(), dxCommon_->GetIDxcCompiler(), dxCommon_->GetIncludeHandler());
	assert(vertexShaderBlob != nullptr);

	//Pixelをコンパイルする
	Microsoft::WRL::ComPtr <IDxcBlob> pixelShaderBlob = ShaderManager::CompileShader(L"Resources/Shaders/Particle.PS.hlsl", L"ps_6_0", dxCommon_->GetIDxcUtils(), dxCommon_->GetIDxcCompiler(), dxCommon_->GetIncludeHandler());
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
///						パーティクル生成処理
/// -------------------------------------------------------------
Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate, ParticleEffectType type)
{
	Particle particle;

	switch (type)
	{
	case ParticleEffectType::Default: {
		std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
		std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
		std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

		Vector3 randomTranslate{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
		particle.transform.translate_ = translate + randomTranslate;
		particle.transform.scale_ = { 1.0f, 1.0f, 1.0f };
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };
		particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
		particle.lifeTime = distTime(randomEngine);
		particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
		break;
	}

	case ParticleEffectType::Slash: {
		std::uniform_real_distribution<float> distScale(0.4f, 1.5f);
		std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);

		particle.transform.scale_ = { 0.05f, distScale(randomEngine), 1.0f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 0.0f, 0.0f, 0.0f };
		particle.transform.rotate_ = { 0.0f, 0.0f, distRotate(randomEngine) };
		particle.transform.translate_ = translate;
		particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		particle.lifeTime = 1.0f;
		particle.velocity = { 0.0f, 0.0f, 0.0f };
		break;
	}
	case ParticleEffectType::Ring: {
		// ランダム処理なしの固定値
		particle.transform.translate_ = translate; // 位置
		particle.transform.scale_ = { 1.0f, 1.0f, 1.0f }; // 大きさ
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f }; // Z軸回転（45度）

		particle.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 色
		particle.lifeTime = 999.0f; // 半永久的に表示
		particle.velocity = { 0.0f, 0.0f, 0.0f }; // 動かさない

		particle.startScale = particle.transform.scale_;
		particle.endScale = particle.transform.scale_;
		break;
	}
	case ParticleEffectType::Cylinder: {
		std::uniform_real_distribution<float> distColor(0.0f, 1.0f);

		particle.transform.translate_ = translate;
		particle.transform.scale_ = { 1.0f, 1.0f, 1.0f }; // 高さ方向にスケール
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };

		particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
		particle.lifeTime = 999.0f; // 一時的にずっと表示

		particle.startScale = particle.transform.scale_;
		particle.endScale = particle.transform.scale_;
		break;
	}
	}

	particle.currentTime = 0.0f;
	return particle;
}

std::list<Particle> ParticleManager::Emit(const Emitter& emitter, std::mt19937& randomEngine, ParticleEffectType type)
{
	std::list<Particle> particles;
	for (uint32_t count = 0; count < emitter.count; ++count)
	{
		particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate_, type));
	}

	return particles;
}

bool ParticleManager::IsCollision(const AABB& aabb, const Vector3& point)
{
	// 点がAABBの範囲内にあるかチェック
	return (point.x >= aabb.min.x && point.x <= aabb.max.x &&
		point.y >= aabb.min.y && point.y <= aabb.max.y &&
		point.z >= aabb.min.z && point.z <= aabb.max.z);
}
