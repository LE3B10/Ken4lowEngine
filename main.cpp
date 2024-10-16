#include <Windows.h>
#include <cstdint>		/*int32_tを使うためにincludeを追加*/
#include <string>
#include <format>
#include <dxgi1_6.h>
#include <cassert>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <fstream>
#include <sstream>
#include <wrl.h>

#include "externals/DirectXTex/DirectXTex.h"

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

#include "WinApp.h"
#include "Input.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "D3DResourceLeakChecker.h"
#include "LogString.h"

D3DResourceLeakChecker resourceLeakCheck;

// クライアント領域サイズ
static const uint32_t kClientWidth = 1280;
static const uint32_t kClientHeight = 720;

#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")

#define pi 3.141592653589793238462643383279502884197169399375105820974944f

// MaterialDataの構造体
struct MaterialData
{
	std::string textureFilePath;
};

// ModelData構造体
struct ModelData
{
	std::vector<VertexData> vertices;
	MaterialData material;
};

// DescriptorHeapを生成する
Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> CreateDescriptorHeap(Microsoft::WRL::ComPtr <ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible)
{
	//ディスクリプタヒープの生成
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;	//レンダーターゲットビュー用
	descriptorHeapDesc.NumDescriptors = numDescriptors;						//ダブルバッファ用に2つ。多くても別に構わない
	descriptorHeapDesc.Flags = shadervisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	//ディスクリプタヒープが作れなかったので起動できない
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

// Resource作成の関数化
Microsoft::WRL::ComPtr <ID3D12Resource> CreateBufferResource(Microsoft::WRL::ComPtr <ID3D12Device> device, size_t sizeInBytes)
{
	//頂点リソース用のヒープ設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;	//UploadHeapを使う
	//頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	//バッファリソース。テクスチャの場合はまた別の設定をする
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;				//リソースのサイズ
	//バッファの場合はこれらは1にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	//バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//実際に頂点リソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));
	return vertexResource.Get();
}

// CompilerShader関数
IDxcBlob* CompilerShader(
	//CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	//Compilerに使用するProfile
	const wchar_t* profile,
	//初期化で生成したものを3つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	//これからシェーダーをコンパイルする旨をログに出す
	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	//hlslファイルを読み込む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	//読めなかったら止める
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;	//UTF8の文字コードであることを通知

	/// 2.Compileする
	LPCWSTR arguments[] =
	{
		filePath.c_str(),			//コンパイル対象のhlslファイル名
		L"-E",L"main",				//エントリーポイントの指定。基本的にmain以外にはしない
		L"-T",profile,				//ShaderProfileの設定
		L"-Zi",L"-Qembed_debug",	//デバッグ用の情報を詰め込む
		L"-Od",						//最適化を外しておく
		L"-Zpr",					//メモリレイアウトは行優先
	};
	//実際にSahaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,		//読み込んだファイル
		arguments,					//コンパイルオプション
		_countof(arguments),		//コンパイルオプションの数
		includeHandler,				//includeが服待てた諸々
		IID_PPV_ARGS(&shaderResult)	//コンパイル結果
	);
	//コンパイルエラーではなくdxcが起動できないなどの致命的な状況
	assert(SUCCEEDED(hr));

	// 3.警告・エラーが出てないか確認する
	//警告・エラーが出てきたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		Log(shaderError->GetStringPointer());

		//警告・エラーダメゼッタイ
		assert(false);
	}

	// 4.Compile結果を受け取って返す
	//コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	//成功したログを出す
	Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
	assert(SUCCEEDED(hr));
	//もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();
	//実行用のバイナリを返却
	return shaderBlob;
}

// Textureデータを読む
DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	//テクスチャファイルを呼んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミップマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//ミップマップ付きのデータを返す
	return mipImages;
}

// DirectX12のTextureResourceを作る
Microsoft::WRL::ComPtr <ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr <ID3D12Device> device, const DirectX::TexMetadata& metadata)
{
	//1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);									//Textureの幅
	resourceDesc.Height = UINT(metadata.height);								//Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);						//mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);					//奥行 or 配列Textureの配列行数
	resourceDesc.Format = metadata.format;										//TextureのFormat
	resourceDesc.SampleDesc.Count = 1;											//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);		//Textureの次元数。普段使っているのは二次元

	//2. 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;								//細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;		//WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;					//プロセッサの近くに配膳

	//3. Resourceを生成する
	Microsoft::WRL::ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,														//Heapの設定
		D3D12_HEAP_FLAG_NONE,													//Heapの特殊な設定。特になし。
		&resourceDesc,															///Resourceの設定
		D3D12_RESOURCE_STATE_GENERIC_READ,										//初回のResourceState。Textureは基本読むだけ
		nullptr,																//Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));												//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

//データを移送するUploadTextureData関数
void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages)
{
	//Meta情報を取得
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	//全MipMapについて
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel)
	{
		//MipMapLevelを指定して各Imageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		//Textureに転送
		HRESULT hr = texture->WriteToSubresource(
			UINT(mipLevel),
			nullptr,				//全領域へコピー
			img->pixels,			//元データアドレス
			UINT(img->rowPitch),	//1ラインサイズ
			UINT(img->slicePitch)	//1枚サイズ
		);
		assert(SUCCEEDED(hr));
	}
}

// DepthStencilTextureを作る
Microsoft::WRL::ComPtr <ID3D12Resource> CreateDepthStencilTextureResource(Microsoft::WRL::ComPtr <ID3D12Device> device, int32_t width, int32_t height)
{
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;										//Textureの幅
	resourceDesc.Height = height;									//Textureの高さ
	resourceDesc.MipLevels = 1;										//mipmapの数
	resourceDesc.DepthOrArraySize = 1;								//奥行 or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;			//DepthStencilとして知用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;								//サンプリングカウント。１固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	//２次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	//DepthStencilとして使う通知

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapPropaties{};
	heapPropaties.Type = D3D12_HEAP_TYPE_DEFAULT;					//VRAMに作る

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;					//1.0f（最大値）でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;		//フォーマット。Resourceと合わせる

	//Resourceの生成
	Microsoft::WRL::ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapPropaties,							//Heapの設定
		D3D12_HEAP_FLAG_NONE,					//Heapの特殊な設定。特になし。
		&resourceDesc,							//Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,		//深度値を書き込む状態にしておく
		&depthClearValue,						//Clear最適地
		IID_PPV_ARGS(&resource));				//作成するResourceポインタのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

//CPUのDescriptorHandleを取得する関数
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

//GPUのDescriptorHandleを取得する関数
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	//1. 中で必要なる変数の宣言
	MaterialData materialData;
	std::string line;

	//2. ファイルを開く
	std::ifstream file(directoryPath + "/" + filename); //ファイルを開く
	assert(file.is_open());// とりあえず開けなかったら止める

	//3. 実際にファイルを読み、ModelDataを構築していく_頂点情報を読む
	while (std::getline(file, line))
	{
		std::string identifire;
		std::istringstream s(line);
		s >> identifire;

		// identifireに応じた処理
		if (identifire == "map_Kd")
		{
			std::string textureFilename;
			s >> textureFilename;
			//連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	//4. MaterialDataを返す
	return materialData;
}

// Objファイルを読み込む関数
ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	//1. 中で必要なる変数の宣言
	ModelData modelData;				// 構築するModelData
	std::vector<Vector4> positions;		// 位置
	std::vector<Vector3> normals;		// 法線
	std::vector<Vector2> texcoords;		// テクスチャ座標
	std::string line;					// ファイルから読んだ1行を格納するもの

	//2. ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);		// ファイルを開く
	assert(file.is_open());									// 開けなかったら止める

	//3. 実際にファイルを読み、ModelDataを構築していく_頂点情報を読む
	while (std::getline(file, line))
	{
		std::string identifier;
		std::stringstream s(line);
		s >> identifier;			// 先頭の識別子を読む

		// identifierに応じた処理
		if (identifier == "v")
		{
			Vector4 position{};
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		}
		else if (identifier == "vt")
		{
			Vector2 texcoord{};
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);
		}
		else if (identifier == "vn")
		{
			Vector3 normal{};
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		}
		else if (identifier == "f")
		{
			VertexData triangle[3];
			// 三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex)
			{
				std::string vertexDefinition;
				s >> vertexDefinition;
				// 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndieces[3]{};
				for (int32_t element = 0; element < 3; ++element)
				{
					std::string index;
					std::getline(v, index, '/');	// 区切りでインデックスを読んでいく
					elementIndieces[element] = std::stoi(index);
				}
				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[static_cast<std::vector<Vector4, std::allocator<Vector4>>::size_type>(elementIndieces[0]) - 1];
				Vector2 texcoord = texcoords[static_cast<std::vector<Vector2, std::allocator<Vector2>>::size_type>(elementIndieces[1]) - 1];
				Vector3 normal = normals[static_cast<std::vector<Vector3, std::allocator<Vector3>>::size_type>(elementIndieces[2]) - 1];
				position.x *= -1;
				texcoord.y = 1.0f - texcoord.y;
				normal.x *= -1;
				/*VertexData vertex = { position,texcoord,normal };
				modelData.vertices.push_back(vertex);*/
				triangle[faceVertex] = { position,texcoord,normal };
			}
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		}
		else if (identifier == "mtllib")
		{
			// materialTemplateLibraryファイル名を取得
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一改装にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	//4. ModelDataを返す
	return modelData;
}


// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	HRESULT hr{};

	/// ---------- シングルトンインスタンス ---------- ///
	WinApp* winApp = WinApp::GetInstance();
	Input* input = Input::GetInstance();
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	ImGuiManager* imguiManager = ImGuiManager::GetInstance();

	/// ---------- WindowsAPIのウィンドウ作成 ---------- ///
	winApp->CreateMainWindow(kClientWidth, kClientHeight);

	/// ---------- 入力の初期化 ---------- ///
	input->Initialize(winApp);

	/// ---------- DirectXの初期化 ----------///
	dxCommon->Initialize(winApp, kClientWidth, kClientHeight);

	/// ---------- ImGuiManagerの初期化 ---------- ///
	imguiManager->Initialize(winApp, dxCommon);


#pragma region DescriptorSize
	//DescriptorSizeを取得しておく
	const uint32_t descriptorSizeSRV = dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
#pragma endregion


#pragma region PSO(Pipeline State Object)
	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;														//0から始まる
	descriptorRange[0].NumDescriptors = 1;															//数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;									//SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;	//Offsetを自動計算

	//RootParameter作成。複数設定できるので配列。今回は1つだけなので長さ１の配列
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;								//CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								//PixelShaderを使う
	rootParameters[0].Descriptor.ShaderRegister = 0;												//レジスタ番号０とバインド

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;								//CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;							//VertexShaderを使う
	rootParameters[1].Descriptor.ShaderRegister = 0;												//レジスタ番号０を使う

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;					//DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								//PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;							//Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);				//Tableで利用する数

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;								//CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								//PixelShaderを使う
	rootParameters[3].Descriptor.ShaderRegister = 1;												//レジスタ番号1を使う
	descriptionRootSignature.pParameters = rootParameters;											//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);								//配列の長さ
#pragma endregion


#pragma region Samplerの設定
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
#pragma endregion


#pragma region シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr <ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	hr = dxCommon->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
#pragma endregion


#pragma region InputLayoutの設定を行う・InputLayoutの拡張
	//InputLayout
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
#pragma endregion


#pragma region BlendStateの設定を行う
	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
#pragma endregion


#pragma region RasterizerStateの設定を行う
	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
#pragma endregion


#pragma region ShaderをCompileする
	//Shaderをコンパイルする
	Microsoft::WRL::ComPtr <IDxcBlob> vertexShaderBlob = CompilerShader(L"Resources/Shaders/Object3D.VS.hlsl", L"vs_6_0", dxCommon->GetIDxcUtils(), dxCommon->GetIDxcCompiler(), dxCommon->GetIncludeHandler());
	assert(vertexShaderBlob != nullptr);

	//Pixelをコンパイルする
	Microsoft::WRL::ComPtr <IDxcBlob> pixelShaderBlob = CompilerShader(L"Resources/Shaders/Object3D.PS.hlsl", L"ps_6_0", dxCommon->GetIDxcUtils(), dxCommon->GetIDxcCompiler(), dxCommon->GetIncludeHandler());
	assert(pixelShaderBlob != nullptr);
#pragma endregion


#pragma region DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
#pragma endregion


#pragma region グラフィックスパイプラインステートオブジェクト（Pipeline State Object, PSO）を生成する
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};												// パイプラインステートディスクリプタの初期化
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();													// RootSgnature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;													// InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),vertexShaderBlob->GetBufferSize() };	// VertexDhader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() };	// PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;															// BlendState
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
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
#pragma endregion


#pragma region マテリアル用のリソースを作成しそのリソースにデータを書き込む処理を行う
	//マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResource = CreateBufferResource(dxCommon->GetDevice(), sizeof(Material));
	//マテリアルにデータを書き込む
	Material* materialData = nullptr;
	//書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	//今回は赤を書き込んでみる
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = true;
	//UVTramsform行列を単位行列で初期化
	materialData->uvTransform = MakeIdentity();
#pragma endregion


#pragma region スプライト用のマテリアルリソースを作成し設定する処理を行う
	//スプライト用のマテリアルソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResourceSprite = CreateBufferResource(dxCommon->GetDevice(), sizeof(Material));
	Material* materialDataSprite = nullptr;
	//書き込むためのアドレスを取得
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	//SpriteはLightingしないのでfalseを設定する
	materialDataSprite->enableLighting = false;
	////UVTramsform行列を単位行列で初期化(スプライト用)
	materialDataSprite->uvTransform = MakeIdentity();
#pragma endregion


#pragma region 平行光源のプロパティ 色 方向 強度 を格納するバッファリソースを生成しその初期値を設定
	//平行光源用のリソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> directionalLightResource = CreateBufferResource(dxCommon->GetDevice(), sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	//書き込むためのアドレスを取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	directionalLightData->color = { 1.0f,1.0f,1.0f ,1.0f };
	directionalLightData->direction = { 0.0f,-1.0f,0.0f };
	directionalLightData->intensity = 1.0f;
#pragma endregion


#pragma region WVP行列データを格納するバッファリソースを生成し初期値として単位行列を設定
	//WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr <ID3D12Resource> wvpResource = CreateBufferResource(dxCommon->GetDevice(), sizeof(TransfomationMatrix));
	//データを書き込む
	TransfomationMatrix* wvpData = nullptr;
	//書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込んでおく
	wvpData->World = MakeIdentity();
	wvpData->WVP = MakeIdentity();
#pragma endregion


#pragma region スプライトの頂点バッファリソースと変換行列リソースを生成
	//Sprite用の頂点リソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResourceSprite = CreateBufferResource(dxCommon->GetDevice(), sizeof(VertexData) * 6);

	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	// 頂点データを設定する
	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	//1枚目の三角形
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f };		//左下
	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };			//左上
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f };		//右下
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };

	//2枚目の三角形
	vertexDataSprite[3].position = { 0.0f,0.0f,0.0f,1.0f };			//左上
	vertexDataSprite[3].texcoord = { 0.0f,0.0f };
	vertexDataSprite[4].position = { 640.0f,0.0f,0.0f,1.0f };		//右上
	vertexDataSprite[4].texcoord = { 1.0f,0.0f };
	vertexDataSprite[5].position = { 640.0f,360.0f,0.0f,1.0f };		//右下
	vertexDataSprite[5].texcoord = { 1.0f,1.0f };

	// 法線情報を追加する
	for (int i = 0; i < 6; ++i) {
		vertexDataSprite[i].normal = { 0.0f, 0.0f, -1.0f };
	}

	//Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr <ID3D12Resource> transfomationMatrixResourceSprite = CreateBufferResource(dxCommon->GetDevice(), sizeof(TransfomationMatrix));

	//データを書き込む
	TransfomationMatrix* transfomationMatrixDataSprite = nullptr;
	transfomationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transfomationMatrixDataSprite));

	//単位行列を書き込んでおく
	transfomationMatrixDataSprite->World = MakeIdentity();
	transfomationMatrixDataSprite->WVP = MakeIdentity();
#pragma endregion


#pragma region スプライトのインデックスバッファを作成および設定する
	Microsoft::WRL::ComPtr <ID3D12Resource> indexResourceSprite = CreateBufferResource(dxCommon->GetDevice(), sizeof(uint32_t) * 6);
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス６つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; indexDataSprite[1] = 1; indexDataSprite[2] = 2;
	indexDataSprite[3] = 1; indexDataSprite[4] = 4; indexDataSprite[5] = 2;
#pragma endregion


#pragma region テクスチャファイルを読み込みテクスチャリソースを作成しそれに対してSRVを設定してこれらをデスクリプタヒープにバインド
	// モデルの読み込み
	ModelData modelData = LoadObjFile("Resources", "axis.obj");

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr <ID3D12Resource> textureResource = CreateTextureResource(dxCommon->GetDevice(), metadata);
	UploadTextureData(textureResource.Get(), mipImages);

	//2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr <ID3D12Resource> textureResource2 = CreateTextureResource(dxCommon->GetDevice(), metadata2);
	UploadTextureData(textureResource2.Get(), mipImages2);

	// 1つ目のテクスチャのSRV設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;				//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// 2つ目のテクスチャのSRV設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;				//2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// 1つ目のテクスチャのSRVのデスクリプタヒープへのバインド
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), descriptorSizeSRV, 1);
	textureSrvHandleCPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	// 2つ目のテクスチャのSRVのデスクリプタヒープへのバインド
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(dxCommon->GetSRVDescriptorHeap(), descriptorSizeSRV, 2);
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
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource = CreateBufferResource(dxCommon->GetDevice(), sizeof(VertexData) * (modelData.vertices.size() + TotalVertexCount));
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
	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex)
	{
		float lat = -pi / 2.0f + kLatEvery * latIndex; // θ
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex)
		{
			float u = float(lonIndex) / float(kSubdivision);
			float v = 1.0f - float(latIndex) / float(kSubdivision);

			uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;
			float lon = lonIndex * kLonEvery; // Φ

			sphereVertexData[start + 0].position = { cos(lat) * cos(lon), sin(lat), cos(lat) * sin(lon), 1.0f };
			sphereVertexData[start + 0].texcoord = { u, v };
			sphereVertexData[start + 0].normal = { sphereVertexData[start + 0].position.x, sphereVertexData[start + 0].position.y, sphereVertexData[start + 0].position.z };

			sphereVertexData[start + 1].position = { cos(lat + kLatEvery) * cos(lon), sin(lat + kLatEvery), cos(lat + kLatEvery) * sin(lon), 1.0f };
			sphereVertexData[start + 1].texcoord = { u, v - 1.0f / float(kSubdivision) };
			sphereVertexData[start + 1].normal = { sphereVertexData[start + 1].position.x, sphereVertexData[start + 1].position.y, sphereVertexData[start + 1].position.z };

			sphereVertexData[start + 2].position = { cos(lat) * cos(lon + kLonEvery), sin(lat), cos(lat) * sin(lon + kLonEvery), 1.0f };
			sphereVertexData[start + 2].texcoord = { u + 1.0f / float(kSubdivision), v };
			sphereVertexData[start + 2].normal = { sphereVertexData[start + 2].position.x, sphereVertexData[start + 2].position.y, sphereVertexData[start + 2].position.z };

			sphereVertexData[start + 3].position = { cos(lat + kLatEvery) * cos(lon + kLonEvery), sin(lat + kLatEvery), cos(lat + kLatEvery) * sin(lon + kLonEvery), 1.0f };
			sphereVertexData[start + 3].texcoord = { u + 1.0f / float(kSubdivision), v - 1.0f / float(kSubdivision) };
			sphereVertexData[start + 3].normal = { sphereVertexData[start + 3].position.x, sphereVertexData[start + 3].position.y, sphereVertexData[start + 3].position.z };

			sphereVertexData[start + 4].position = { cos(lat) * cos(lon + kLonEvery), sin(lat), cos(lat) * sin(lon + kLonEvery), 1.0f };
			sphereVertexData[start + 4].texcoord = { u + 1.0f / float(kSubdivision), v };
			sphereVertexData[start + 4].normal = { sphereVertexData[start + 4].position.x, sphereVertexData[start + 4].position.y, sphereVertexData[start + 4].position.z };

			sphereVertexData[start + 5].position = { cos(lat + kLatEvery) * cos(lon), sin(lat + kLatEvery), cos(lat + kLatEvery) * sin(lon), 1.0f };
			sphereVertexData[start + 5].texcoord = { u, v - 1.0f / float(kSubdivision) };
			sphereVertexData[start + 5].normal = { sphereVertexData[start + 5].position.x, sphereVertexData[start + 5].position.y, sphereVertexData[start + 5].position.z };
		}
	}

	// アンマップ
	vertexResource->Unmap(0, nullptr);
#pragma endregion

	//Tramsform変数を作る
	Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	Transform cameraTransform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,-10.0f} };
	Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	//UVTransform用の変数を用意
	Transform uvTransformSprite{ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f}, };

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

		// ImGuiフレーム開始
		imguiManager->BeginFrame();

		//開発用のUIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();

		// Gui
		{
			ImGui::Begin("Test Window");
			ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.01f);
			ImGui::SliderAngle("CameraRotateX", &cameraTransform.rotate.x);
			ImGui::SliderAngle("CameraRotateY", &cameraTransform.rotate.y);
			ImGui::SliderAngle("CameraRotateZ", &cameraTransform.rotate.z);

			ImGui::DragFloat3("transformSprite", &transformSprite.translate.x, 1.0f);
			ImGui::DragFloat3("scale", &transform.scale.x, 0.01f);
			ImGui::DragFloat3("rotate", &transform.rotate.x, 0.01f);
			ImGui::DragFloat3("translate", &transform.translate.x, 0.01f);
			ImGui::Checkbox("useMonsterBall", &useMonsterBall);
			ImGui::DragFloat3("directionalLight", &directionalLightData->direction.x, 0.01f);
			ImGui::DragFloat2("UVTranslete", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
			ImGui::End();
		}

		// ImGuiフレーム終了
		imguiManager->EndFrame();


		///-----ゲームの処理-----///

		//transform.rotate.y += 0.03f;

		/*-----Transform情報を作る-----*/
		Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		Matrix4x4 camraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		Matrix4x4 viewMatrix = Inverse(camraMatrix);
		Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);
		Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

		wvpData->WVP = worldViewProjectionMatrix;
		wvpData->World = worldMatrix;

		//Sprite用のWorldViewProjectionMatrixを作る
		Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
		Matrix4x4 viewMatrixSprite = MakeIdentity();
		Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
		Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

		transfomationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
		transfomationMatrixDataSprite->World = worldMatrix;

		Matrix4x4 uvTransformMatrix = MakeAffineMatrix(uvTransformSprite.scale, uvTransformSprite.rotate, uvTransformSprite.translate);
		materialDataSprite->uvTransform = uvTransformMatrix;


		// 描画開始処理
		dxCommon->BeginDraw();

		// ディスクリプタヒープの設定
		ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon->GetSRVDescriptorHeap() };
		dxCommon->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);


		/*-----シーン（モデル）の描画設定と描画-----*/
		//ルートシグネチャとパイプラインステートの設定
		dxCommon->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
		dxCommon->GetCommandList()->SetPipelineState(graphicsPipelineState.Get());

		// 頂点バッファの設定とプリミティブトポロジの設定
		dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // モデル用VBV

		// 定数バッファビュー (CBV) とディスクリプタテーブルの設定
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
		dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

		// モデルの描画
		dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

		/*-----スプライトの描画設定と描画-----*/
		dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSprite); // スプライト用VBV
		dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferViewSprite); // IBVの設定
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transfomationMatrixResourceSprite->GetGPUVirtualAddress());
		dxCommon->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

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