#pragma once

#include <Scene/Actor/Character/CharacterActor.h>

#include <string_view>

namespace Ken4lowEngine
{
	class BossAttackComponent;
	class BossBrainComponent;
	class BossPhaseComponent;
	class BossPresentationComponent;
	class BossWeakPointComponent;
	class GaugeComponent;
	class HumanoidVisualComponent;
	class Matrix4x4;
	class TextComponent;

	/// 共通Character機能とBoss専用判断・攻撃・フェーズ・弱点・演出・HP HUDを束ねる本番Actor。
	class BossActor final : public CharacterActor
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		std::string GetClassTypeName() const override { return "BossActor"; }
		void SetTargetActor(CharacterActor* targetActor);
		CharacterDamageResult ApplyWeakPointDamage(std::string_view partId, float baseDamage);
		CharacterDamageResult ApplyBulletDamage(float damage, const Vector3& hitPosition);
		CharacterDamageResult ApplyBulletDamage(const CharacterDamageInfo& damageInfo) { return ApplyDamage(damageInfo); } // 弾・爆発とも同じDamage構造をBoss Healthへ渡す。
		void ResetForValidation(const Vector3& worldPosition);
		void SetPosition(const Vector3& worldPosition);
		Vector3 GetPosition() const;
		void SetYaw(float yaw);
		void ClearRootParentKeepingWorldPosition();
		void ForceSyncWorldTransform();
		bool HasRootParent() const;
		Vector3 GetRootLocalPosition() const;
		Vector3 GetRootWorldPosition() const;
		void SetBattleEnabled(bool enabled);
		bool IsBattleEnabled() const { return battleEnabled_; }
		void SetHealthHudVisible(bool visible);
		void UpdateShadowMatrix(const Matrix4x4& lightViewProjection);
		float GetHP() const;
		float GetMaxHP() const;
		int GetCurrentPhase() const;
		unsigned int GetPhaseRevision() const;
		bool IsDeathPresentationComplete() const;
		const Vector3& GetDeathWorldPosition() const { return deathWorldPosition_; }
		BossBrainComponent* GetBossBrainComponent();
		const BossBrainComponent* GetBossBrainComponent() const;
		BossAttackComponent* GetBossAttackComponent();
		const BossAttackComponent* GetBossAttackComponent() const;
		BossPhaseComponent* GetBossPhaseComponent();
		const BossPhaseComponent* GetBossPhaseComponent() const;
		BossWeakPointComponent* GetBossWeakPointComponent();
		const BossWeakPointComponent* GetBossWeakPointComponent() const;
		BossPresentationComponent* GetBossPresentationComponent();
		const BossPresentationComponent* GetBossPresentationComponent() const;
		HumanoidVisualComponent* GetHumanoidVisualComponent();
		const HumanoidVisualComponent* GetHumanoidVisualComponent() const;
		GaugeComponent* GetHealthGaugeComponent();
		const GaugeComponent* GetHealthGaugeComponent() const;
		TextComponent* GetHealthLabelComponent();
		const TextComponent* GetHealthLabelComponent() const;

	protected:
		void OnDeath(const CharacterDeathEvent& deathEvent) override;

	private:
		void SyncHealthHud();
		void RestoreDeathWorldPosition();

	private:
		CharacterActor* targetActor_ = nullptr;
		Vector3 deathWorldPosition_{};
		bool battleEnabled_ = false;
		bool hasDeathWorldPosition_ = false;
	};
} // namespace Ken4lowEngine
