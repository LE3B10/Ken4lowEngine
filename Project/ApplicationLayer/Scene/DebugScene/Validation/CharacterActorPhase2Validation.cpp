#include "CharacterActorPhase2Validation.h"

#include <ActorJsonSerializer.h>
#include <ActorWorld.h>
#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/CharacterTargetComponent.h>

#include <memory>
#include <sstream>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void CharacterActorPhase2Validation::Initialize(K4E::ActorWorld& actorWorld)
{
	actorWorld_ = &actorWorld;
	if (K4E::CharacterActor* existingCharacter = FindTarget())
	{
		ConfigureTarget(*existingCharacter);
		return; // Scene再初期化時も同名の検証Actorを重複生成しない。
	}
	K4E::CharacterActor& character = actorWorld_->SpawnActor<K4E::CharacterActor>();
	ConfigureTarget(character);
}

void CharacterActorPhase2Validation::ProcessRequests()
{
	if (!actorWorld_) return;
	K4E::CharacterActor* target = FindTarget();

	if (requestSpawn_)
	{
		requestSpawn_ = false;
		if (!target)
		{
			listenerActor_ = nullptr;
			listenerId_ = 0;
			K4E::CharacterActor& actor = actorWorld_->SpawnActor<K4E::CharacterActor>();
			ConfigureTarget(actor);
			target = &actor;
			lastSucceeded_ = true;
			lastMessage_ = "CharacterActorをActorWorld経由で生成しました。";
		}
	}

	if (requestDamage_)
	{
		requestDamage_ = false;
		const K4E::CharacterDamageResult result = target ? target->ApplyDamage(25.0f) : K4E::CharacterDamageResult{};
		lastAppliedDamage_ = result.appliedDamage;
		lastSucceeded_ = result.accepted;
		lastMessage_ = result.accepted ? "CharacterHealthComponentへ25ダメージを委譲しました。" : "ダメージを適用できませんでした。";
	}

	if (requestLethalDamage_)
	{
		requestLethalDamage_ = false;
		const float lethalDamage = target && target->GetHealthComponent()
			? target->GetHealthComponent()->GetCurrentHealth() + 1.0f
			: 0.0f;
		const K4E::CharacterDamageResult result = target ? target->ApplyDamage(lethalDamage) : K4E::CharacterDamageResult{};
		lastAppliedDamage_ = result.appliedDamage;
		lastSucceeded_ = result.killed;
		lastMessage_ = result.killed ? "死亡遷移と死亡イベントを通知しました。" : "致死ダメージを適用できませんでした。";
	}

	if (requestRestore_)
	{
		requestRestore_ = false;
		K4E::CharacterHealthComponent* health = target ? target->GetHealthComponent() : nullptr;
		if (health)
		{
			health->RestoreFullHealth();
			lastSucceeded_ = target->IsAlive();
			lastMessage_ = "HP Component側で全回復しました。";
		}
	}

	if (requestMove_)
	{
		requestMove_ = false;
		K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
		if (movement)
		{
			movement->SetMovementEnabled(true);
			movement->SetVelocity({ 1.0f, 0.0f, 0.0f });
			lastSucceeded_ = true;
			lastMessage_ = "Movement Componentへ移動速度を設定しました。";
		}
	}

	if (requestStop_)
	{
		requestStop_ = false;
		K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
		if (movement)
		{
			movement->Stop();
			lastSucceeded_ = true;
			lastMessage_ = "Movement Componentの移動を停止しました。";
		}
	}

	if (requestSave_)
	{
		requestSave_ = false;
		lastSucceeded_ = target && actorWorld_->SaveActorToJson(*target, jsonPath_);
		lastMessage_ = lastSucceeded_ ? "CharacterActor JSONを保存しました。" : "CharacterActor JSONの保存に失敗しました。";
	}

	if (requestReload_)
	{
		requestReload_ = false;
		lastSucceeded_ = target && actorWorld_->ReloadActorFromJson(*target, jsonPath_);
		lastMessage_ = lastSucceeded_ ? "Component構成とHPをJSONから復元しました。" : "CharacterActor JSONの再読込に失敗しました。";
	}
}

void CharacterActorPhase2Validation::DrawImGui()
{
#ifdef USE_IMGUI
	if (requestValidation_)
	{
		requestValidation_ = false;
		RunValidation();
	}

	if (!ImGui::Begin("CharacterActor Phase 2 検証"))
	{
		ImGui::End();
		return;
	}

	K4E::CharacterActor* target = FindTarget();
	const K4E::CharacterHealthComponent* health = target ? target->GetHealthComponent() : nullptr;
	const K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
	const K4E::Vector3 targetPosition = target ? target->GetTargetPosition() : K4E::Vector3{};
	const K4E::Vector3 velocity = movement ? movement->GetVelocity() : K4E::Vector3{};
	ImGui::Text("対象Actor: %s", target ? target->GetName().c_str() : "なし");
	ImGui::Text("生存状態: %s", target && target->IsAlive() ? "生存" : "死亡");
	ImGui::Text("HP: %.1f / %.1f", health ? health->GetCurrentHealth() : 0.0f, health ? health->GetMaxHealth() : 0.0f);
	ImGui::Text("ターゲット位置: %.2f, %.2f, %.2f", targetPosition.x, targetPosition.y, targetPosition.z);
	ImGui::Text("移動速度: %.2f, %.2f, %.2f", velocity.x, velocity.y, velocity.z);
	ImGui::Text("死亡イベント回数: %d", deathEventCount_);
	ImGui::Text("直前の適用ダメージ: %.1f", lastAppliedDamage_);
	ImGui::Text("直前の死亡ダメージ: %.1f", lastDeathDamage_);
	ImGui::TextColored(lastSucceeded_ ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
		"%s", lastMessage_.c_str());

	if (ImGui::Button("自動検証##Phase2")) requestValidation_ = true;
	ImGui::SameLine();
	if (ImGui::Button("Character生成##Phase2")) requestSpawn_ = true;
	ImGui::SameLine();
	if (ImGui::Button("25ダメージ##Phase2")) requestDamage_ = true;

	if (ImGui::Button("致死ダメージ##Phase2")) requestLethalDamage_ = true;
	ImGui::SameLine();
	if (ImGui::Button("全回復##Phase2")) requestRestore_ = true;
	ImGui::SameLine();
	if (ImGui::Button("移動開始##Phase2")) requestMove_ = true;
	ImGui::SameLine();
	if (ImGui::Button("移動停止##Phase2")) requestStop_ = true;

	if (ImGui::Button("JSON保存##Phase2")) requestSave_ = true;
	ImGui::SameLine();
	if (ImGui::Button("JSON再読込##Phase2")) requestReload_ = true;
	ImGui::TextDisabled("CharacterActorは窓口だけを持ち、HP計算と移動は各Componentが担当します。");
	ImGui::End();
#endif
}

void CharacterActorPhase2Validation::Finalize()
{
	actorWorld_ = nullptr;
	listenerActor_ = nullptr;
	listenerId_ = 0;
}

K4E::CharacterActor* CharacterActorPhase2Validation::FindTarget() const
{
	if (!actorWorld_) return nullptr;
	for (const auto& actor : actorWorld_->GetActors())
	{
		if (actor && actor->GetName() == targetActorName_) return dynamic_cast<K4E::CharacterActor*>(actor.get());
	}
	return nullptr;
}

void CharacterActorPhase2Validation::ConfigureTarget(K4E::CharacterActor& actor)
{
	actor.SetName(targetActorName_);
	actor.SetLayer("DebugValidation");
	actor.AddTag("CharacterActorPhase2");
	if (listenerActor_ != &actor || !actor.HasDeathListener(listenerId_))
	{
		listenerId_ = actor.AddDeathListener([this](const K4E::CharacterDeathEvent& deathEvent)
			{
				++deathEventCount_;
				lastDeathDamage_ = deathEvent.result.appliedDamage;
			});
		listenerActor_ = &actor;
	}
	if (K4E::SceneComponent* root = actor.GetRootComponent())
	{
		root->SetLocalPosition({ 4.0f, 0.0f, 0.0f });
		root->RefreshWorldTransform();
	}
}

void CharacterActorPhase2Validation::RunValidation()
{
	K4E::CharacterActor* target = FindTarget();
	K4E::CharacterHealthComponent* health = target ? target->GetHealthComponent() : nullptr;
	K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
	K4E::CharacterTargetComponent* targetComponent = target ? target->GetTargetComponent() : nullptr;
	if (!target || !health || !movement || !targetComponent)
	{
		lastSucceeded_ = false;
		lastMessage_ = "CharacterActorまたは必須Componentが見つかりません。";
		return;
	}

	ConfigureTarget(*target);
	const float originalMaxHealth = health->GetMaxHealth();
	const float originalCurrentHealth = health->GetCurrentHealth();
	const bool originalInvulnerable = health->IsInvulnerable();
	const K4E::Vector3 originalVelocity = movement->GetVelocity();
	const bool originalMovementEnabled = movement->IsMovementEnabled();
	const int deathEventsBefore = deathEventCount_;
	int removedListenerCallCount = 0;
	const K4E::CharacterActor::DeathListenerId removedListenerId = target->AddDeathListener(
		[&removedListenerCallCount](const K4E::CharacterDeathEvent&) { ++removedListenerCallCount; });
	const bool listenerRemoved = target->RemoveDeathListener(removedListenerId);

	health->ResetHealth(100.0f);
	health->SetInvulnerable(false);
	const K4E::CharacterDamageResult nonLethalResult = target->ApplyDamage(25.0f);
	const K4E::CharacterDamageResult lethalResult = target->ApplyDamage(100.0f);
	const K4E::CharacterDamageResult duplicateResult = target->ApplyDamage(1.0f);
	lastAppliedDamage_ = lethalResult.appliedDamage;

	const nlohmann::json snapshot = K4E::ActorJsonSerializer::SerializeActor(*target);
	bool hasHealthJson = false;
	bool hasMovementJson = false;
	bool hasTargetJson = false;
	if (snapshot.contains("Components") && snapshot["Components"].is_array())
	{
		for (const auto& componentJson : snapshot["Components"])
		{
			const std::string className = componentJson.value("Class", std::string{});
			hasHealthJson = hasHealthJson || className == "CharacterHealthComponent";
			hasMovementJson = hasMovementJson || className == "CharacterMovementComponent";
			hasTargetJson = hasTargetJson || className == "CharacterTargetComponent";
		}
	}

	std::unique_ptr<K4E::Actor> restoredActor = K4E::ActorJsonSerializer::CreateActorFromJson(snapshot);
	K4E::CharacterActor* restoredCharacter = dynamic_cast<K4E::CharacterActor*>(restoredActor.get());
	const bool factoryRestoreSucceeded = restoredCharacter && restoredCharacter->GetHealthComponent() &&
		restoredCharacter->GetMovementComponent() && restoredCharacter->GetTargetComponent();
	if (restoredActor) restoredActor->FinalizeForWorld();

	const K4E::Vector3 actorTargetPosition = target->GetTargetPosition();
	const K4E::Vector3 componentTargetPosition = targetComponent->GetTargetPosition();
	const bool targetPositionMatches = actorTargetPosition.x == componentTargetPosition.x &&
		actorTargetPosition.y == componentTargetPosition.y && actorTargetPosition.z == componentTargetPosition.z;
	const K4E::Vector3 explicitTargetPosition{ 8.0f, 2.0f, -3.0f };
	targetComponent->SetTargetPosition(explicitTargetPosition);
	const K4E::Vector3 delegatedTargetPosition = target->GetTargetPosition();
	const bool explicitTargetMatches = targetComponent->HasExplicitTargetPosition() &&
		delegatedTargetPosition.x == explicitTargetPosition.x && delegatedTargetPosition.y == explicitTargetPosition.y &&
		delegatedTargetPosition.z == explicitTargetPosition.z;
	targetComponent->ClearTargetPosition();
	const bool componentAccessMatches = target->GetCharacterComponent<K4E::CharacterHealthComponent>() == health;
	movement->SetMovementEnabled(true);
	movement->SetVelocity({ 2.0f, 0.0f, 0.0f });
	const bool movementDelegationMatches = target->GetMovementComponent() == movement &&
		movement->IsMovementEnabled() && movement->GetVelocity().x == 2.0f;

	const bool succeeded = listenerRemoved && removedListenerCallCount == 0 && componentAccessMatches && movementDelegationMatches &&
		targetPositionMatches && explicitTargetMatches && nonLethalResult.accepted && nonLethalResult.healthBefore == 100.0f &&
		nonLethalResult.healthAfter == 75.0f && !nonLethalResult.killed && lethalResult.accepted && lethalResult.killed &&
		lethalResult.healthAfter == 0.0f && !duplicateResult.accepted && deathEventCount_ == deathEventsBefore + 1 &&
		snapshot.value("Class", std::string{}) == "CharacterActor" && hasHealthJson && hasMovementJson && hasTargetJson && factoryRestoreSucceeded;

	health->SetMaxHealth(originalMaxHealth);
	health->SetCurrentHealth(originalCurrentHealth);
	health->SetInvulnerable(originalInvulnerable);
	movement->SetVelocity(originalVelocity);
	movement->SetMovementEnabled(originalMovementEnabled);

	std::ostringstream message;
	message << (succeeded ? "Phase 2自動検証に成功しました。" : "Phase 2自動検証で不整合を検出しました。")
		<< " HP=" << health->GetCurrentHealth() << "/" << health->GetMaxHealth()
		<< " DeathEvent=" << deathEventCount_;
	lastSucceeded_ = succeeded;
	lastMessage_ = message.str();
}
