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

	// スプライトの初期化
	transformSprite = { { size_.x, size_.y, 1.0f }, { 0.0f, 0.0f, rotation_ }, { position_.x, position_.y, 0.0f } };
	uvTransformSprite = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	// スプライト用のマテリアルリソースを作成し設定する処理を行う
	CreateMaterialResource(dxCommon);

	// スプライトの頂点バッファリソースと変換行列リソースを生成
	CreateVertexBufferResource(dxCommon);

	// スプライトのインデックスバッファを作成および設定する
	CreateIndexBuffer(dxCommon);
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
	float bottom = 0.0f - anchorPoint_.y;

	// 左右反転
	//if (transform2D_.isFlipX)
	//{
	//	left = -left;
	//	right = -right;
	//}
	//// 上下反転
	//if (transform2D_.isFlipX)
	//{
	//	top = -top;
	//	bottom = -bottom;
	//}


	// メタデータ取得
	//const DirectX::TexMetadata& metaData = TextureManager::GetInstance()->GetMetaData(textureName);

	// 横
	/*float texture_Left = transform2D_.LeftTop.x / metaData.width;
	float texture_Right = (transform2D_.LeftTop.x + transform2D_.textureSize.x) / metaData.width;*/

	// 縦
	/*float texture_Top = transform2D_.LeftTop.y / metaData.height;
	float texture_Top = (transform2D_.LeftTop.y + transform2D_.textureSize.y) / metaData.height;*/


	//1枚目の三角形

	// 左下
	vertexDataSprite[0].position = { 0.0f, 1.0f, 0.0f, 1.0f };
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	// 左上
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	// 右下
	vertexDataSprite[2].position = { 1.0f, 1.0f, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };

	//2枚目の三角形

	// 左上
	vertexDataSprite[3].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 0.0f, 0.0f };
	// 右上
	vertexDataSprite[4].position = { 1.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[4].texcoord = { 1.0f, 0.0f };
	// 右下
	vertexDataSprite[5].position = { 1.0f, 1.0f, 0.0f, 1.0f };
	vertexDataSprite[5].texcoord = { 1.0f, 1.0f };

	// 法線情報を追加する
	for (int i = 0; i < 6; ++i) {
		vertexDataSprite[i].normal = { 0.0f, 0.0f, -1.0f };
	}

	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 4;
	indexDataSprite[5] = 2;

	transformSprite.scale = { size_.x, size_.y, 1.0f };
	transformSprite.rotate = { 0.0f, 0.0f, rotation_ };
	transformSprite.translate = { position_.x, position_.y, 0.0f };

	//Sprite用のWorldViewProjectionMatrixを作る
	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
	Matrix4x4 viewMatrixSprite = MakeIdentity();
	Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

	transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
	transformationMatrixDataSprite->World = viewMatrixSprite;

	Matrix4x4 uvTransformMatrix = MakeAffineMatrix(uvTransformSprite.scale, uvTransformSprite.rotate, uvTransformSprite.translate);
	materialDataSprite->uvTransform = uvTransformMatrix;

	// 変更した頂点データを頂点バッファに再度アップロード
	memcpy(vertexDataSprite, &vertexDataSprite[0], sizeof(VertexData) * 6);
}


/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void Sprite::DrawImGui()
{
	if (ImGui::TreeNode("Sprite"))
	{
		ImGui::DragFloat3("scale Sprite", &transformSprite.scale.x, 0.1f);
		ImGui::DragFloat3("rotate Sprite", &transformSprite.rotate.x, 0.01f);
		ImGui::DragFloat3("transform Sprite", &transformSprite.translate.x, 1.0f);
		ImGui::DragFloat2("UVTranslete", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
		ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
		ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
		ImGui::ColorEdit4("Color", &materialDataSprite->color.x);
		ImGui::Text("Sprite Size: %f, %f", size_.x, size_.y);
		ImGui::TreePop();
	}
}


/// -------------------------------------------------------------
///						　スプライト描画
/// -------------------------------------------------------------
void Sprite::DrawCall(ID3D12GraphicsCommandList* commandList)
{
	commandList->DrawIndexedInstanced(kVertexNum, 1, 0, 0, 0);
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
	vertexResourceSprite = createBuffer_->CreateBufferResource(dxCommon->GetDevice(), sizeof(VertexData) * 6);

	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * kVertexNum;
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
	indexResourceSprite = createBuffer_->CreateBufferResource(dxCommon->GetDevice(), sizeof(uint32_t) * kVertexNum);
	//リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス６つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * kVertexNum;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
}
