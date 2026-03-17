#include "BossAttackComponent.h"
#include "Attacks/IBossAttack.h"

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void BossAttackComponent::Initialize(BossBase* owner)
{
	owner_ = owner;
	currentAttack_ = nullptr;

	// 登録済み攻撃を初期化
	for (auto& attack : attacks_)
	{
		if (attack)
		{
			attack->Initialize(owner_);
		}
	}
}

/// -------------------------------------------------------------
/// 終了処理
/// -------------------------------------------------------------
void BossAttackComponent::Finalize()
{
	// 実行中攻撃があれば安全のため終了しておく
	if (currentAttack_)
	{
		currentAttack_->End();
		currentAttack_ = nullptr;
	}

	attacks_.clear();
	owner_ = nullptr;
}

/// -------------------------------------------------------------
/// 攻撃登録
/// -------------------------------------------------------------
void BossAttackComponent::RegisterAttack(std::unique_ptr<IBossAttack> attack)
{
	if (!attack)
	{
		return;
	}

	// すでに owner が確定している場合は、その場で初期化してよい
	if (owner_)
	{
		attack->Initialize(owner_);
	}

	attacks_.push_back(std::move(attack));
}

/// -------------------------------------------------------------
/// 更新
/// -------------------------------------------------------------
void BossAttackComponent::Update(float deltaTime)
{
	// 実行中でない攻撃のクールダウンだけ進める
	for (auto& attack : attacks_)
	{
		if (!attack)
		{
			continue;
		}

		// 今アクティブな攻撃には TickCooldown しない
		// End() 側でクールダウン開始させる想定
		if (attack.get() != currentAttack_)
		{
			attack->TickCooldown(deltaTime);
		}
	}

	// 実行中の攻撃が無ければここで終了
	if (!currentAttack_)
	{
		return;
	}

	// 実行中攻撃を更新
	currentAttack_->Update(deltaTime);

	// 終了判定
	if (currentAttack_->IsFinished())
	{
		currentAttack_->End();
		currentAttack_ = nullptr;
	}
}

/// -------------------------------------------------------------
/// 名前で攻撃開始
/// -------------------------------------------------------------
bool BossAttackComponent::StartAttackByName(const std::string& attackName)
{
	IBossAttack* attack = FindAttackByName(attackName);
	return StartAttackInternal(attack);
}

/// -------------------------------------------------------------
/// インデックスで攻撃開始
/// -------------------------------------------------------------
bool BossAttackComponent::StartAttackByIndex(size_t index)
{
	IBossAttack* attack = GetAttack(index);
	return StartAttackInternal(attack);
}

/// -------------------------------------------------------------
/// 現在攻撃中のものを強制終了
/// -------------------------------------------------------------
void BossAttackComponent::ForceEndCurrentAttack()
{
	if (!currentAttack_)
	{
		return;
	}

	currentAttack_->End();
	currentAttack_ = nullptr;
}

/// -------------------------------------------------------------
/// インデックスで攻撃取得
/// -------------------------------------------------------------
IBossAttack* BossAttackComponent::GetAttack(size_t index) const
{
	if (index >= attacks_.size())
	{
		return nullptr;
	}

	return attacks_[index].get();
}

/// -------------------------------------------------------------
/// 名前で攻撃取得
/// -------------------------------------------------------------
IBossAttack* BossAttackComponent::FindAttackByName(const std::string& attackName) const
{
	for (const auto& attack : attacks_)
	{
		if (!attack)
		{
			continue;
		}

		if (attackName == attack->GetName())
		{
			return attack.get();
		}
	}

	return nullptr;
}

/// -------------------------------------------------------------
/// 開始可能攻撃一覧を収集
/// -------------------------------------------------------------
std::vector<IBossAttack*> BossAttackComponent::CollectStartableAttacks() const
{
	std::vector<IBossAttack*> result;

	// すでに攻撃中なら新規攻撃は始めない前提
	if (currentAttack_)
	{
		return result;
	}

	for (const auto& attack : attacks_)
	{
		if (!attack)
		{
			continue;
		}

		if (attack->CanStart())
		{
			result.push_back(attack.get());
		}
	}

	return result;
}

/// -------------------------------------------------------------
/// 攻撃描画
/// -------------------------------------------------------------
void BossAttackComponent::Draw()
{
	for (auto& attack : attacks_)
	{
		if (attack)
		{
			attack->Draw();
		}
	}
}

/// -------------------------------------------------------------
/// 攻撃シャドウ描画
/// -------------------------------------------------------------
void BossAttackComponent::DrawShadow()
{
	for (auto& attack : attacks_)
	{
		if (attack)
		{
			attack->DrawShadow();
		}
	}
}

/// -------------------------------------------------------------
/// デバッグ描画
/// -------------------------------------------------------------
void BossAttackComponent::DrawImGui()
{
	for (auto& attack : attacks_)
	{
		if (attack)
		{
			attack->DrawImGui();
		}
	}
}

/// -------------------------------------------------------------
/// 攻撃開始共通処理
/// -------------------------------------------------------------
bool BossAttackComponent::StartAttackInternal(IBossAttack* attack)
{
	// null は開始不可
	if (!attack)
	{
		return false;
	}

	// すでに別の攻撃中なら開始しない
	if (currentAttack_)
	{
		return false;
	}

	// 攻撃条件を満たしていないなら開始しない
	if (!attack->CanStart())
	{
		return false;
	}

	attack->Start();
	currentAttack_ = attack;
	return true;
}