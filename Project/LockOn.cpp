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

	// ロックオン継続
	if (target_)
	{
		//// ロックオンマークの座標計算
		//Vector3 positionWorld = target_->GetCenterPosition();
		//Vector3 positionScreen = WorldToScreen(positionWorld);
		//Vector2 positionScreenV2(positionScreen.x, positionScreen.y);
		//lockOnMark_->SetPosition(positionScreenV2);
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

	// 全ての敵に対して順にロックオン判定
	for (const std::unique_ptr<Enemy>& enemy : enemies)
	{
		// 敵のロックオン座標を取得
		Vector3 positionWorld = enemy->GetCenterPosition();

		// ワールド -> ビュー座標変換
		Vector3 positionView = Vector3::Transform(positionWorld, followCamera_->GetViewProjectionMatrix());

		// 距離条件チェック
		if (minDistance_ <= positionView.z && positionView.z <= maxDistance_)
		{
			// カメラ前方との角度を計算
			float arcTangent = std::atan2(
				std::sqrt(positionView.x * positionView.x + positionView.y * positionView.y),
				positionView.z);

			// 角度条件チェック（コーンに治まっているか）
			if (fabs(arcTangent) <= angleRange_)
			{
				targets.emplace_back(std::make_pair(positionView.z, enemy.get()));
			}
		}
	}

	// ロックオン対象をリセット
	target_ = nullptr;
	if (!targets.empty())
	{
		// 距離で照準にソート
		targets.sort([](auto& pair1, auto& pair2) { return pair1.first < pair2.first; });

		// ソートの結果一番近い敵をロックオン対象とする
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
