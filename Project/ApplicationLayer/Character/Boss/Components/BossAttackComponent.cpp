#include "BossAttackComponent.h"
#include "IBossAttack.h"
#include "BossBase.h"
#include "BossAnimationComponent.h"

#include <cstring>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

/// -------------------------------------------------------------
///							初期化処理
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
///							終了処理
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
///							攻撃登録
/// -------------------------------------------------------------
void BossAttackComponent::RegisterAttack(std::unique_ptr<IBossAttack> attack)
{
	// null チェック
	if (!attack) return;

	// すでに owner が確定している場合は、その場で初期化してよい
	if (owner_)
	{
		attack->Initialize(owner_);
	}

	// 攻撃を登録
	attacks_.push_back(std::move(attack));
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void BossAttackComponent::Update(float deltaTime)
{
	// 実行中でない攻撃のクールダウンだけ進める
	for (auto& attack : attacks_)
	{
		// null チェック
		if (!attack) continue;

		// 今アクティブな攻撃には TickCooldown しない
		// End() 側でクールダウン開始させる想定
		if (attack.get() != currentAttack_)
		{
			attack->TickCooldown(deltaTime);
		}
	}

	// 実行中の攻撃が無ければここで終了
	if (!currentAttack_) return;

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
///						名前で攻撃開始
/// -------------------------------------------------------------
bool BossAttackComponent::StartAttackByName(const std::string& attackName)
{
	IBossAttack* attack = FindAttackByName(attackName.c_str());
	return StartAttackInternal(attack);
}

/// -------------------------------------------------------------
///					インデックスで攻撃開始
/// -------------------------------------------------------------
bool BossAttackComponent::StartAttackByIndex(size_t index)
{
	IBossAttack* attack = GetAttack(index);
	return StartAttackInternal(attack);
}

/// -------------------------------------------------------------
///					現在攻撃中のものを強制終了
/// -------------------------------------------------------------
void BossAttackComponent::ForceEndCurrentAttack()
{
	// 実行中の攻撃が無ければここで終了
	if (!currentAttack_) return;

	currentAttack_->End();
	currentAttack_ = nullptr;
}

/// -------------------------------------------------------------
///					インデックスで攻撃取得
/// -------------------------------------------------------------
IBossAttack* BossAttackComponent::GetAttack(size_t index) const
{
	// 範囲外チェック
	if (index >= attacks_.size()) return nullptr;

	// 範囲内で攻撃が登録されていればそれを返す
	return attacks_[index].get();
}

/// -------------------------------------------------------------
///						名前で攻撃取得
/// -------------------------------------------------------------
IBossAttack* BossAttackComponent::FindAttackByName(const char* attackName) const
{
	for (const auto& attack : attacks_)
	{
		if (!attack) continue;

		if (std::strcmp(attack->GetName(), attackName) == 0)
		{
			return attack.get();
		}
	}

	return nullptr;
}

/// -------------------------------------------------------------
///					開始可能攻撃一覧を収集
/// -------------------------------------------------------------
std::vector<IBossAttack*> BossAttackComponent::CollectStartableAttacks() const
{
	std::vector<IBossAttack*> result;

	// すでに攻撃中なら新規攻撃は始めない前提
	if (currentAttack_) return result;

	for (const auto& attack : attacks_)
	{
		if (!attack) continue;

		if (attack->CanStart())
		{
			result.push_back(attack.get());
		}
	}

	return result;
}

/// -------------------------------------------------------------
///						攻撃描画
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
///					攻撃シャドウ描画
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
///						デバッグ描画
/// -------------------------------------------------------------
void BossAttackComponent::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Boss Attack Component", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	ImGui::Text("IsAttacking : %s", currentAttack_ ? "true" : "false");
	ImGui::Text("AttackCount : %d", static_cast<int>(attacks_.size()));

	if (currentAttack_)
	{
		ImGui::Text("CurrentAttack : %s", currentAttack_->GetName());
	}
	else
	{
		ImGui::Text("CurrentAttack : None");
	}

	ImGui::Separator();
	ImGui::Text("Registered Attacks");

	for (size_t i = 0; i < attacks_.size(); ++i)
	{
		IBossAttack* attack = attacks_[i].get();
		if (!attack)
		{
			continue;
		}

		ImGui::PushID(static_cast<int>(i));

		ImGui::Text("[%d] %s", static_cast<int>(i), attack->GetName());
		ImGui::Text("  Active    : %s", attack->IsActive() ? "true" : "false");
		ImGui::Text("  Cooldown  : %.2f", attack->GetCooldownRemaining());
		ImGui::Text("  Range     : %.2f - %.2f", attack->GetMinRange(), attack->GetMaxRange());
		ImGui::Text("  Priority  : %d", attack->GetPriority());
		ImGui::Text("  CanStart  : %s", attack->CanStart() ? "true" : "false");

		const bool canStartThis = (!currentAttack_ && attack->CanStart());
		if (!canStartThis)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Start This Attack"))
		{
			StartAttackByIndex(i);
		}

		if (!canStartThis)
		{
			ImGui::EndDisabled();
		}

		attack->DrawImGui();

		ImGui::Separator();
		ImGui::PopID();
	}
#endif
}

/// -------------------------------------------------------------
///						攻撃開始共通処理
/// -------------------------------------------------------------
bool BossAttackComponent::StartAttackInternal(IBossAttack* attack)
{
	// null は開始不可
	if (!attack) return false;

	// すでに別の攻撃中なら開始しない
	if (currentAttack_)	return false;

	// 攻撃条件を満たしていないなら開始しない
	if (!attack->CanStart()) return false;

	attack->Start();
	currentAttack_ = attack;

	// ---------------------------------------------------------
	// 攻撃開始時にアニメ時間をリセット
	// これをしないと前回攻撃の経過時間が残って、
	// Windup が飛んだように見えることがある
	// ---------------------------------------------------------
	if (owner_ && owner_->GetAnimationComponent())
	{
		owner_->GetAnimationComponent()->ResetAttackTimer();
	}

	return true;
}