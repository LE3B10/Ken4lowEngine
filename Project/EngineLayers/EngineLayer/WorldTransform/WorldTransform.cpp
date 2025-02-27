#include "WorldTransform.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include "Camera.h"
#include <Object3DCommon.h>

void WorldTransform::Initialize()
{
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

#pragma region WVP行列データを格納するバッファリソースを生成し初期値として単位行列を設定
	//WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	wvpResource = ResourceManager::CreateBufferResource(DirectXCommon::GetInstance()->GetDevice(), sizeof(TransformationMatrix));

	//書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込んでおく
	wvpData->World = Matrix4x4::MakeIdentity();
	wvpData->WVP = Matrix4x4::MakeIdentity();
	wvpData->WorldInversedTranspose = Matrix4x4::MakeIdentity();
#pragma endregion
}

void WorldTransform::Update()
{
    // ローカル変換行列を作成
    Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(scale_, rotate_, translate_);

    // 親オブジェクトがあれば親のワールド行列を掛ける
    if (parent_)
    {
        worldMatrix = Matrix4x4::Multiply(worldMatrix, parent_->matWorld_);
    }

    // 親がいる場合は親の回転を引き継ぐ
    if (parent_)
    {
        worldRotate_ = parent_->worldRotate_ + rotate_;
    }
    else
    {
        worldRotate_ = rotate_;
    }

    // ワールド座標を取得
    worldTranslate_ = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };

    // ビュー・プロジェクション変換を適用
    Matrix4x4 worldViewProjectionMatrix;
    if (camera_)
    {
        const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
        worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);
    }
    else
    {
        worldViewProjectionMatrix = worldMatrix;
    }

    // ワールド行列を保存
    matWorld_ = worldMatrix;
    wvpData->WVP = worldViewProjectionMatrix;
    wvpData->World = worldMatrix;
    wvpData->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
}

void WorldTransform::SetPipeline()
{
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
}
