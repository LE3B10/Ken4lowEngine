#define NOMINMAX
#include "GuardianBoss.h"
#include "BossPunchAttack.h"
#include "BossHeavyPunchAttack.h"
#include "GuardianShockwaveAttack.h"
#include "BossChargeAttack.h"
#include <LinearInterpolation.h>
#include <LogString.h>
#include <ParameterManager.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	constexpr const char* kGuardianBossGroup = "GuardianBoss";
	constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

	void EnsureGuardianBossParameters()
	{
		static bool isInitialized = false;
		if (isInitialized)
		{
			return;
		}
		isInitialized = true;

		auto* parameters = ParameterManager::GetInstance();
		parameters->CreateGroup(kGuardianBossGroup);

		// Guardian固有JSONがあれば読み込み、不足分は現行挙動と同じ既定値で補完する。
		if (std::filesystem::exists("Resources/ParameterManager/GuardianBoss.json"))
		{
			parameters->LoadFile(kGuardianBossGroup);
		}
		else
		{
			Log("[GuardianBoss] GuardianBoss.json not found. Use built-in default guardian parameters.\n");
		}

		// 速度・攻撃範囲・時間系パラメータは巨大な範囲ではなく実用的な範囲で編集する。
		parameters->AddItem(kGuardianBossGroup, "moveSpeed", 2.0f, 0.0f, 50.0f);
		parameters->AddItem(kGuardianBossGroup, "rotateSpeed", 4.0f, 0.0f, 30.0f);
		parameters->AddItem(kGuardianBossGroup, "attackRange", 5.75f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianAttackHitRange", 6.0f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianAttackHitRadius", 2.0f, 0.0f, 30.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianAttackForwardOffset", 3.0f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianAttackHitAngleDeg", 90.0f, 0.0f, 360.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianShockwaveRange", 10.0f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianShockwaveAngleDeg", 70.0f, 0.0f, 360.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianShockwaveDamage", 15.0f, 0.0f, 999.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianShockwaveCooldown", 6.0f, 0.0f, 60.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianShockwaveStartupSec", 0.8f, 0.0f, 10.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianShockwaveActiveSec", 0.25f, 0.0f, 10.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianShockwaveRecoverySec", 1.0f, 0.0f, 10.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianCloseAttackRange", 4.0f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianMiddleAttackRange", 10.0f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianFarAttackRange", 20.0f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianChargeSpeed", 18.0f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianChargeDistance", 12.0f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianChargeDamage", 20.0f, 0.0f, 999.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianChargeStartupSec", 0.6f, 0.0f, 10.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianChargeRecoverySec", 1.0f, 0.0f, 10.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianChargeCooldown", 8.0f, 0.0f, 60.0f);
		parameters->AddItem(kGuardianBossGroup, "HitFlashDuration", 0.18f, 0.0f, 3.0f);
		parameters->AddItem(kGuardianBossGroup, "HitFlashIntensity", 2.2f, 0.0f, 10.0f);
		parameters->AddItem(kGuardianBossGroup, "ParticleSpawnCount", 48, 0, 1000);
		parameters->AddItem(kGuardianBossGroup, "ParticleSpawnRadius", 0.5f, 0.0f, 20.0f);
		parameters->AddItem(kGuardianBossGroup, "ParticleLifetime", 1.0f, 0.01f, 10.0f);
		parameters->AddItem(kGuardianBossGroup, "ParticleInitialSpeed", 1.0f, 0.0f, 10.0f);
		parameters->AddItem(kGuardianBossGroup, "moveStartDistance", 4.8f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "moveStopDistance", 4.8f, 0.0f, 100.0f);
		parameters->AddItem(kGuardianBossGroup, "attackDuration", 0.85f, 0.0f, 30.0f);
		parameters->AddItem(kGuardianBossGroup, "attackCooldown", 1.20f, 0.0f, 30.0f);
		parameters->AddItem(kGuardianBossGroup, "staggerDuration", 0.30f, 0.0f, 30.0f);
		parameters->AddItem(kGuardianBossGroup, "heavyPunchReuseDelay", 1.0f, 0.0f, 30.0f);
		parameters->AddItem(kGuardianBossGroup, "animationWalkSpeed", 6.0f, 0.0f, 30.0f);
		parameters->AddItem(kGuardianBossGroup, "animationWalkAmplitude", 0.55f, 0.0f, 5.0f);
		parameters->AddItem(kGuardianBossGroup, "GuardianModelYawOffsetDeg", 180.0f, -360.0f, 360.0f);
		parameters->AddItem(kGuardianBossGroup, "skinPath", std::string("Characters/zombie.dds"));
		// 内部キーとは別にImGui専用の日本語ラベルを登録する。
		parameters->SetDisplayName(kGuardianBossGroup, "moveSpeed", "ガーディアン移動速度");
		parameters->SetDisplayName(kGuardianBossGroup, "rotateSpeed", "ガーディアン旋回速度");
		parameters->SetDisplayName(kGuardianBossGroup, "attackRange", "ガーディアン攻撃距離");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianAttackHitRange", "攻撃判定リーチ");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianAttackHitRadius", "攻撃判定半径");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianAttackForwardOffset", "攻撃判定前方オフセット");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianAttackHitAngleDeg", "攻撃判定角度");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianShockwaveRange", "衝撃波リーチ");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianShockwaveAngleDeg", "衝撃波角度");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianShockwaveDamage", "衝撃波ダメージ");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianShockwaveCooldown", "衝撃波クールタイム");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianShockwaveStartupSec", "衝撃波予備動作");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianShockwaveActiveSec", "衝撃波判定時間");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianShockwaveRecoverySec", "衝撃波後隙");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianCloseAttackRange", "近距離攻撃範囲");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianMiddleAttackRange", "中距離攻撃範囲");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianFarAttackRange", "遠距離検知範囲");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianChargeSpeed", "突進速度");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianChargeDistance", "突進距離");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianChargeDamage", "突進ダメージ");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianChargeStartupSec", "突進予備動作");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianChargeRecoverySec", "突進後隙");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianChargeCooldown", "突進クールタイム");
		parameters->SetDisplayName(kGuardianBossGroup, "HitFlashDuration", "被弾点滅時間");
		parameters->SetDisplayName(kGuardianBossGroup, "HitFlashIntensity", "被弾点滅強度");
		parameters->SetDisplayName(kGuardianBossGroup, "ParticleSpawnCount", "ヒット粒子数");
		parameters->SetDisplayName(kGuardianBossGroup, "ParticleSpawnRadius", "ヒット粒子発生半径");
		parameters->SetDisplayName(kGuardianBossGroup, "ParticleLifetime", "ヒット粒子寿命倍率");
		parameters->SetDisplayName(kGuardianBossGroup, "ParticleInitialSpeed", "ヒット粒子初速倍率");
		parameters->SetDisplayName(kGuardianBossGroup, "moveStartDistance", "移動開始距離");
		parameters->SetDisplayName(kGuardianBossGroup, "moveStopDistance", "移動停止距離");
		parameters->SetDisplayName(kGuardianBossGroup, "attackDuration", "攻撃時間");
		parameters->SetDisplayName(kGuardianBossGroup, "attackCooldown", "攻撃クールタイム");
		parameters->SetDisplayName(kGuardianBossGroup, "staggerDuration", "ひるみ時間");
		parameters->SetDisplayName(kGuardianBossGroup, "heavyPunchReuseDelay", "強攻撃再使用間隔");
		parameters->SetDisplayName(kGuardianBossGroup, "animationWalkSpeed", "歩行アニメ速度");
		parameters->SetDisplayName(kGuardianBossGroup, "animationWalkAmplitude", "歩行アニメ振幅");
		parameters->SetDisplayName(kGuardianBossGroup, "GuardianModelYawOffsetDeg", "ガーディアンモデル向き補正");
		parameters->SetDisplayName(kGuardianBossGroup, "skinPath", "スキンパス");
	}

	template<typename T>
	T GetGuardianParameterOrDefault(const std::string& key, const T& defaultValue)
	{
		try
		{
			return ParameterManager::GetInstance()->GetValue<T>(kGuardianBossGroup, key);
		}
		catch (const std::exception& e)
		{
			Log("[GuardianBoss] Failed to read GuardianBoss." + key + ": " + e.what() + ". Use default.\n");
			return defaultValue;
		}
	}
}


GuardianBoss::~GuardianBoss()
{
	// Guardian固有コールバックを破棄時に解除し、無効ポインタへの反映を防ぐ。
	ParameterManager::GetInstance()->UnregisterParameterApplier(kGuardianBossGroup, this);
}

void GuardianBoss::Finalize()
{
	ParameterManager::GetInstance()->UnregisterParameterApplier(kGuardianBossGroup, this); // Finalize後にGuardian固有値を無効な部位へ反映しないよう解除する。
	BossBase::Finalize();
}


void GuardianBoss::ApplyParameters()
{
	BossBase::ApplyParameters();
	EnsureGuardianBossParameters();
	moveSpeed_ = GetGuardianParameterOrDefault("moveSpeed", moveSpeed_);
	rotateSpeed_ = GetGuardianParameterOrDefault("rotateSpeed", rotateSpeed_);
	attackRange_ = GetGuardianParameterOrDefault("attackRange", attackRange_);
	attackHitRange_ = GetGuardianParameterOrDefault("GuardianAttackHitRange", attackHitRange_);
	attackHitRadius_ = GetGuardianParameterOrDefault("GuardianAttackHitRadius", attackHitRadius_);
	attackForwardOffset_ = GetGuardianParameterOrDefault("GuardianAttackForwardOffset", attackForwardOffset_);
	attackHitAngleDeg_ = GetGuardianParameterOrDefault("GuardianAttackHitAngleDeg", attackHitAngleDeg_);
	shockwaveRange_ = GetGuardianParameterOrDefault("GuardianShockwaveRange", shockwaveRange_); // Guardian衝撃波リーチをJSON調整値から復元する
	shockwaveAngleDeg_ = GetGuardianParameterOrDefault("GuardianShockwaveAngleDeg", shockwaveAngleDeg_); // Guardian衝撃波角度をJSON調整値から復元する
	shockwaveDamage_ = GetGuardianParameterOrDefault("GuardianShockwaveDamage", shockwaveDamage_); // Guardian衝撃波ダメージをJSON調整値から復元する
	shockwaveCooldown_ = GetGuardianParameterOrDefault("GuardianShockwaveCooldown", shockwaveCooldown_); // Guardian衝撃波クールタイムをJSON調整値から復元する
	shockwaveStartupSec_ = GetGuardianParameterOrDefault("GuardianShockwaveStartupSec", shockwaveStartupSec_); // Guardian衝撃波予備動作をJSON調整値から復元する
	shockwaveActiveSec_ = GetGuardianParameterOrDefault("GuardianShockwaveActiveSec", shockwaveActiveSec_); // Guardian衝撃波判定時間をJSON調整値から復元する
	shockwaveRecoverySec_ = GetGuardianParameterOrDefault("GuardianShockwaveRecoverySec", shockwaveRecoverySec_); // Guardian衝撃波後隙をJSON調整値から復元する
	closeAttackRange_ = GetGuardianParameterOrDefault("GuardianCloseAttackRange", closeAttackRange_);
	middleAttackRange_ = GetGuardianParameterOrDefault("GuardianMiddleAttackRange", middleAttackRange_);
	farAttackRange_ = GetGuardianParameterOrDefault("GuardianFarAttackRange", farAttackRange_);
	chargeSpeed_ = GetGuardianParameterOrDefault("GuardianChargeSpeed", chargeSpeed_);
	chargeDistance_ = GetGuardianParameterOrDefault("GuardianChargeDistance", chargeDistance_);
	chargeDamage_ = GetGuardianParameterOrDefault("GuardianChargeDamage", chargeDamage_);
	chargeStartupSec_ = GetGuardianParameterOrDefault("GuardianChargeStartupSec", chargeStartupSec_);
	chargeRecoverySec_ = GetGuardianParameterOrDefault("GuardianChargeRecoverySec", chargeRecoverySec_);
	chargeCooldown_ = GetGuardianParameterOrDefault("GuardianChargeCooldown", chargeCooldown_);
	middleAttackRange_ = std::max(closeAttackRange_, middleAttackRange_); // 距離帯が逆転した設定でも近距離→中距離→遠距離の順を保つ。
	farAttackRange_ = std::max(middleAttackRange_, farAttackRange_);
	particleSpawnCount_ = static_cast<uint32_t>(std::max(0, GetGuardianParameterOrDefault("ParticleSpawnCount", static_cast<int>(particleSpawnCount_))));
	particleSpawnRadius_ = GetGuardianParameterOrDefault("ParticleSpawnRadius", particleSpawnRadius_);
	particleLifetime_ = GetGuardianParameterOrDefault("ParticleLifetime", particleLifetime_);
	particleInitialSpeed_ = GetGuardianParameterOrDefault("ParticleInitialSpeed", particleInitialSpeed_);
	moveStartDistance_ = GetGuardianParameterOrDefault("moveStartDistance", moveStartDistance_);
	moveStopDistance_ = GetGuardianParameterOrDefault("moveStopDistance", moveStopDistance_);
	attackDuration_ = GetGuardianParameterOrDefault("attackDuration", attackDuration_);
	attackCooldown_ = GetGuardianParameterOrDefault("attackCooldown", attackCooldown_);
	staggerDuration_ = GetGuardianParameterOrDefault("staggerDuration", staggerDuration_);
	heavyPunchReuseDelay_ = GetGuardianParameterOrDefault("heavyPunchReuseDelay", heavyPunchReuseDelay_);
	animationWalkSpeed_ = GetGuardianParameterOrDefault("animationWalkSpeed", animationWalkSpeed_);
	animationWalkAmplitude_ = GetGuardianParameterOrDefault("animationWalkAmplitude", animationWalkAmplitude_);
	guardianModelYawOffsetDeg_ = GetGuardianParameterOrDefault("GuardianModelYawOffsetDeg", guardianModelYawOffsetDeg_);
	if (GetAnimationComponent())
	{
		GetAnimationComponent()->SetWalkSpeed(animationWalkSpeed_);
		GetAnimationComponent()->SetWalkAmplitude(animationWalkAmplitude_);
		GetAnimationComponent()->SetAttackDuration(attackDuration_);
	}
	ApplyAttackHitParametersToAttacks(); // 攻撃判定リーチ変更は保存/反映後に実行中の攻撃処理へ再適用する。
	ApplySkinToAllParts(GetGuardianSkinPath()); // スキンパス変更は保存/反映後に実行中モデルへ再適用する。
}


std::string GuardianBoss::GetGuardianSkinPath() const
{
	EnsureGuardianBossParameters();
	return GetGuardianParameterOrDefault("skinPath", std::string("Characters/zombie.dds")); // スキンもJSONから参照し、失敗時は既定テクスチャへ戻す
}

float GuardianBoss::GetModelYawOffsetRad() const
{
	return guardianModelYawOffsetDeg_ * kDegToRad;
}

/// -------------------------------------------------------------
/// Guardian 固有初期化
/// -------------------------------------------------------------
void GuardianBoss::SetupBoss()
{
	// 人型ボス共通初期化
	HumanoidBossBase::SetupBoss();

	EnsureGuardianBossParameters();
	moveSpeed_ = GetGuardianParameterOrDefault("moveSpeed", moveSpeed_); // Guardianの移動速度をJSON調整値から復元する
	rotateSpeed_ = GetGuardianParameterOrDefault("rotateSpeed", rotateSpeed_); // Guardianの旋回速度をJSON調整値から復元する
	attackRange_ = GetGuardianParameterOrDefault("attackRange", attackRange_); // Guardianの攻撃開始距離をJSON調整値から復元する
	attackHitRange_ = GetGuardianParameterOrDefault("GuardianAttackHitRange", attackHitRange_); // Guardianの攻撃判定リーチをJSON調整値から復元する
	attackHitRadius_ = GetGuardianParameterOrDefault("GuardianAttackHitRadius", attackHitRadius_); // Guardianの攻撃判定半径をJSON調整値から復元する
	attackForwardOffset_ = GetGuardianParameterOrDefault("GuardianAttackForwardOffset", attackForwardOffset_); // Guardianの攻撃判定前方オフセットをJSON調整値から復元する
	attackHitAngleDeg_ = GetGuardianParameterOrDefault("GuardianAttackHitAngleDeg", attackHitAngleDeg_); // Guardianの攻撃判定角度をJSON調整値から復元する
	shockwaveRange_ = GetGuardianParameterOrDefault("GuardianShockwaveRange", shockwaveRange_); // Guardian衝撃波リーチをJSON調整値から復元する
	shockwaveAngleDeg_ = GetGuardianParameterOrDefault("GuardianShockwaveAngleDeg", shockwaveAngleDeg_); // Guardian衝撃波角度をJSON調整値から復元する
	shockwaveDamage_ = GetGuardianParameterOrDefault("GuardianShockwaveDamage", shockwaveDamage_); // Guardian衝撃波ダメージをJSON調整値から復元する
	shockwaveCooldown_ = GetGuardianParameterOrDefault("GuardianShockwaveCooldown", shockwaveCooldown_); // Guardian衝撃波クールタイムをJSON調整値から復元する
	shockwaveStartupSec_ = GetGuardianParameterOrDefault("GuardianShockwaveStartupSec", shockwaveStartupSec_); // Guardian衝撃波予備動作をJSON調整値から復元する
	shockwaveActiveSec_ = GetGuardianParameterOrDefault("GuardianShockwaveActiveSec", shockwaveActiveSec_); // Guardian衝撃波判定時間をJSON調整値から復元する
	shockwaveRecoverySec_ = GetGuardianParameterOrDefault("GuardianShockwaveRecoverySec", shockwaveRecoverySec_); // Guardian衝撃波後隙をJSON調整値から復元する
	closeAttackRange_ = GetGuardianParameterOrDefault("GuardianCloseAttackRange", closeAttackRange_);
	middleAttackRange_ = GetGuardianParameterOrDefault("GuardianMiddleAttackRange", middleAttackRange_);
	farAttackRange_ = GetGuardianParameterOrDefault("GuardianFarAttackRange", farAttackRange_);
	chargeSpeed_ = GetGuardianParameterOrDefault("GuardianChargeSpeed", chargeSpeed_);
	chargeDistance_ = GetGuardianParameterOrDefault("GuardianChargeDistance", chargeDistance_);
	chargeDamage_ = GetGuardianParameterOrDefault("GuardianChargeDamage", chargeDamage_);
	chargeStartupSec_ = GetGuardianParameterOrDefault("GuardianChargeStartupSec", chargeStartupSec_);
	chargeRecoverySec_ = GetGuardianParameterOrDefault("GuardianChargeRecoverySec", chargeRecoverySec_);
	chargeCooldown_ = GetGuardianParameterOrDefault("GuardianChargeCooldown", chargeCooldown_);
	middleAttackRange_ = std::max(closeAttackRange_, middleAttackRange_); // 距離帯が逆転した設定でも近距離→中距離→遠距離の順を保つ。
	farAttackRange_ = std::max(middleAttackRange_, farAttackRange_);
	particleSpawnCount_ = static_cast<uint32_t>(std::max(0, GetGuardianParameterOrDefault("ParticleSpawnCount", static_cast<int>(particleSpawnCount_))));
	particleSpawnRadius_ = GetGuardianParameterOrDefault("ParticleSpawnRadius", particleSpawnRadius_);
	particleLifetime_ = GetGuardianParameterOrDefault("ParticleLifetime", particleLifetime_);
	particleInitialSpeed_ = GetGuardianParameterOrDefault("ParticleInitialSpeed", particleInitialSpeed_);
	moveStartDistance_ = GetGuardianParameterOrDefault("moveStartDistance", moveStartDistance_); // Guardianの移動開始距離をJSON調整値から復元する
	moveStopDistance_ = GetGuardianParameterOrDefault("moveStopDistance", moveStopDistance_); // Guardianの移動停止距離をJSON調整値から復元する
	attackDuration_ = GetGuardianParameterOrDefault("attackDuration", attackDuration_); // Guardianの攻撃時間をJSON調整値から復元する
	attackCooldown_ = GetGuardianParameterOrDefault("attackCooldown", attackCooldown_); // Guardianの攻撃後クールタイムをJSON調整値から復元する
	staggerDuration_ = GetGuardianParameterOrDefault("staggerDuration", staggerDuration_); // Guardianのひるみ時間をJSON調整値から復元する
	heavyPunchReuseDelay_ = GetGuardianParameterOrDefault("heavyPunchReuseDelay", heavyPunchReuseDelay_); // HeavyPunch再使用間隔をJSON調整値から復元する
	animationWalkSpeed_ = GetGuardianParameterOrDefault("animationWalkSpeed", animationWalkSpeed_); // 歩行アニメ速度をJSON調整値から復元する
	animationWalkAmplitude_ = GetGuardianParameterOrDefault("animationWalkAmplitude", animationWalkAmplitude_); // 歩行アニメ振幅をJSON調整値から復元する
	guardianModelYawOffsetDeg_ = GetGuardianParameterOrDefault("GuardianModelYawOffsetDeg", guardianModelYawOffsetDeg_); // 攻撃forwardは変えず、Guardianメッシュの見た目正面だけ補正する。

	// フェーズ初期化
	SetPhase(BossPhase::Phase1);

	stateTimer_ = 0.0f;
	attackCooldownTimer_ = 0.0f;
	hasAppliedAttackHit_ = false;
	receivedHitCount_ = 0;
	bulletHitCount_ = 0;
	bossAttackHitCount_ = 0;
	lastReceivedDamage_ = 0.0f;
	lastPlayerDamage_ = 0.0f;

	// HeavyPunch 連打抑制初期化
	lastSelectedAttack_ = "None";
	heavyPunchReuseTimer_ = 0.0f;

	// ---------------------------------------------------------
	// 初期状態は Idle
	// ここも StateMachine 経由で合わせる
	// ---------------------------------------------------------
	ChangeBossState(BossState::Idle);

	// ---------------------------------------------------------
	// アニメーションコンポーネントへ Guardian 用パラメータを渡す
	// ---------------------------------------------------------
	if (GetAnimationComponent())
	{
		GetAnimationComponent()->SetWalkSpeed(animationWalkSpeed_);
		GetAnimationComponent()->SetWalkAmplitude(animationWalkAmplitude_);
		GetAnimationComponent()->SetAttackDuration(attackDuration_);

		GetAnimationComponent()->ResetWalkTimer();
		GetAnimationComponent()->ResetAttackTimer();
		GetAnimationComponent()->ResetAllPose(1.0f);
	}

	// 攻撃判定パラメータを登録済み攻撃へ反映
	ApplyAttackHitParametersToAttacks();

	// スキン一括適用
	ApplySkinToAllParts(GetGuardianSkinPath());

	ParameterManager::GetInstance()->RegisterParameterApplier(kGuardianBossGroup, this, [this]() { ApplyParameters(); }); // 保存/反映後にGuardian固有値を実行中のボスへ再適用する。
}

/// -------------------------------------------------------------
/// ダメージ
/// 軽いひるみへ移行
/// -------------------------------------------------------------
void GuardianBoss::OnDamaged(float damage)
{
	if (GetState() == BossState::Dead)
	{
		return;
	}

	const float hpBefore = GetHP();

	// HP減算は基底側に任せる
	BossBase::OnDamaged(damage);

	if (GetHP() < hpBefore)
	{
		++receivedHitCount_;
		lastReceivedDamage_ = damage;
	}

	{
		std::ostringstream oss;
		oss << "[GuardianBoss] HP=" << GetHP() << "/" << GetMaxHP()
			<< ", hitCount=" << receivedHitCount_
			<< ", lastDamage=" << lastReceivedDamage_;
		Log(oss.str() + "\n");
	}

	// 生きていたらひるみへ
	if (!IsDead())
	{
		BeginStaggerState();
	}
}

/// -------------------------------------------------------------
/// 死亡
/// -------------------------------------------------------------
void GuardianBoss::OnDead()
{
	ChangeBossState(BossState::Dead);
	stateTimer_ = 0.0f;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetAllPose(1.0f);
	}
}

/// -------------------------------------------------------------
/// 銃弾ダメージ
/// -------------------------------------------------------------
void GuardianBoss::OnBulletDamaged(float damage)
{
	++bulletHitCount_;
	OnDamaged(damage);

	std::ostringstream oss;
	oss << "[GuardianBoss] Player bullet hit: damage=" << damage
		<< ", bulletHitCount=" << bulletHitCount_
		<< ", HP=" << GetHP() << "/" << GetMaxHP();
	Log(oss.str() + "\n");
}

/// -------------------------------------------------------------
/// 衝突
/// -------------------------------------------------------------
void GuardianBoss::OnCollision(Collider* other)
{
	(void)other;
}

void GuardianBoss::OnTargetPlayerDamaged(float damage)
{
	++bossAttackHitCount_;
	lastPlayerDamage_ = damage;

	std::ostringstream oss;
	oss << "[GuardianBoss] Boss attack damaged player: damage=" << damage
		<< ", hitCount=" << bossAttackHitCount_;
	Log(oss.str() + "\n");
}

/// -------------------------------------------------------------
/// 状態更新
/// Guardian の思考をここで決める
/// -------------------------------------------------------------
void GuardianBoss::UpdateState(float deltaTime)
{
	stateTimer_ += deltaTime;
	attackCooldownTimer_ = std::max(0.0f, attackCooldownTimer_ - deltaTime);

	// ---------------------------------------------------------
	// HeavyPunch の連打抑制タイマー
	// ---------------------------------------------------------
	heavyPunchReuseTimer_ = std::max(0.0f, heavyPunchReuseTimer_ - deltaTime);

	CheckDeath();
	if (GetState() == BossState::Dead)
	{
		return;
	}

	switch (GetState())
	{
	case BossState::Intro:
		{
			BeginIdleState();
			break;
		}

	case BossState::Idle:
		{
			const float distance = GetDistanceToTargetXZ();

			// ---------------------------------------------------------
			// クールタイム中は完全停止
			// ・移動しない
			// ・向き直りもしない
			// ・攻撃もしない
			// ---------------------------------------------------------
			if (attackCooldownTimer_ > 0.0f)
			{
				break;
			}

			FaceTarget(deltaTime);

			const bool hasStartableAttack = GetAttackComponent() && !GetAttackComponent()->CollectStartableAttacks().empty();

			// 実際に開始可能な攻撃がある時だけAttackへ入り、候補なしAttackで歩行アニメが止まる状態を避ける。
			if (hasStartableAttack)
			{
				BeginAttackState();
			}
			// 遠ければ移動へ
			else if (distance > moveStartDistance_)
			{
				BeginMoveState();
			}
			break;
		}

	case BossState::Move:
		{
			FaceTarget(deltaTime);

			const float distance = GetDistanceToTargetXZ();
			const bool hasStartableAttack = GetAttackComponent() && !GetAttackComponent()->CollectStartableAttacks().empty();

			// 移動中も開始可能な攻撃だけをAttackへ渡し、歩行と攻撃選択の責務を分ける。
			if (attackCooldownTimer_ <= 0.0f && hasStartableAttack)
			{
				BeginAttackState();
			}
			// 十分近づいたら待機
			else if (distance <= moveStopDistance_)
			{
				BeginIdleState();
			}
			break;
		}

	case BossState::Attack:
		{
			// 攻撃中は各攻撃が開始時に固定した向きを使うため、毎フレームの向き直りは行わない。

			// ---------------------------------------------------------
			// 手動デバッグ中でない場合のみ、自動で攻撃を選ぶ
			// ---------------------------------------------------------
			if (!useManualAttackDebug_)
			{
				if (GetAttackComponent() && !GetAttackComponent()->IsAttacking())
				{
					if (stateTimer_ <= 0.10f)
					{
						TryStartBestAttack();
					}
				}
			}

			// ---------------------------------------------------------
			// 少し待ってから攻撃終了判定
			// 手動デバッグ中でも、攻撃が終わったら Idle に戻す
			// ---------------------------------------------------------
			if (stateTimer_ >= 0.05f)
			{
				if (GetAttackComponent() && !GetAttackComponent()->IsAttacking())
				{
					attackCooldownTimer_ = attackCooldown_;
					hasAppliedAttackHit_ = false;
					BeginIdleState();
				}
			}
			break;
		}

	case BossState::Stagger:
		{
			if (stateTimer_ >= staggerDuration_)
			{
				BeginIdleState();
			}
			break;
		}

	case BossState::Down:
	case BossState::PhaseTransition:
	case BossState::Dead:
	default:
		{
			break;
		}
	}
}

/// -------------------------------------------------------------
/// 移動更新
/// Move状態のときだけ前進する
/// -------------------------------------------------------------
void GuardianBoss::UpdateMovement(float deltaTime)
{
	if (GetState() != BossState::Move)
	{
		return;
	}

	// 攻撃直後のクールタイム中は絶対に動かない
	if (attackCooldownTimer_ > 0.0f)
	{
		return;
	}

	Vector3 toTarget
	{
		GetTargetPosition().x - GetPosition().x,
		0.0f,
		GetTargetPosition().z - GetPosition().z
	};

	const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lenSq <= 0.0001f)
	{
		return;
	}

	const float len = std::sqrt(lenSq);
	toTarget.x /= len;
	toTarget.z /= len;

	Vector3 newPos = GetPosition();
	newPos.x += toTarget.x * moveSpeed_ * deltaTime;
	newPos.z += toTarget.z * moveSpeed_ * deltaTime;
	SetPosition(newPos);
}

/// -------------------------------------------------------------
/// 攻撃更新
/// 実際の攻撃処理は基底側 + AttackComponent に任せる
/// 見た目アニメは AnimationComponent 側で更新される
/// -------------------------------------------------------------
void GuardianBoss::UpdateAttack(float deltaTime)
{
	BossBase::UpdateAttack(deltaTime);
	(void)deltaTime;
}

/// -------------------------------------------------------------
/// 死亡チェック
/// -------------------------------------------------------------
void GuardianBoss::CheckDeath()
{
	if (IsDead() && GetState() != BossState::Dead)
	{
		OnDead();
	}
}

/// -------------------------------------------------------------
/// 攻撃登録
/// -------------------------------------------------------------
void GuardianBoss::SetupAttacks()
{
	RegisterAttack(std::make_unique<BossPunchAttack>());
	RegisterAttack(std::make_unique<BossHeavyPunchAttack>());
	RegisterAttack(std::make_unique<GuardianShockwaveAttack>()); // 中距離にも圧をかけるGuardian専用衝撃波を登録する。
	RegisterAttack(std::make_unique<BossChargeAttack>()); // 遠距離では開始方向固定の突進で距離を詰める。
}

/// -------------------------------------------------------------
/// フェーズ設定
/// 今は空でOK
/// -------------------------------------------------------------
void GuardianBoss::SetupPhaseData()
{
}

/// -------------------------------------------------------------
/// 弱点設定
/// 今は空でOK
/// -------------------------------------------------------------
void GuardianBoss::SetupWeakPoints()
{
}

/// -------------------------------------------------------------
/// ターゲット方向へ向く
/// -------------------------------------------------------------
void GuardianBoss::FaceTarget(float deltaTime)
{
	Vector3 toTarget
	{
		GetTargetPosition().x - GetPosition().x,
		0.0f,
		GetTargetPosition().z - GetPosition().z
	};

	const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lenSq <= 0.0001f)
	{
		return;
	}

	const float desiredYaw = std::atan2(toTarget.x, toTarget.z); // forward(sinYaw, cosYaw)と同じワールド座標系でターゲットへ向ける。
	float currentYaw = GetYaw();

	float diff = WrapAngle(desiredYaw - currentYaw);
	const float maxStep = rotateSpeed_ * deltaTime;
	diff = std::clamp(diff, -maxStep, maxStep);

	currentYaw += diff;
	SetYaw(currentYaw);
}

/// -------------------------------------------------------------
/// ターゲットまでのXZ距離
/// -------------------------------------------------------------
float GuardianBoss::GetDistanceToTargetXZ() const
{
	const Vector3 pos = GetPosition();
	const Vector3 target = GetTargetPosition();

	const float dx = target.x - pos.x;
	const float dz = target.z - pos.z;
	return std::sqrt(dx * dx + dz * dz);
}

/// -------------------------------------------------------------
/// 状態変更ヘルパー
/// StateMachine と BossBase::state_ のズレを防ぐため、
/// 状態変更は必ずここを通す
/// -------------------------------------------------------------
void GuardianBoss::ChangeBossState(BossState newState)
{
	if (GetStateMachine())
	{
		GetStateMachine()->ChangeState(*this, newState);
	}
	else
	{
		// 念のためフォールバック
		SetState(newState);
	}
}

/// -------------------------------------------------------------
/// Attack 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginAttackState()
{
	ChangeBossState(BossState::Attack);
	stateTimer_ = 0.0f;
	hasAppliedAttackHit_ = false;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetAttackTimer();
	}

	// ---------------------------------------------------------
	// 手動デバッグ中はここで自動開始しない
	// ImGuiから手動で開始させる
	// ---------------------------------------------------------
	if (!useManualAttackDebug_)
	{
		TryStartBestAttack();
	}
}

/// -------------------------------------------------------------
/// Move 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginMoveState()
{
	ChangeBossState(BossState::Move);
	stateTimer_ = 0.0f;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetWalkTimer();
	}
}

/// -------------------------------------------------------------
/// Idle 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginIdleState()
{
	ChangeBossState(BossState::Idle);
	stateTimer_ = 0.0f;

	if (GetAnimationComponent())
	{
		// 歩行アニメの残りを消す
		GetAnimationComponent()->ResetWalkTimer();

		// 姿勢を自然に戻す
		GetAnimationComponent()->ResetAllPose(0.18f);
	}
}

/// -------------------------------------------------------------
/// Stagger 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginStaggerState()
{
	ChangeBossState(BossState::Stagger);
	stateTimer_ = 0.0f;
}

/// -------------------------------------------------------------
/// 攻撃ヒットタイミング
/// 今は将来拡張用に残す
/// 現段階では BossPunchAttack 側に判定を寄せる方針
/// -------------------------------------------------------------
void GuardianBoss::TryAttackHit()
{
	// 今は未使用
}

void GuardianBoss::ApplyAttackHitParametersToAttacks()
{
	if (!GetAttackComponent())
	{
		return;
	}

	// Guardianの攻撃開始距離と実ヒット判定値を分けて、登録済みPunch系へ即時反映する。
	if (auto* punch = dynamic_cast<BossPunchAttack*>(GetAttackComponent()->FindAttackByName("Punch")))
	{
		punch->SetValidRange(0.0f, closeAttackRange_);
		punch->SetHitParameters(attackHitRange_, attackHitRadius_, attackForwardOffset_, attackHitAngleDeg_);
		punch->SetImpactParticleParameters(particleSpawnCount_, particleSpawnRadius_, particleLifetime_, particleInitialSpeed_);
	}

	if (auto* heavy = dynamic_cast<BossHeavyPunchAttack*>(GetAttackComponent()->FindAttackByName("HeavyPunch")))
	{
		heavy->SetValidRange(0.0f, closeAttackRange_);
		heavy->SetHitParameters(attackHitRange_, attackHitRadius_, attackForwardOffset_, attackHitAngleDeg_);
		heavy->SetImpactParticleParameters(particleSpawnCount_ + particleSpawnCount_ / 2, particleSpawnRadius_ * 1.2f, particleLifetime_, particleInitialSpeed_ * 1.1f);
	}

	if (auto* shockwave = dynamic_cast<GuardianShockwaveAttack*>(GetAttackComponent()->FindAttackByName("GuardianShockwave")))
	{
		shockwaveStartRange_ = middleAttackRange_; // 開始条件はAI用、実ヒット範囲はSetShockwaveParameters側で別管理する。
		shockwave->SetValidRange(closeAttackRange_, middleAttackRange_);
		shockwave->SetShockwaveParameters(shockwaveRange_, shockwaveAngleDeg_, shockwaveDamage_);
		shockwave->SetTimingParameters(shockwaveStartupSec_, shockwaveActiveSec_, shockwaveRecoverySec_, shockwaveCooldown_);
		shockwave->SetImpactParticleParameters(particleSpawnCount_ * 2, std::max(particleSpawnRadius_, 0.8f), particleLifetime_, particleInitialSpeed_);
	}

	if (auto* charge = dynamic_cast<BossChargeAttack*>(GetAttackComponent()->FindAttackByName("ChargeAttack")))
	{
		charge->SetValidRange(middleAttackRange_, farAttackRange_);
		charge->SetChargeParameters(chargeSpeed_, chargeDistance_, chargeDamage_, chargeStartupSec_, chargeRecoverySec_, chargeCooldown_);
		charge->SetImpactParticleParameters(particleSpawnCount_ * 2, std::max(particleSpawnRadius_, 0.75f), particleLifetime_, particleInitialSpeed_ * 1.2f);
	}
}

bool GuardianBoss::TryStartBestAttack()
{
	// AttackComponent が無いなら何もできない
	if (!GetAttackComponent())
	{
		lastSelectedAttack_ = "None";
		return false;
	}

	// すでに攻撃中なら新規開始しない
	if (GetAttackComponent()->IsAttacking())
	{
		return false;
	}

	// Brain が無いなら判断できない
	if (!GetBrain())
	{
		lastSelectedAttack_ = "None";
		return false;
	}

	// ---------------------------------------------------------
	// 攻撃選択は Brain に任せる
	// ---------------------------------------------------------
	const std::string selectedAttack = GetBrain()->SelectBestAttackName();
	if (selectedAttack.empty())
	{
		lastSelectedAttack_ = "None";
		return false;
	}

	// ---------------------------------------------------------
	// 実際の開始は Guardian 側で安全に行う
	// ここを通すことで Attack アニメ時間もリセットできる
	// ---------------------------------------------------------
	if (!StartAttackByNameSafe(selectedAttack.c_str()))
	{
		lastSelectedAttack_ = "None";
		return false;
	}

	lastSelectedAttack_ = selectedAttack;

	// HeavyPunch だけ軽い再使用待ちを残す
	if (selectedAttack == "HeavyPunch")
	{
		heavyPunchReuseTimer_ = heavyPunchReuseDelay_;
	}

	return true;
}

bool GuardianBoss::StartAttackByNameSafe(const char* attackName)
{
	if (!GetAttackComponent())
	{
		return false;
	}

	// すでに攻撃中なら新規開始しない
	if (GetAttackComponent()->IsAttacking())
	{
		return false;
	}

	// 指定名の攻撃を開始
	if (!GetAttackComponent()->StartAttackByName(attackName))
	{
		return false;
	}

	// 攻撃アニメ時間をリセット
	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetAttackTimer();
	}

	return true;
}

/// -------------------------------------------------------------
/// ImGui
/// -------------------------------------------------------------
void GuardianBoss::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("GuardianBoss");

	// ---------------------------------------------------------
	// 基本状態
	// ---------------------------------------------------------
	ImGui::Text("State: %d", static_cast<int>(GetState()));
	ImGui::Text("HP: %.1f / %.1f", GetHP(), GetMaxHP());
	ImGui::Text("DistanceToTargetXZ: %.2f", GetDistanceToTargetXZ());

	ImGui::SeparatorText("ボス被弾確認");
	ImGui::Text("ボス出現済み: はい");
	ImGui::Text("ボス生存中: %s", IsAlive() ? "はい" : "いいえ");
	ImGui::Text("ボスHP: %.1f", GetHP());
	ImGui::Text("ボス最大HP: %.1f", GetMaxHP());
	ImGui::Text("ボスHP割合: %.1f%%", GetHPRate() * 100.0f);
	ImGui::Text("近接攻撃ヒット回数: %d", GetMeleeHitCount());
	ImGui::Text("銃弾ヒット回数: %d", bulletHitCount_);
	ImGui::Text("最後にボスへ与えたダメージ: %.1f", lastReceivedDamage_);
	ImGui::Text("ボス攻撃ヒット回数: %d", bossAttackHitCount_);
	ImGui::Text("最後にプレイヤーが受けたボスダメージ: %.1f", lastPlayerDamage_);

	ImGui::Separator();
	ImGui::Text("StateTimer      : %.2f", stateTimer_);
	ImGui::Text("AttackCooldown  : %.2f", attackCooldownTimer_);
	ImGui::Text("IsCoolingDown   : %s", (attackCooldownTimer_ > 0.0f) ? "true" : "false");

	// ---------------------------------------------------------
	// 調整パラメータ
	// ---------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Tuning");

	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.01f, 0.1f, 20.0f);
	ImGui::DragFloat("Rotate Speed", &rotateSpeed_, 0.01f, 0.1f, 20.0f);
	ImGui::DragFloat("Move Start Dist", &moveStartDistance_, 0.01f, 0.1f, 50.0f);
	ImGui::DragFloat("Move Stop Dist", &moveStopDistance_, 0.01f, 0.1f, 50.0f);
	bool attackHitTuningChanged = false;
	attackHitTuningChanged |= ImGui::DragFloat("Attack Range", &attackRange_, 0.01f, 0.1f, 20.0f);
	attackHitTuningChanged |= ImGui::DragFloat("攻撃判定リーチ", &attackHitRange_, 0.01f, 0.0f, 30.0f);
	attackHitTuningChanged |= ImGui::DragFloat("攻撃判定半径", &attackHitRadius_, 0.01f, 0.0f, 20.0f);
	attackHitTuningChanged |= ImGui::DragFloat("攻撃判定前方オフセット", &attackForwardOffset_, 0.01f, 0.0f, 30.0f);
	attackHitTuningChanged |= ImGui::DragFloat("攻撃判定角度", &attackHitAngleDeg_, 0.1f, 0.0f, 360.0f);
	if (attackHitTuningChanged)
	{
		ApplyAttackHitParametersToAttacks(); // GuardianBossデバッグUIで直接変えた値も現在の攻撃判定へ即時反映する。
	}
	ImGui::DragFloat("Attack Duration", &attackDuration_, 0.01f, 0.05f, 10.0f);
	ImGui::DragFloat("Attack Cooldown", &attackCooldown_, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Stagger Duration", &staggerDuration_, 0.01f, 0.0f, 10.0f);

	// ---------------------------------------------------------
	// Guardian 専用補助情報
	// ---------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Guardian Attack Context");

	ImGui::Text("LastSelectedAttack : %s", lastSelectedAttack_.c_str());
	ImGui::Text("HeavyReuseTimer    : %.2f", heavyPunchReuseTimer_);

	// ---------------------------------------------------------
	// Brain の判断確認
	// BossBase に brain_ / GetBrain() を追加した前提
	// ---------------------------------------------------------
	if (GetBrain())
	{
		ImGui::Separator();
		ImGui::Text("BossBrain Debug");

		ImGui::Text("BrainBestAttack : %s", GetBrain()->GetLastBestAttackName().c_str());
		ImGui::Text("BrainBestScore  : %.2f", GetBrain()->GetLastBestScore());
	}
	else
	{
		ImGui::Separator();
		ImGui::Text("BossBrain Debug");
		ImGui::Text("Brain : None");
	}

	// ---------------------------------------------------------
	// 手動攻撃デバッグ
	// useManualAttackDebug_ が true の間は、
	// BeginAttackState / UpdateState 側でも AI 自動選択を止める前提
	// ---------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Guardian Manual Attack Debug");

	ImGui::Checkbox("Use Manual Attack Debug", &useManualAttackDebug_);

	const char* attackItems[] =
	{
			"Punch",
			"HeavyPunch",
			"GuardianShockwave",
			"ChargeAttack"
	};
	ImGui::Combo("Manual Attack", &manualAttackIndex_, attackItems, IM_ARRAYSIZE(attackItems));

	if (GetAttackComponent())
	{
		const bool isAttackState = (GetState() == BossState::Attack);
		const bool isAlreadyAttacking = GetAttackComponent()->IsAttacking();
		const bool canManualTrigger = isAttackState && !isAlreadyAttacking;

		ImGui::Text("ManualTriggerReady : %s", canManualTrigger ? "true" : "false");

		if (!isAttackState)
		{
			ImGui::Text("Note: Manual start is enabled only in Attack state.");
		}

		if (isAlreadyAttacking)
		{
			ImGui::Text("Note: Current attack is running.");
		}

		if (!canManualTrigger)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Start Selected Attack"))
		{
			if (manualAttackIndex_ == 0)
			{
				if (StartAttackByNameSafe("Punch"))
				{
					lastSelectedAttack_ = "Punch";
				}
			}
			else if (manualAttackIndex_ == 1)
			{
				if (StartAttackByNameSafe("HeavyPunch"))
				{
					lastSelectedAttack_ = "HeavyPunch";
					heavyPunchReuseTimer_ = heavyPunchReuseDelay_;
				}
			}
			else if (manualAttackIndex_ == 2)
			{
				if (StartAttackByNameSafe("GuardianShockwave"))
				{
					lastSelectedAttack_ = "GuardianShockwave";
				}
			}
			else if (manualAttackIndex_ == 3)
			{
				if (StartAttackByNameSafe("ChargeAttack"))
				{
					lastSelectedAttack_ = "ChargeAttack";
				}
			}
		}

		if (!canManualTrigger)
		{
			ImGui::EndDisabled();
		}

		// -----------------------------------------------------
		// Idle / Move からでもテストしやすくする
		// -----------------------------------------------------
		if (GetState() != BossState::Attack)
		{
			if (ImGui::Button("Force Enter Attack State"))
			{
				BeginAttackState();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Force Idle State"))
		{
			BeginIdleState();
		}
	}

	// ---------------------------------------------------------
	// Guardian 側の攻撃選択確認
	// priority / CanStart / 各種条件を見える化
	// ---------------------------------------------------------
	if (GetAttackComponent())
	{
		ImGui::Separator();
		ImGui::Text("Guardian Attack Selection Debug");

		IBossAttack* punch = GetAttackComponent()->FindAttackByName("Punch");
		IBossAttack* heavy = GetAttackComponent()->FindAttackByName("HeavyPunch");
		IBossAttack* shockwave = GetAttackComponent()->FindAttackByName("GuardianShockwave");

		ImGui::Text("LastSelectedAttack : %s", lastSelectedAttack_.c_str());
		ImGui::Text("HeavyReuseTimer    : %.2f", heavyPunchReuseTimer_);
		ImGui::Text("DistanceToTargetXZ : %.2f", GetDistanceToTargetXZ());
		ImGui::Text("AttackHitRange    : %.2f", attackHitRange_);
		ImGui::Text("AttackHitRadius   : %.2f", attackHitRadius_);
		ImGui::Text("AttackForwardOff  : %.2f", attackForwardOffset_);
		ImGui::Text("AttackHitAngleDeg : %.2f", attackHitAngleDeg_);
		ImGui::Text("ShockwaveStart    : %.2f", shockwaveStartRange_);
		ImGui::Text("ShockwaveRange    : %.2f", shockwaveRange_);
		ImGui::Text("ShockwaveAngleDeg : %.2f", shockwaveAngleDeg_);
		ImGui::Text("ShockwaveDamage   : %.2f", shockwaveDamage_);
		ImGui::Text("ShockwaveCooldown : %.2f", shockwaveCooldown_);

		if (punch)
		{
			ImGui::Separator();
			ImGui::Text("[Punch]");
			ImGui::Text("Priority           : %d", punch->GetPriority());
			ImGui::Text("CanStart(Attack)   : %s", punch->CanStart() ? "true" : "false");
			ImGui::Text("CooldownRemaining  : %.2f", punch->GetCooldownRemaining());
			ImGui::Text("Range              : %.2f - %.2f", punch->GetMinRange(), punch->GetMaxRange());
		}
		else
		{
			ImGui::Text("[Punch] Not Registered");
		}

		if (heavy)
		{
			ImGui::Separator();
			ImGui::Text("[HeavyPunch]");
			ImGui::Text("Priority           : %d", heavy->GetPriority());
			ImGui::Text("CanStart(Attack)   : %s", heavy->CanStart() ? "true" : "false");
			ImGui::Text("CooldownRemaining  : %.2f", heavy->GetCooldownRemaining());
			ImGui::Text("Range              : %.2f - %.2f", heavy->GetMinRange(), heavy->GetMaxRange());

			// HeavyPunch が Guardian 側で落ちる理由
			ImGui::Text("HeavyDistanceOK    : %s", (GetDistanceToTargetXZ() <= 2.8f) ? "true" : "false");
			ImGui::Text("HeavyReuseOK       : %s", (heavyPunchReuseTimer_ <= 0.0f) ? "true" : "false");
			ImGui::Text("HeavyLastAttackOK  : %s", (lastSelectedAttack_ != "HeavyPunch") ? "true" : "false");
		}
		else
		{
			ImGui::Text("[HeavyPunch] Not Registered");
		}

		if (shockwave)
		{
			ImGui::Separator();
			ImGui::Text("[GuardianShockwave]");
			ImGui::Text("Priority           : %d", shockwave->GetPriority());
			ImGui::Text("CanStart(Attack)   : %s", shockwave->CanStart() ? "true" : "false");
			ImGui::Text("CooldownRemaining  : %.2f", shockwave->GetCooldownRemaining());
			ImGui::Text("Range              : %.2f - %.2f", shockwave->GetMinRange(), shockwave->GetMaxRange());
		}
		else
		{
			ImGui::Text("[GuardianShockwave] Not Registered");
		}
	}

	// ---------------------------------------------------------
	// 現在攻撃中の詳細
	// Punch / HeavyPunch / Shockwave のフェーズ確認
	// ---------------------------------------------------------
	if (GetAttackComponent())
	{
		ImGui::Separator();
		ImGui::Text("Current Attack Debug");

		ImGui::Text("IsAttacking: %s", GetAttackComponent()->IsAttacking() ? "true" : "false");

		if (IBossAttack* current = GetAttackComponent()->GetCurrentAttack())
		{
			ImGui::Text("CurrentAttack      : %s", current->GetName());
			ImGui::Text("AttackFinished     : %s", current->IsFinished() ? "true" : "false");
			ImGui::Text("CooldownRemaining  : %.2f", current->GetCooldownRemaining());

			if (auto* punch = dynamic_cast<BossPunchAttack*>(current))
			{
				ImGui::Text("PunchPhase         : %d", static_cast<int>(punch->GetPhase()));
				ImGui::Text("PunchPhaseTimer    : %.2f", punch->GetPhaseTimer());
				ImGui::Text("PunchHasHit        : %s", punch->HasHit() ? "true" : "false");
			}

			if (auto* heavy = dynamic_cast<BossHeavyPunchAttack*>(current))
			{
				ImGui::Text("HeavyPhase         : %d", static_cast<int>(heavy->GetPhase()));
				ImGui::Text("HeavyPhaseTimer    : %.2f", heavy->GetPhaseTimer());
				ImGui::Text("HeavyHasHit        : %s", heavy->HasHit() ? "true" : "false");
			}

			if (auto* shockwave = dynamic_cast<GuardianShockwaveAttack*>(current))
			{
				ImGui::Text("ShockwavePhase      : %d", static_cast<int>(shockwave->GetPhase()));
				ImGui::Text("ShockwavePhaseTimer : %.2f", shockwave->GetPhaseTimer());
				ImGui::Text("ShockwaveHasHit     : %s", shockwave->HasHit() ? "true" : "false");
			}
		}
		else
		{
			ImGui::Text("CurrentAttack      : None");
		}
	}

	// ---------------------------------------------------------
	// アニメーション確認
	// ---------------------------------------------------------
	if (GetAnimationComponent())
	{
		ImGui::Separator();
		ImGui::Text("Animation Debug");

		ImGui::Text("WalkAnimTime   : %.2f", GetAnimationComponent()->GetWalkTime());
		ImGui::Text("AttackAnimTime : %.2f", GetAnimationComponent()->GetAttackTime());
	}

	// ---------------------------------------------------------
	// AttackComponent 側詳細
	// 各攻撃の CanStart / Cooldown / Priority を見る
	// ---------------------------------------------------------
	if (GetAttackComponent())
	{
		ImGui::Separator();
		GetAttackComponent()->DrawImGui();
	}

	ImGui::End();
#endif
}

int GuardianBoss::GetMeleeHitCount() const
{
	return std::max(0, receivedHitCount_ - bulletHitCount_);
}
