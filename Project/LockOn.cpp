#include "LockOn.h"
#include "WinApp.h"
#include "Player.h"
#include "Camera.h"
#include "Enemy.h"
#include "Input.h"
#include "Object3DCommon.h"
#include "TextureManager.h"
#include "FollowCamera.h"


void LockOn::Initialize()
{
	TextureManager::GetInstance()->LoadTexture("Resources/Target.png");
	input_ = Input::GetInstance();

	lockOnMark_ = std::make_unique<Sprite>();
	lockOnMark_->Initialize("Resources/Target.png");
	float screenWidth = WinApp::kClientWidth;
	float screenHeight = WinApp::kClientHeight;
	lockOnMark_->SetPosition(Vector2(screenWidth / 2.0f - 32, screenHeight / 2.0f - 32));
}

void LockOn::Update(const std::list<std::unique_ptr<Enemy>>& enemies)
{
	// ロックオンボタンが押されたら検索
	if (input_->TriggerKey(DIK_T) || input_->TriggerButton(XButtons.R_Shoulder))
	{
		Serching(enemies);
		isLockedOn_ = !isLockedOn_;
	}

	// ロックオン対象が削除されていないかチェック
	bool targetExists = false;
	for (const auto& enemy : enemies)
	{
		if (enemy.get() == target_)
		{
			targetExists = true;
			break;
		}
	}

	// 敵がいなくなったらロックオン解除
	if (!targetExists)
	{
		target_ = nullptr;
		isLockedOn_ = false;
	}

	// ロックオン継続
	if (target_)
	{
		// ロックオンマークの座標計算
		Vector3 positionWorld = target_->GetCenterPosition(); // 敵のロックオン座標を取得
		Vector3 positionScreen = WorldToScreen(positionWorld); // ワールド座標からスクリーン座標に変換
		Vector2 positionScreenV2(positionScreen.x, positionScreen.y); // Vector2に格納
		lockOnMark_->SetPosition({ positionScreenV2.x - 32, positionScreenV2.y - 32 }); // スプライトの座標を設定
	}

	// スプライト更新
	lockOnMark_->Update();
}

void LockOn::Draw()
{
	if (isLockedOn_)
	{
		lockOnMark_->Draw();
	}
}

void LockOn::Serching(const std::list<std::unique_ptr<Enemy>>& enemies)
{
	// ロックオン対象の候補を格納（距離, 敵のポインタ）
	std::list<std::pair<float, const Enemy*>> targets;

	// カメラのビュー行列を取得
	Matrix4x4 viewMatrix = followCamera_->GetViewMatrix();
	Vector3 cameraPosition = followCamera_->GetPosition();

	// 全ての敵に対して順にロックオン判定
	for (const std::unique_ptr<Enemy>& enemy : enemies)
	{
		// 敵のロックオン座標を取得
		Vector3 positionWorld = enemy->GetCenterPosition();

		// ワールド座標をビュー座標に変換 (射影行列は適用しない)
		Vector3 positionView = Vector3::Transform(positionWorld, viewMatrix);

		// カメラとのユークリッド距離を計算
		float distance = Vector3::Length(positionWorld - cameraPosition);

		// 距離条件チェック
		if (minDistance_ <= distance && distance <= maxDistance_)
		{
			// カメラ前方との角度を計算
			float arcTangent = std::atan2(
				std::sqrt(positionView.x * positionView.x + positionView.y * positionView.y),
				positionView.z);

			// 角度条件チェック（コーンに収まっているか）
			if (fabs(arcTangent) <= angleRange_)
			{
				targets.emplace_back(std::make_pair(distance, enemy.get()));
			}
		}
	}

	// ロックオン対象をリセット
	target_ = nullptr;
	if (!targets.empty())
	{
		// 距離でソート（近い順）
		targets.sort([](auto& pair1, auto& pair2) { return pair1.first < pair2.first; });

		// 最も近い敵をロックオン対象とする
		target_ = targets.front().second;
	}
}

bool LockOn::OutOfRangeJudgement(const std::list<std::unique_ptr<Enemy>>& enemies)
{
	// ターゲットがいなければロック解除
	if (!target_)
	{
		isLockedOn_ = false;
		return false;
	}


	// 範囲内ならロックオン維持
	return true;
}

Vector3 LockOn::WorldToScreen(const Vector3& positionWorld)
{
	// ビュー・プロジェクション行列を取得
	Matrix4x4 viewProj = followCamera_->GetViewProjectionMatrix();

	// 変換用のVector4を作成 (w = 1.0f)
	Vector4 positionWorld4(positionWorld.x, positionWorld.y, positionWorld.z, 1.0f);

	// ワールド座標を NDC に変換
	Vector4 positionNDC = Vector4::Transform(positionWorld4, viewProj);

	// 透視投影変換後の w で割る (射影除算)
	if (positionNDC.w != 0.0f) {
		positionNDC.x /= positionNDC.w;
		positionNDC.y /= positionNDC.w;
		positionNDC.z /= positionNDC.w;
	}

	// スクリーンサイズ取得
	float screenWidth = static_cast<float>(WinApp::kClientWidth);
	float screenHeight = static_cast<float>(WinApp::kClientHeight);

	// スクリーン座標に変換
	Vector3 positionScreen;
	positionScreen.x = (positionNDC.x * 0.5f + 0.5f) * screenWidth;
	positionScreen.y = (1.0f - (positionNDC.y * 0.5f + 0.5f)) * screenHeight;
	positionScreen.z = positionNDC.z; // 深度値 (使わない場合は省略可)

	return positionScreen;
}
