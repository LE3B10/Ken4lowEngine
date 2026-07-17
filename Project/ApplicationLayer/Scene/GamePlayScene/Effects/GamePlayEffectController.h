#pragma once

#include "GamePlayWorld.h"
#include "PostEffect/PlayerHealthPostEffectController.h"

#include <Camera.h>
#include <GpuParticleManager.h>
#include <Matrix4x4.h>
#include <ModelComponent.h>
#include <PostEffectManager.h>
#include <Player.h>

#include <algorithm>
#include <cmath>
#include <memory>

/// <summary>
/// GamePlayScene で発生する演出系 Controller です。<br/>
/// Player の被弾通知、HP連動ポストエフェクト、武器発射VFX、Boss被弾VFXを担当します。
/// </summary>
class GamePlayEffectController
{
public:
	/// <summary>
	/// World 内の Player、弾、Boss と演出システムを接続します。
	/// </summary>
	void Initialize(GamePlayWorld* world)
	{
		hpPostEffectController_ = std::make_unique<PlayerHealthPostEffectController>();
		hpPostEffectController_->Initialize(Ken4lowEngine::PostEffectManager::GetInstance());
		Ken4lowEngine::EffectSystem::GetInstance()->RegisterSpriteEffect(
			"BossBlood",
			Ken4lowEngine::GpuParticleType::Blood,
			64,
			0,
			0.0f,
			0.30f);
		trackedBoss_ = nullptr;
		lastBossHp_ = 0.0f;
		BindPlayerDamageCallback(world);
		BindWeaponFireEffectTransform(world);
	}

	/// <summary>
	/// 古い Player・BulletManager・Boss 参照が残らないよう、Scene 終了やリトライ再生成前に接続を解除します。
	/// </summary>
	void Finalize(GamePlayWorld* world)
	{
		if (auto* player = world ? world->GetCharacters().GetPlayer() : nullptr)
		{
			player->SetOnDamageTakenCallback({});
		}
		if (BulletManager* bulletManager = world ? world->GetBulletManager() : nullptr)
		{
			bulletManager->SetShotEffectTransformResolver({});
		}
		trackedBoss_ = nullptr;
		lastBossHp_ = 0.0f;

		if (hpPostEffectController_)
		{
			hpPostEffectController_->Finalize();
			hpPostEffectController_.reset();
		}
	}

	/// <summary>
	/// HP 状態、Player被弾、Boss被弾に応じた演出を更新します。
	/// </summary>
	void Update(float deltaTime, GamePlayWorld* world)
	{
		if (hpPostEffectController_)
		{
			// 読み取り専用のHP連携から先に具象Player依存を外し、旧コールバック経路は安定するまで残す。
			hpPostEffectController_->Update(deltaTime, world ? world->GetCharacters().GetPlayerRuntime() : nullptr);
		}
		UpdateBossHitBlood(world);
	}

	/// <summary>
	/// Player Debug ウィンドウへ、演出調整用の ImGui 内容を描画します。
	/// </summary>
	void DrawPlayerDebugContent()
	{
		if (hpPostEffectController_)
		{
			hpPostEffectController_->DrawImGuiContent();
		}
	}

	bool HasPlayerDebugContent() const { return hpPostEffectController_ != nullptr; }

private:
	static Ken4lowEngine::Vector3 GetMuzzleLocalOffset(int weaponId)
	{
		switch (weaponId)
		{
		case 1: return { 0.0f, -0.01f, 0.58f };
		case 3: return { 0.0f, 0.00f, 0.82f };
		case 4: return { 0.0f, 0.00f, 1.08f };
		case 5: return { 0.0f, 0.00f, 0.92f };
		default: return { 0.0f, -0.02f, 0.85f };
		}
	}

	static bool ResolveWeaponMuzzleTransform(
		GamePlayWorld* world,
		Ken4lowEngine::Vector3& outPosition,
		Ken4lowEngine::Vector3& outDirection)
	{
		auto* player = world ? world->GetCharacters().GetPlayer() : nullptr;
		const auto* weaponView = player ? player->GetWeaponViewComponent() : nullptr;
		const auto* weapon = player ? player->GetWeaponComponent() : nullptr;
		Ken4lowEngine::Camera* camera = player ? player->GetCamera() : nullptr;
		if (!weaponView || !weapon || !camera || weapon->IsMeleeWeapon()) return false;

		const Ken4lowEngine::Matrix4x4 cameraRotation = Ken4lowEngine::Matrix4x4::MakeRotateMatrix(camera->GetRotate());
		const Ken4lowEngine::Vector3 weaponWorldPosition =
			camera->GetTranslate() + Ken4lowEngine::Vector3::Transform(weaponView->GetLocalPosition(), cameraRotation);
		const Ken4lowEngine::Vector3 weaponWorldRotation = camera->GetRotate() + weaponView->GetLocalRotation();
		const Ken4lowEngine::Matrix4x4 weaponWorldMatrix = Ken4lowEngine::Matrix4x4::MakeAffineMatrix(
			weaponView->GetLocalScale(),
			weaponWorldRotation,
			weaponWorldPosition);

		outPosition = Ken4lowEngine::Vector3::Transform(GetMuzzleLocalOffset(weapon->GetWeaponId()), weaponWorldMatrix);
		outDirection = Ken4lowEngine::Vector3::Normalize(camera->GetForward());
		const bool finitePosition = std::isfinite(outPosition.x) && std::isfinite(outPosition.y) && std::isfinite(outPosition.z);
		const bool finiteDirection = std::isfinite(outDirection.x) && std::isfinite(outDirection.y) && std::isfinite(outDirection.z);
		return finitePosition && finiteDirection; // ModelComponentの描画と同じViewModel行列から銃口位置を復元する。
	}

	void BindPlayerDamageCallback(GamePlayWorld* world)
	{
		if (auto* player = world ? world->GetCharacters().GetPlayer() : nullptr)
		{
			player->SetOnDamageTakenCallback([this]()
				{
					if (hpPostEffectController_)
					{
						hpPostEffectController_->NotifyDamageTaken();
					}
				});
		}
	}

	void BindWeaponFireEffectTransform(GamePlayWorld* world)
	{
		if (BulletManager* bulletManager = world ? world->GetBulletManager() : nullptr)
		{
			bulletManager->SetShotEffectTransformResolver(
				[world](Ken4lowEngine::Vector3& position, Ken4lowEngine::Vector3& direction)
				{
					return ResolveWeaponMuzzleTransform(world, position, direction);
				});
		}
	}

	void UpdateBossHitBlood(GamePlayWorld* world)
	{
		GuardianBoss* boss = world ? world->GetBoss() : nullptr;
		if (boss != trackedBoss_)
		{
			trackedBoss_ = boss;
			lastBossHp_ = boss ? boss->GetHP() : 0.0f;
			return;
		}
		if (!boss) return;

		const float currentHp = boss->GetHP();
		if (currentHp + 0.001f < lastBossHp_)
		{
			const float appliedDamage = lastBossHp_ - currentHp;
			Ken4lowEngine::Vector3 bloodPosition = boss->GetPosition();
			bloodPosition.y += 1.15f;
			const uint32_t emitCount = static_cast<uint32_t>(std::clamp(
				48L + std::lround(appliedDamage * 1.4f),
				48L,
				120L));
			Ken4lowEngine::EffectSystem::GetInstance()->Play("BossBlood", bloodPosition, emitCount); // Bossの体格と被ダメージ量に合わせて大きな血飛沫を出す。
		}
		lastBossHp_ = currentHp;
	}

	std::unique_ptr<PlayerHealthPostEffectController> hpPostEffectController_;
	GuardianBoss* trackedBoss_ = nullptr;
	float lastBossHp_ = 0.0f;
};
