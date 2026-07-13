#include "CharacterValidation.h"

#include <ActorJsonSerializer.h>
#include <ActorWorld.h>
#include <SceneComponent.h>
#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/CharacterTargetComponent.h>

#include <memory>
#include <cmath>
#include <sstream>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void CharacterValidation::Initialize(K4E::ActorWorld& actorWorld)
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

void CharacterValidation::ProcessRequests()
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
			lastMessage_ = "検証用キャラクターを生成しました。";
		}
	}

	if (requestDamage_)
	{
		requestDamage_ = false;
		const K4E::CharacterDamageResult result = target ? target->ApplyDamage(25.0f) : K4E::CharacterDamageResult{};
		lastAppliedDamage_ = result.appliedDamage;
		lastSucceeded_ = result.accepted;
		lastMessage_ = result.accepted ? "HPへ25ダメージを適用しました。" : "ダメージを適用できませんでした。";
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
			lastMessage_ = "HPを全回復しました。";
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
			lastMessage_ = "移動速度を設定しました。";
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
			lastMessage_ = "移動を停止しました。";
		}
	}

	if (requestSave_)
	{
		requestSave_ = false;
		lastSucceeded_ = target && actorWorld_->SaveActorToJson(*target, jsonPath_);
		lastMessage_ = lastSucceeded_ ? "キャラクター設定をJSONへ保存しました。" : "キャラクター設定の保存に失敗しました。";
	}

	if (requestReload_)
	{
		requestReload_ = false;
		lastSucceeded_ = target && actorWorld_->ReloadActorFromJson(*target, jsonPath_);
		lastMessage_ = lastSucceeded_ ? "共通機能と設定値をJSONから復元しました。" : "キャラクター設定の再読込に失敗しました。";
	}
}

void CharacterValidation::DrawImGui()
{
#ifdef USE_IMGUI
	if (requestValidation_)
	{
		requestValidation_ = false;
		RunValidation();
	}

	if (!ImGui::Begin("キャラクター共通機能 検証"))
	{
		ImGui::End();
		return;
	}

	K4E::CharacterActor* target = FindTarget();
	const K4E::CharacterHealthComponent* health = target ? target->GetHealthComponent() : nullptr;
	const K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
	const K4E::CharacterColliderComponent* collider = target ? target->GetColliderComponent() : nullptr;
	const K4E::CharacterAnimationComponent* animation = target ? target->GetAnimationComponent() : nullptr;
	const K4E::Vector3 targetPosition = target ? target->GetTargetPosition() : K4E::Vector3{};
	const K4E::Vector3 velocity = movement ? movement->GetVelocity() : K4E::Vector3{};
	ImGui::Text("対象Actor: %s", target ? target->GetName().c_str() : "なし");
	ImGui::Text("生存状態: %s", target && target->IsAlive() ? "生存" : "死亡");
	ImGui::Text("HP: %.1f / %.1f", health ? health->GetCurrentHealth() : 0.0f, health ? health->GetMaxHealth() : 0.0f);
	ImGui::Text("ターゲット位置: %.2f, %.2f, %.2f", targetPosition.x, targetPosition.y, targetPosition.z);
	ImGui::Text("移動速度: %.2f, %.2f, %.2f", velocity.x, velocity.y, velocity.z);
	const K4E::Vector3 colliderPosition = collider ? collider->GetWorldPosition() : K4E::Vector3{};
	ImGui::Text("当たり判定位置: %.2f, %.2f, %.2f", colliderPosition.x, colliderPosition.y, colliderPosition.z);
	ImGui::Text("アニメーション: %s / %.2f秒", animation ? animation->GetAnimationName().c_str() : "なし",
		animation ? animation->GetPlaybackTime() : 0.0f);
	ImGui::Text("死亡イベント回数: %d", deathEventCount_);
	ImGui::Text("直前の適用ダメージ: %.1f", lastAppliedDamage_);
	ImGui::Text("直前の死亡ダメージ: %.1f", lastDeathDamage_);
	ImGui::TextColored(lastSucceeded_ ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
		"%s", lastMessage_.c_str());

	if (ImGui::Button("自動検証##CharacterValidation")) requestValidation_ = true;
	ImGui::SameLine();
	if (ImGui::Button("キャラクター生成##CharacterValidation")) requestSpawn_ = true;
	ImGui::SameLine();
	if (ImGui::Button("25ダメージ##CharacterValidation")) requestDamage_ = true;

	if (ImGui::Button("致死ダメージ##CharacterValidation")) requestLethalDamage_ = true;
	ImGui::SameLine();
	if (ImGui::Button("全回復##CharacterValidation")) requestRestore_ = true;
	ImGui::SameLine();
	if (ImGui::Button("移動開始##CharacterValidation")) requestMove_ = true;
	ImGui::SameLine();
	if (ImGui::Button("移動停止##CharacterValidation")) requestStop_ = true;

	if (ImGui::Button("JSON保存##CharacterValidation")) requestSave_ = true;
	ImGui::SameLine();
	if (ImGui::Button("JSON再読込##CharacterValidation")) requestReload_ = true;
	ImGui::TextDisabled("HP、移動、当たり判定、アニメーションをそれぞれ独立して確認できます。");
	ImGui::End();
#endif
}

void CharacterValidation::Finalize()
{
	actorWorld_ = nullptr;
	listenerActor_ = nullptr;
	listenerId_ = 0;
}

K4E::CharacterActor* CharacterValidation::FindTarget() const
{
	if (!actorWorld_) return nullptr;
	for (const auto& actor : actorWorld_->GetActors())
	{
		if (actor && actor->GetName() == targetActorName_) return dynamic_cast<K4E::CharacterActor*>(actor.get());
	}
	return nullptr;
}

void CharacterValidation::ConfigureTarget(K4E::CharacterActor& actor)
{
	actor.SetName(targetActorName_);
	actor.SetLayer("DebugValidation");
	actor.AddTag("CharacterValidation");
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

void CharacterValidation::RunValidation()
{
	K4E::CharacterActor* target = FindTarget();
	K4E::CharacterHealthComponent* health = target ? target->GetHealthComponent() : nullptr;
	K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
	K4E::CharacterTargetComponent* targetComponent = target ? target->GetTargetComponent() : nullptr;
	K4E::CharacterColliderComponent* collider = target ? target->GetColliderComponent() : nullptr;
	K4E::CharacterAnimationComponent* animation = target ? target->GetAnimationComponent() : nullptr;
	K4E::SceneComponent* root = target ? target->GetRootComponent() : nullptr;
	if (!target || !health || !movement || !targetComponent || !collider || !animation || !root)
	{
		lastSucceeded_ = false;
		lastMessage_ = "検証に必要なキャラクター共通機能が見つかりません。";
		return;
	}

	ConfigureTarget(*target);
	const float originalMaxHealth = health->GetMaxHealth();
	const float originalCurrentHealth = health->GetCurrentHealth();
	const bool originalInvulnerable = health->IsInvulnerable();
	const K4E::Vector3 originalVelocity = movement->GetVelocity();
	const bool originalMovementEnabled = movement->IsMovementEnabled();
	const K4E::Vector3 originalRootPosition = root->GetLocalPosition();
	const K4E::Vector3 originalColliderPosition = collider->GetLocalPosition();
	const K4E::Vector3 originalColliderHalfSize = collider->GetHalfSize();
	const K4E::ECollisionShapeType originalColliderShape = collider->GetShapeType();
	const std::string originalAnimationName = animation->GetAnimationName();
	const float originalAnimationDuration = animation->GetDuration();
	const float originalAnimationTime = animation->GetPlaybackTime();
	const float originalAnimationSpeed = animation->GetPlaybackSpeed();
	const bool originalAnimationLoop = animation->IsLooping();
	const bool originalAnimationPlaying = animation->IsPlaying();
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

	root->SetLocalPosition({ 4.0f, 0.0f, 0.0f });
	root->RefreshWorldTransform();
	movement->SetMovementEnabled(true);
	movement->SetVelocity({ 2.0f, 0.0f, 0.0f });
	movement->Update(0.5f);
	const bool transformMovementSucceeded = std::abs(root->GetLocalPosition().x - 5.0f) < 0.0001f;

	collider->SetShapeType(K4E::ECollisionShapeType::OBB);
	collider->SetHalfSize({ 0.55f, 0.95f, 0.45f });
	collider->SetLocalPosition({ 0.0f, 0.1f, 0.0f });
	collider->UpdateEditor(0.0f);
	const K4E::Vector3 colliderWorldPosition = collider->GetWorldPosition();
	const K4E::Vector3 physicsCenter = collider->GetCollider() ? collider->GetCollider()->GetCenterPosition() : K4E::Vector3{};
	const bool colliderFollowSucceeded = collider->GetCollider() &&
		K4E::Vector3::Length(colliderWorldPosition - physicsCenter) < 0.0001f;

	animation->Play("Walk", 2.5f, true);
	animation->SetPlaybackSpeed(1.25f);
	animation->Update(0.4f);
	const bool animationUpdateSucceeded = std::abs(animation->GetPlaybackTime() - 0.5f) < 0.0001f;

	const nlohmann::json snapshot = K4E::ActorJsonSerializer::SerializeActor(*target);
	bool hasHealthJson = false;
	bool hasMovementJson = false;
	bool hasTargetJson = false;
	bool hasColliderJson = false;
	bool hasAnimationJson = false;
	if (snapshot.contains("Components") && snapshot["Components"].is_array())
	{
		for (const auto& componentJson : snapshot["Components"])
		{
			const std::string className = componentJson.value("Class", std::string{});
			hasHealthJson = hasHealthJson || className == "CharacterHealthComponent";
			hasMovementJson = hasMovementJson || className == "CharacterMovementComponent";
			hasTargetJson = hasTargetJson || className == "CharacterTargetComponent";
			hasColliderJson = hasColliderJson || className == "CharacterColliderComponent";
			hasAnimationJson = hasAnimationJson || className == "CharacterAnimationComponent";
		}
	}

	std::unique_ptr<K4E::Actor> restoredActor = K4E::ActorJsonSerializer::CreateActorFromJson(snapshot);
	K4E::CharacterActor* restoredCharacter = dynamic_cast<K4E::CharacterActor*>(restoredActor.get());
	const K4E::CharacterColliderComponent* restoredCollider = restoredCharacter ? restoredCharacter->GetColliderComponent() : nullptr;
	const K4E::CharacterAnimationComponent* restoredAnimation = restoredCharacter ? restoredCharacter->GetAnimationComponent() : nullptr;
	const bool colliderJsonRestored = restoredCollider && restoredCollider->GetShapeType() == K4E::ECollisionShapeType::OBB &&
		std::abs(restoredCollider->GetHalfSize().x - 0.55f) < 0.0001f &&
		std::abs(restoredCollider->GetHalfSize().y - 0.95f) < 0.0001f;
	const bool animationJsonRestored = restoredAnimation && restoredAnimation->GetAnimationName() == "Walk" &&
		std::abs(restoredAnimation->GetDuration() - 2.5f) < 0.0001f &&
		std::abs(restoredAnimation->GetPlaybackTime() - 0.5f) < 0.0001f &&
		std::abs(restoredAnimation->GetPlaybackSpeed() - 1.25f) < 0.0001f;
	const bool factoryRestoreSucceeded = restoredCharacter && restoredCharacter->GetHealthComponent() &&
		restoredCharacter->GetMovementComponent() && restoredCharacter->GetTargetComponent() &&
		colliderJsonRestored && animationJsonRestored;
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
	const bool movementDelegationMatches = target->GetMovementComponent() == movement &&
		movement->IsMovementEnabled() && movement->GetVelocity().x == 2.0f && transformMovementSucceeded;

	const bool succeeded = listenerRemoved && removedListenerCallCount == 0 && componentAccessMatches && movementDelegationMatches &&
		colliderFollowSucceeded && animationUpdateSucceeded && targetPositionMatches && explicitTargetMatches &&
		nonLethalResult.accepted && nonLethalResult.healthBefore == 100.0f &&
		nonLethalResult.healthAfter == 75.0f && !nonLethalResult.killed && lethalResult.accepted && lethalResult.killed &&
		lethalResult.healthAfter == 0.0f && !duplicateResult.accepted && deathEventCount_ == deathEventsBefore + 1 &&
		snapshot.value("Class", std::string{}) == "CharacterActor" && hasHealthJson && hasMovementJson && hasTargetJson &&
		hasColliderJson && hasAnimationJson && factoryRestoreSucceeded;

	health->SetMaxHealth(originalMaxHealth);
	health->SetCurrentHealth(originalCurrentHealth);
	health->SetInvulnerable(originalInvulnerable);
	movement->SetVelocity(originalVelocity);
	movement->SetMovementEnabled(originalMovementEnabled);
	root->SetLocalPosition(originalRootPosition);
	root->RefreshWorldTransform();
	collider->SetShapeType(originalColliderShape);
	collider->SetHalfSize(originalColliderHalfSize);
	collider->SetLocalPosition(originalColliderPosition);
	collider->UpdateEditor(0.0f);
	animation->Play(originalAnimationName, originalAnimationDuration, originalAnimationLoop);
	animation->SetPlaybackSpeed(originalAnimationSpeed);
	animation->SetPlaybackTime(originalAnimationTime);
	if (!originalAnimationPlaying) animation->Pause();

	std::ostringstream message;
	message << (succeeded ? "キャラクター共通機能の自動検証に成功しました。" : "キャラクター共通機能の自動検証で不整合を検出しました。")
		<< " HP=" << health->GetCurrentHealth() << "/" << health->GetMaxHealth()
		<< " DeathEvent=" << deathEventCount_
		<< " Collider=" << (colliderFollowSucceeded ? "OK" : "NG")
		<< " JSON=" << (factoryRestoreSucceeded ? "OK" : "NG");
	lastSucceeded_ = succeeded;
	lastMessage_ = message.str();
}
