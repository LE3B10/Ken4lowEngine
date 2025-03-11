#include "LockOn.h"
#include "Player.h"
#include "Camera.h"
#include "Enemy.h"
#include "Input.h"
#include "Object3DCommon.h"
#include "TextureManager.h"


void LockOn::Initialize()
{
	TextureManager::GetInstance()->LoadTexture("Resources/Target.png");
	input_ = Input::GetInstance();

	lockOnMark_ = std::make_unique<Sprite>();
	lockOnMark_->Initialize("Resources/Target.png");
}

void LockOn::Update(const std::list<std::unique_ptr<Enemy>>& enemies)
{
	// ロックオン解除チェック
	if (isLockedOn_)
	{
		if (!OutOfRangeJudgement(enemies))
		{
			target_ = nullptr; // ターゲット解除
			isLockedOn_ = false;
		}
	}

	// ロックオンボタンが押されたら検索
	if (input_->TriggerKey(DIK_T))
	{
		Serching(enemies);
	}

	// スプライト更新
	lockOnMark_->Update();
}

void LockOn::Draw()
{
	lockOnMark_->Draw();
}

void LockOn::Serching(const std::list<std::unique_ptr<Enemy>>& enemies)
{
	// ロックオン対象の候補を格納（距離, 敵のポインタ）
	std::list<std::pair<float, const Enemy*>> targets;

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
