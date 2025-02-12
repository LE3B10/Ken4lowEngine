#define NOMINMAX
#include "LockOn.h"
#include "Input.h"
#include <TextureManager.h>
#include "Enemy.h"
#include "VectorMath.h"

#include "WinApp.h"

/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void LockOn::Initialize(Camera* camera)
{
	input_ = Input::GetInstance();
	camera_ = camera;

	// レティクル用テクスチャ取得
	TextureManager::GetInstance()->LoadTexture("Resources/Target.png");

	lockOnMark_ = std::make_unique<Sprite>();
	lockOnMark_->Initialize("Resources/Target.png");
}


/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void LockOn::Update(const std::list<std::unique_ptr<Enemy>>& enemies, Camera* camera)
{
	bool isRPressed = input_->TriggerKey(DIK_R);

	if (isRPressed && !wasRKyePresses_)
	{
		if (!isLockedOn_) {
			// 最も近いエネミーをターゲットに
			float minDist = std::numeric_limits<float>::max();
			const Enemy* closestEnemy = nullptr;

			for (const auto& enemy : enemies)
			{
				float dist = Distance(camera->GetTranslate(), enemy->GetCenterPosition());
				if (dist < minDist)
				{
					minDist = dist;
					closestEnemy = enemy.get();
				}
			}

			if (closestEnemy)
			{
				target_ = closestEnemy;
				isLockedOn_ = true;
			}
		}
		else
		{
			// ロック解除
			isLockedOn_ = false;
			target_ = nullptr;
		}
	}

	wasRKyePresses_ = isRPressed;
}


/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
void LockOn::Draw()
{
	if (ExistTarget()) {
		Vector3 worldPos = GetTargetPosition();
		Vector2 screenPos = ConvertToScreenPosition(worldPos);
		lockOnMark_->SetPosition(screenPos);
		lockOnMark_->Draw();
	}
}

Vector3 LockOn::GetTargetPosition() const
{
	if (target_)
	{
		return target_->GetCenterPosition();
	}
	return { 0.0f, 0.0f, 0.0f };
}

Vector2 LockOn::ConvertToScreenPosition(const Vector3& worldPos)
{
	Matrix4x4 viewProjMatrix = camera_->GetViewProjectionMatrix(); // カメラのビュー射影行列
	Vector3 clipSpacePos = Transform(worldPos, viewProjMatrix);

	// NDC (-1 ~ +1) からスクリーン座標に変換
	float screenX = (clipSpacePos.x / clipSpacePos.z) * (WinApp::kClientWidth / 2.0f) + (WinApp::kClientWidth / 2.0f);
	float screenY = (clipSpacePos.y / clipSpacePos.z) * -(WinApp::kClientHeight / 2.0f) + (WinApp::kClientHeight / 2.0f);

	return { screenX, screenY };
}
