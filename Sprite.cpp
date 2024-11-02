#include "Sprite.h"

#include "DirectXCommon.h"
#include "TextureManager.h"
#include "MatrixMath.h"
#include "Transform.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Sprite::Initialize()
{
	//spriteData_ = CreateSpriteData(kVertexNum, kIndexNum);
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// スプライト用のマテリアルリソースを作成し設定する処理を行う
	CreateMaterialResource(dxCommon);
	// スプライトの頂点バッファリソースと変換行列リソースを生成
	CreateVertexBufferResource(dxCommon);
	// スプライトのインデックスバッファを作成および設定する
	CreateIndexBuffer(dxCommon);
}

void Sprite::Update()
{
	//Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	////Sprite用のWorldViewProjectionMatrixを作る
	//Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
	//Matrix4x4 viewMatrixSprite = MakeIdentity();
	//Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
	//Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

	//transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
	//transformationMatrixDataSprite->World = viewMatrixSprite;
}


/// -------------------------------------------------------------
///			VBV - IBV - CBVの設定処理（スプライト用）
/// -------------------------------------------------------------
void Sprite::SetSpriteBufferData(ID3D12GraphicsCommandList* commandList)
{
	//commandList->IASetVertexBuffers(0, 1, &spriteData_->vertexBufferViewSprite); // スプライト用VBV
	//commandList->IASetIndexBuffer(&spriteData_->indexBufferViewSprite); // IBVの設定
	//commandList->SetGraphicsRootConstantBufferView(0, spriteData_->materialResourceSprite.Get()->GetGPUVirtualAddress());
	//commandList->SetGraphicsRootConstantBufferView(1, spriteData_->transformationMatrixResourceSprite.Get()->GetGPUVirtualAddress());
}


/// -------------------------------------------------------------
///						　スプライト描画
/// -------------------------------------------------------------
void Sprite::DrawCall(ID3D12GraphicsCommandList* commandList)
{
	commandList->DrawIndexedInstanced(kVertexNum, 1, 0, 0, 0);
}


/// -------------------------------------------------------------
///	 スプライト用のマテリアルリソースを作成し設定する処理を行う
/// -------------------------------------------------------------
void Sprite::CreateMaterialResource(DirectXCommon* dxCommon)
{
	//スプライト用のマテリアルソースを作る
	materialResourceSprite = createBuffer_.CreateBufferResource(dxCommon->GetDevice(), sizeof(Material));

	//書き込むためのアドレスを取得
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	//SpriteはLightingしないのでfalseを設定する
	materialDataSprite->enableLighting = false;
	////UVTramsform行列を単位行列で初期化(スプライト用)
	materialDataSprite->uvTransform = MakeIdentity();
}


/// -------------------------------------------------------------
///	 スプライトの頂点バッファリソースと変換行列リソースを生成
/// -------------------------------------------------------------
void Sprite::CreateVertexBufferResource(DirectXCommon* dxCommon)
{
	//Sprite用の頂点リソースを作る
	vertexResourceSprite = createBuffer_.CreateBufferResource(dxCommon->GetDevice(), sizeof(VertexData) * 6);

	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

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
	transformationMatrixResourceSprite = createBuffer_.CreateBufferResource(dxCommon->GetDevice(), sizeof(TransformationMatrix));
	// 座標変換行列リソースにデータを書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));

	//単位行列を書き込んでおく
	transformationMatrixDataSprite->World = MakeIdentity();
	transformationMatrixDataSprite->WVP = MakeIdentity();
}


/// -------------------------------------------------------------
///	  スプライトのインデックスバッファを作成および設定する
/// -------------------------------------------------------------
void Sprite::CreateIndexBuffer(DirectXCommon* dxCommon)
{
	indexResourceSprite = createBuffer_.CreateBufferResource(dxCommon->GetDevice(), sizeof(uint32_t) * 6);
	//リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス６つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; indexDataSprite[1] = 1; indexDataSprite[2] = 2;
	indexDataSprite[3] = 1; indexDataSprite[4] = 4; indexDataSprite[5] = 2;
}
