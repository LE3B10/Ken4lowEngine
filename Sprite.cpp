#include "Sprite.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "MatrixMath.h"


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Sprite::Initialize(const std::string& filePath)
{
	//spriteData_ = CreateSpriteData(kVertexNum, kIndexNum);
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(filePath);

	// スプライトのインデックスバッファを作成および設定する
	CreateIndexBuffer(dxCommon);

	// スプライト用のマテリアルリソースを作成し設定する処理を行う
	CreateMaterialResource(dxCommon);

	// スプライトの頂点バッファリソースと変換行列リソースを生成
	CreateVertexBufferResource(dxCommon);

	// テクスチャサイズに合わせる
	AdjustTextureSize();
}


/// -------------------------------------------------------------
///							　更新処理
/// -------------------------------------------------------------
void Sprite::Update()
{
	// アンカーポイント
	float left = 0.0f - anchorPoint_.x;
	float right = 1.0f - anchorPoint_.x;
	float top = 0.0f - anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

	// 左右反転
	if (isFlipX_)
	{
		left = -left;
		right = -right;
	}
	
	// 上下反転
	if (isFlipY_)
	{
		top = -top;
		bottom = -bottom;
	}

	// メタデータ取得
	const DirectX::TexMetadata& metaData = TextureManager::GetInstance()->GetMetaData(textureIndex);

	// テクスチャ範囲指定
	float tex_left = textureLeftTop_.x / metaData.width;
	float tex_right = (textureLeftTop_.x + textureSize_.x) / metaData.width;
	float tex_top = textureLeftTop_.y / metaData.height;
	float tex_bottom = (textureLeftTop_.y + textureSize_.y) / metaData.height;

	/// ---------- 頂点データ設定 ---------- ///

	// 左上
	vertexDataSprite[0].position = { left, bottom, 0.0f, 1.0f };
	vertexDataSprite[0].texcoord = { tex_left, tex_bottom };

	// 左下
	vertexDataSprite[1].position = { left, top, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { tex_left, tex_top };

	// 右下
	vertexDataSprite[2].position = { right, bottom, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { tex_right, tex_bottom };

	// 右上
	vertexDataSprite[3].position = { right, top, 0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { tex_right, tex_top };

	// 法線情報を追加する
	for (int i = 0; i < 6; ++i) {
		vertexDataSprite[i].normal = { 0.0f, 0.0f, -1.0f };
	}

	// ワールド行列の計算
	Transform transform{ { size_.x, size_.y, 1.0f }, { 0.0f, 0.0f, rotation_ }, { position_.x, position_.y, 0.0f } };

	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	Matrix4x4 viewMatrixSprite = MakeIdentity();
	Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

	transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
	transformationMatrixDataSprite->World = worldMatrixSprite;

	// 頂点データ更新
	//memcpy(vertexDataSprite, &vertexDataSprite[0], sizeof(VertexData) * kNumVertex);
}


/// -------------------------------------------------------------
///						　スプライト描画
/// -------------------------------------------------------------
void Sprite::DrawCall(ID3D12GraphicsCommandList* commandList)
{
	commandList->DrawIndexedInstanced(kNumVertex, 1, 0, 0, 0);
}


/// -------------------------------------------------------------
///			VBV - IBV - CBVの設定処理（スプライト用）
/// -------------------------------------------------------------
void Sprite::SetSpriteBufferData(ID3D12GraphicsCommandList* commandList)
{
	commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite); // スプライト用VBV
	commandList->IASetIndexBuffer(&indexBufferViewSprite); // IBVの設定
	commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite.Get()->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite.Get()->GetGPUVirtualAddress());

	// ディスクリプタテーブルの設定
	commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex));
}


/// -------------------------------------------------------------
///	 スプライト用のマテリアルリソースを作成し設定する処理を行う
/// -------------------------------------------------------------
void Sprite::CreateMaterialResource(DirectXCommon* dxCommon)
{
	//スプライト用のマテリアルソースを作る
	materialResourceSprite = createBuffer_->CreateBufferResource(dxCommon->GetDevice(), sizeof(Material));

	//書き込むためのアドレスを取得
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	//SpriteはLightingしないのでfalseを設定する
	materialDataSprite->enableLighting = false;
	////UVTramsform行列を単位行列で初期化(スプライト用)
	materialDataSprite->uvTransform = MakeIdentity();
}


/// -------------------------------------------------------------
///	  スプライトの頂点バッファリソースと変換行列リソースを生成
/// -------------------------------------------------------------
void Sprite::CreateVertexBufferResource(DirectXCommon* dxCommon)
{
	//Sprite用の頂点リソースを作る
	vertexResourceSprite = createBuffer_->CreateBufferResource(dxCommon->GetDevice(), sizeof(VertexData) * kNumVertex);

	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * kNumVertex;
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	//Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	transformationMatrixResourceSprite = createBuffer_->CreateBufferResource(dxCommon->GetDevice(), sizeof(TransformationMatrix));
	// 座標変換行列リソースにデータを書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));

	//単位行列を書き込んでおく
	transformationMatrixDataSprite->World = MakeIdentity();
	transformationMatrixDataSprite->WVP = MakeIdentity();
}


/// -------------------------------------------------------------
///	   スプライトのインデックスバッファを作成および設定する
/// -------------------------------------------------------------
void Sprite::CreateIndexBuffer(DirectXCommon* dxCommon)
{
	indexResourceSprite = createBuffer_->CreateBufferResource(dxCommon->GetDevice(), sizeof(uint32_t) * kNumVertex);
	//リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス６つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * kNumVertex;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));

	// インデックスデータにデータを書き
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;
}


/// -------------------------------------------------------------
///			　テクスチャサイズをイメージに合わせる
/// -------------------------------------------------------------
void Sprite::AdjustTextureSize()
{
	// テクスチャメタデータを取得
	const DirectX::TexMetadata& metaData = TextureManager::GetInstance()->GetMetaData(textureIndex);

	textureSize_.x = static_cast<float>(metaData.width);
	textureSize_.y = static_cast<float>(metaData.height);

	// 画像サイズをテクスチャサイズに合わせる
	size_ = textureSize_;
}
