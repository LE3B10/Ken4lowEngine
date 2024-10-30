#include "Sprite.h"

#include "DirectXCommon.h"
#include "TextureManager.h"
#include "MatrixMath.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Sprite::Initialize()
{
	spriteData_ = CreateSpriteData(kVertexNum, kIndexNum);
}


/// -------------------------------------------------------------
///			VBV - IBV - CBVの設定処理（スプライト用）
/// -------------------------------------------------------------
void Sprite::SetSpriteBufferData(ID3D12GraphicsCommandList* commandList)
{
	commandList->IASetVertexBuffers(0, 1, &spriteData_->vertexBufferViewSprite); // スプライト用VBV
	commandList->IASetIndexBuffer(&spriteData_->indexBufferViewSprite); // IBVの設定
	commandList->SetGraphicsRootConstantBufferView(0, spriteData_->materialResourceSprite.Get()->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, spriteData_->transformationMatrixResourceSprite.Get()->GetGPUVirtualAddress());
}


/// -------------------------------------------------------------
///						　スプライト描画
/// -------------------------------------------------------------
void Sprite::DrawCall(ID3D12GraphicsCommandList* commandList)
{
	commandList->DrawIndexedInstanced(kVertexNum, 1, 0, 0, 0);
}


/// -------------------------------------------------------------
///					スプライトのメッシュの生成
/// -------------------------------------------------------------
std::unique_ptr<Sprite::SpriteData> Sprite::CreateSpriteData(UINT vertexCount, UINT indexCount)
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	std::unique_ptr<SpriteData> spriteData = std::make_unique<SpriteData>();

	HRESULT hr{};

	// 頂点バッファの生成
	UINT vertexBufferSize = static_cast<UINT>(sizeof(VertexData) * vertexCount);

	// 頂点バッファビューの生成
	spriteData->vertexResourceSprite = createBuffer_.CreateBufferResource(dxCommon->GetDevice(), vertexBufferSize);

	//頂点バッファビューを作成する
	spriteData->vertexBufferViewSprite.BufferLocation = spriteData->vertexResourceSprite->GetGPUVirtualAddress();
	spriteData->vertexBufferViewSprite.SizeInBytes = vertexBufferSize;
	spriteData->vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	// 頂点データを設定する
	hr = spriteData->vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&spriteData->vertexDataSprite));
	assert(SUCCEEDED(hr));

	// インデックスデータのサイズ
	UINT indexDataSize = static_cast<UINT>(sizeof(uint32_t) * indexCount);

	//リソースの先頭のアドレスから使う
	spriteData->indexBufferViewSprite.BufferLocation = spriteData->indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス６つ分のサイズ
	spriteData->indexBufferViewSprite.SizeInBytes = indexDataSize;
	//インデックスはuint32_tとする
	spriteData->indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
	// インデックスデータを設定する
	hr = spriteData->indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&spriteData->indexDataSprite));
	assert(SUCCEEDED(hr));

	// マテリアルバッファの生成
	spriteData->materialResourceSprite = createBuffer_.CreateBufferResource(dxCommon->GetDevice(), sizeof(Material));

	//書き込むためのアドレスを取得
	hr = spriteData->materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&spriteData->materialDataSprite));

	// マテリアルの初期化
	spriteData->materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// UVTramsform行列を単位行列で初期化(スプライト用)
	spriteData->materialDataSprite->uvTransform = MakeIdentity();
	assert(SUCCEEDED(hr));

	// 座標変換行列の生成
	spriteData->materialResourceSprite = createBuffer_.CreateBufferResource(dxCommon->GetDevice(), sizeof(TransformationMatrix));

	// 座標変換データのマッピング
	hr = spriteData->transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&spriteData->transformationMatrixDataSprite));

	//単位行列を書き込んでおく
	spriteData->transformationMatrixDataSprite->World = MakeIdentity();
	spriteData->transformationMatrixDataSprite->WVP = MakeIdentity();
	assert(SUCCEEDED(hr));

	return spriteData;
}
