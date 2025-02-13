#include "DebugCamera.h"
#include "WinApp.h"
#include "Input.h"
#include "ParameterManager.h"

/// -------------------------------------------------------------
///					シングルトンインスタンス
/// -------------------------------------------------------------
DebugCamera* DebugCamera::GetInstance()
{
	static DebugCamera instance;
	return &instance;
}


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void DebugCamera::Initialize()
{
	worldTransform_.Initialize();
	worldTransform_.translate_ = { 0.0f,0.0f,-100.0f };
    
    ParameterManager* manager = ParameterManager::GetInstance();
    manager->AddItem("DebugCamera", "Position", worldTransform_.translate_);

	fovY_ = 0.45f;
	aspectRatio_ = float(WinApp::kClientWidth) / float(WinApp::kClientHeight);
	nearClip_ = 0.1f;
	farClip_ = 100.0f;
	worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);
	viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);
	projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);

	rotateMatrix_ = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);
}


/// -------------------------------------------------------------
///							　更新処理
/// -------------------------------------------------------------
void DebugCamera::Update()
{
    ParameterManager* manager = ParameterManager::GetInstance();
    worldTransform_.translate_ = manager->GetValue<Vector3>("DebugCamera", "Position");
    
	Move();

    // 更新された角度をもとに回転行列を再生成
    rotateMatrix_ = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);

    Matrix4x4 transformMatrix = Matrix4x4::MakeTranslateMatrix(worldTransform_.translate_);

    // ワールド行列を作る
    worldMatrix_ = rotateMatrix_ * transformMatrix;

    // ビュー行列を作る
    viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);

    // **プロジェクション行列の計算は変更しない**
    projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);

    // **ビュー射影行列を更新**
    viewProjectionMatrix_ = viewMatrix_ * projectionMatrix_;

    worldTransform_.Update();
}


/// -------------------------------------------------------------
///							移動操作処理
/// -------------------------------------------------------------
void DebugCamera::Move()
{
    Vector3 move = { 0.0f, 0.0f, 0.0f };

    // カメラの前後左右移動
    if (Input::GetInstance()->PushKey(DIK_W))
    {
        move.z += 0.4f;
    }
    if (Input::GetInstance()->PushKey(DIK_S))
    {
        move.z -= 0.4f;
    }
    if (Input::GetInstance()->PushKey(DIK_A))
    {
        move.x -= 0.4f;
    }
    if (Input::GetInstance()->PushKey(DIK_D))
    {
        move.x += 0.4f;
    }

    // カメラの高さ移動
    if (Input::GetInstance()->PushKey(DIK_SPACE))
    {
        move.y += 0.4f;
    }
    if (Input::GetInstance()->PushKey(DIK_LSHIFT))
    {
        move.y -= 0.4f;
    }

    // カメラの向きに基づいた移動計算
    move = rotateMatrix_ * move;
    worldTransform_.translate_ += move;

    // カメラの角度変更
    if (Input::GetInstance()->PushKey(DIK_UP))
    {
        worldTransform_.rotate_.x += 0.04f;
    }
    if (Input::GetInstance()->PushKey(DIK_DOWN))
    {
        worldTransform_.rotate_.x -= 0.04f;
    }
    if (Input::GetInstance()->PushKey(DIK_LEFT))
    {
        worldTransform_.rotate_.y -= 0.04f;
    }
    if (Input::GetInstance()->PushKey(DIK_RIGHT))
    {
        worldTransform_.rotate_.y += 0.04f;
    }
}
