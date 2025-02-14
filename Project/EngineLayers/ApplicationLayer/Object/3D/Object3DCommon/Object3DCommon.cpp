#include "Object3DCommon.h"
#include "DirectXCommon.h"
#include "DebugCamera.h"

/// -------------------------------------------------------------
///				　	シングルトンインスタンス
/// -------------------------------------------------------------
Object3DCommon* Object3DCommon::GetInstance()
{
	static Object3DCommon instance;
    return &instance;
}


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void Object3DCommon::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	isDebugCamera_ = false;	

	// ライトマネージャの生成と初期化
	lightManager_ = std::make_unique<LightManager>();
	lightManager_->Initialize(dxCommon_);
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void Object3DCommon::Update()
{
	if (isDebugCamera_)
	{
#ifdef _DEBUG
		debugViewProjectionMatrix_ = DebugCamera::GetInstance()->GetViewProjectionMatrix();
		defaultCamera_->SetViewProjectionMatrix(debugViewProjectionMatrix_);
#endif // _DEBUG
	}
	else
	{
		viewProjectionMatrix_ = Matrix4x4::Multiply(defaultCamera_->GetViewMatrix(), defaultCamera_->GetProjectionMatrix());
		defaultCamera_->SetViewProjectionMatrix(viewProjectionMatrix_);
	}
}


/// -------------------------------------------------------------
///				　		共通描画処理設定
/// -------------------------------------------------------------
void Object3DCommon::SetRenderSetting()
{
	lightManager_->PreDraw();
}
