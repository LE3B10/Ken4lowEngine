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
#include <cctype>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	constexpr const char* kGuardianBossGroup = "GuardianBoss";

	std::string ToLowerExtension(std::string extension)
	{
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return extension;
	}

	std::vector<std::string> CollectAssetFiles(const std::vector<std::filesystem::path>& rootDirectories, const std::vector<std::string>& extensions)
	{
		std::vector<std::string> result;

		for (const std::filesystem::path& rootDirectory : rootDirectories)
		{
			std::error_code errorCode;
			if (!std::filesystem::exists(rootDirectory, errorCode) || !std::filesystem::is_directory(rootDirectory, errorCode))
			{
				continue;
			}

			std::filesystem::recursive_directory_iterator it(rootDirectory, std::filesystem::directory_options::skip_permission_denied, errorCode);
			std::filesystem::recursive_directory_iterator end;
			for (; it != end && !errorCode; it.increment(errorCode))
			{
				if (!it->is_regular_file(errorCode))
				{
					continue;
				}

				const std::string extension = ToLowerExtension(it->path().extension().string());
				if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end())
				{
					continue;
				}

				std::error_code relativeError;
				std::filesystem::path relativePath = std::filesystem::relative(it->path(), rootDirectory, relativeError);
				if (relativeError)
				{
					relativePath = it->path();
				}

				// モデル/テクスチャローダーへ渡す形式に合わせ、Resources配下からの相対パスを候補にする。
				result.push_back(relativePath.generic_string());
			}
		}

		std::sort(result.begin(), result.end());
		result.erase(std::unique(result.begin(), result.end()), result.end());
		return result;
	}

	void AddFallbackOption(std::vector<std::string>& options, const std::string& fallback)
	{
		if (std::find(options.begin(), options.end(), fallback) == options.end())
		{
			options.push_back(fallback);
			std::sort(options.begin(), options.end());
		}
	}

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
		std::vector<std::string> modelOptions = CollectAssetFiles(
			{ "Resources/Models", "Resources/Model" },
			{ ".gltf", ".glb", ".obj" });
		std::vector<std::string> textureOptions = CollectAssetFiles(
			{ "Resources/Textures", "Resources/Texture" },
			{ ".dds", ".png", ".jpg", ".jpeg" });

		AddFallbackOption(modelOptions, "Characters/body.gltf");
		AddFallbackOption(modelOptions, "Characters/head.gltf");
		AddFallbackOption(modelOptions, "Characters/left_arm.gltf");
		AddFallbackOption(modelOptions, "Characters/right_arm.gltf");
		AddFallbackOption(modelOptions, "Characters/left_leg.gltf");
		AddFallbackOption(modelOptions, "Characters/right_leg.gltf");
		AddFallbackOption(textureOptions, "Characters/zombie.dds");

		parameters->AddItem(kGuardianBossGroup, "bodyModelPath", std::string("Characters/body.gltf"));
		parameters->SetStringOptions(kGuardianBossGroup, "bodyModelPath", modelOptions);
		parameters->AddItem(kGuardianBossGroup, "headModelPath", std::string("Characters/head.gltf"));
		parameters->SetStringOptions(kGuardianBossGroup, "headModelPath", modelOptions);
		parameters->AddItem(kGuardianBossGroup, "leftArmModelPath", std::string("Characters/left_arm.gltf"));
		parameters->SetStringOptions(kGuardianBossGroup, "leftArmModelPath", modelOptions);
		parameters->AddItem(kGuardianBossGroup, "rightArmModelPath", std::string("Characters/right_arm.gltf"));
		parameters->SetStringOptions(kGuardianBossGroup, "rightArmModelPath", modelOptions);
		parameters->AddItem(kGuardianBossGroup, "leftLegModelPath", std::string("Characters/left_leg.gltf"));
		parameters->SetStringOptions(kGuardianBossGroup, "leftLegModelPath", modelOptions);
		parameters->AddItem(kGuardianBossGroup, "rightLegModelPath", std::string("Characters/right_leg.gltf"));
		parameters->SetStringOptions(kGuardianBossGroup, "rightLegModelPath", modelOptions);
		parameters->AddItem(kGuardianBossGroup, "skinPath", std::string("Characters/zombie.dds"));
		parameters->SetStringOptions(kGuardianBossGroup, "skinPath", textureOptions);
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
		parameters->SetDisplayName(kGuardianBossGroup, "bodyModelPath", "胴体モデルパス");
		parameters->SetDisplayName(kGuardianBossGroup, "headModelPath", "頭モデルパス");
		parameters->SetDisplayName(kGuardianBossGroup, "leftArmModelPath", "左腕モデルパス");
		parameters->SetDisplayName(kGuardianBossGroup, "rightArmModelPath", "右腕モデルパス");
		parameters->SetDisplayName(kGuardianBossGroup, "leftLegModelPath", "左脚モデルパス");
		parameters->SetDisplayName(kGuardianBossGroup, "rightLegModelPath", "右脚モデルパス");
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
	ApplyVisualParameters();
	movementTuning_.moveSpeed = GetGuardianParameterOrDefault("moveSpeed", movementTuning_.moveSpeed);
	movementTuning_.rotateSpeed = GetGuardianParameterOrDefault("rotateSpeed", movementTuning_.rotateSpeed);

	if (movementComponent_)
	{
		// Guardian固有の移動速度を実際に移動するMovementComponentへ反映する
		GetMovementComponent()->SetMoveSpeed(movementTuning_.moveSpeed);
		GetMovementComponent()->SetTurnSpeed(movementTuning_.rotateSpeed);
	}

	attackHitTuning_.attackRange = GetGuardianParameterOrDefault("attackRange", attackHitTuning_.attackRange);
	attackHitTuning_.hitRange = GetGuardianParameterOrDefault("GuardianAttackHitRange", attackHitTuning_.hitRange);
	attackHitTuning_.hitRadius = GetGuardianParameterOrDefault("GuardianAttackHitRadius", attackHitTuning_.hitRadius);
	attackHitTuning_.forwardOffset = GetGuardianParameterOrDefault("GuardianAttackForwardOffset", attackHitTuning_.forwardOffset);
	attackHitTuning_.hitAngleDeg = GetGuardianParameterOrDefault("GuardianAttackHitAngleDeg", attackHitTuning_.hitAngleDeg);
	shockwaveTuning_.range = GetGuardianParameterOrDefault("GuardianShockwaveRange", shockwaveTuning_.range); // Guardian衝撃波リーチをJSON調整値から復元する
	shockwaveTuning_.angleDeg = GetGuardianParameterOrDefault("GuardianShockwaveAngleDeg", shockwaveTuning_.angleDeg); // Guardian衝撃波角度をJSON調整値から復元する
	shockwaveTuning_.damage = GetGuardianParameterOrDefault("GuardianShockwaveDamage", shockwaveTuning_.damage); // Guardian衝撃波ダメージをJSON調整値から復元する
	shockwaveTuning_.cooldown = GetGuardianParameterOrDefault("GuardianShockwaveCooldown", shockwaveTuning_.cooldown); // Guardian衝撃波クールタイムをJSON調整値から復元する
	shockwaveTuning_.startupSec = GetGuardianParameterOrDefault("GuardianShockwaveStartupSec", shockwaveTuning_.startupSec); // Guardian衝撃波予備動作をJSON調整値から復元する
	shockwaveTuning_.activeSec = GetGuardianParameterOrDefault("GuardianShockwaveActiveSec", shockwaveTuning_.activeSec); // Guardian衝撃波判定時間をJSON調整値から復元する
	shockwaveTuning_.recoverySec = GetGuardianParameterOrDefault("GuardianShockwaveRecoverySec", shockwaveTuning_.recoverySec); // Guardian衝撃波後隙をJSON調整値から復元する
	attackHitTuning_.closeRange = GetGuardianParameterOrDefault("GuardianCloseAttackRange", attackHitTuning_.closeRange);
	attackHitTuning_.middleRange = GetGuardianParameterOrDefault("GuardianMiddleAttackRange", attackHitTuning_.middleRange);
	attackHitTuning_.farRange = GetGuardianParameterOrDefault("GuardianFarAttackRange", attackHitTuning_.farRange);
	chargeTuning_.speed = GetGuardianParameterOrDefault("GuardianChargeSpeed", chargeTuning_.speed);
	chargeTuning_.distance = GetGuardianParameterOrDefault("GuardianChargeDistance", chargeTuning_.distance);
	chargeTuning_.damage = GetGuardianParameterOrDefault("GuardianChargeDamage", chargeTuning_.damage);
	chargeTuning_.startupSec = GetGuardianParameterOrDefault("GuardianChargeStartupSec", chargeTuning_.startupSec);
	chargeTuning_.recoverySec = GetGuardianParameterOrDefault("GuardianChargeRecoverySec", chargeTuning_.recoverySec);
	chargeTuning_.cooldown = GetGuardianParameterOrDefault("GuardianChargeCooldown", chargeTuning_.cooldown);
	attackHitTuning_.middleRange = std::max(attackHitTuning_.closeRange, attackHitTuning_.middleRange); // 距離帯が逆転した設定でも近距離→中距離→遠距離の順を保つ。
	attackHitTuning_.farRange = std::max(attackHitTuning_.middleRange, attackHitTuning_.farRange);
	particleTuning_.spawnCount = static_cast<uint32_t>(std::max(0, GetGuardianParameterOrDefault("ParticleSpawnCount", static_cast<int>(particleTuning_.spawnCount))));
	particleTuning_.spawnRadius = GetGuardianParameterOrDefault("ParticleSpawnRadius", particleTuning_.spawnRadius);
	particleTuning_.lifetime = GetGuardianParameterOrDefault("ParticleLifetime", particleTuning_.lifetime);
	particleTuning_.initialSpeed = GetGuardianParameterOrDefault("ParticleInitialSpeed", particleTuning_.initialSpeed);
	movementTuning_.moveStartDistance = GetGuardianParameterOrDefault("moveStartDistance", movementTuning_.moveStartDistance);
	movementTuning_.moveStopDistance = GetGuardianParameterOrDefault("moveStopDistance", movementTuning_.moveStopDistance);
	animationTuning_.attackDuration = GetGuardianParameterOrDefault("attackDuration", animationTuning_.attackDuration);
	animationTuning_.attackCooldown = GetGuardianParameterOrDefault("attackCooldown", animationTuning_.attackCooldown);
	animationTuning_.staggerDuration = GetGuardianParameterOrDefault("staggerDuration", animationTuning_.staggerDuration);
	attackSelectState_.heavyPunchReuseDelay = GetGuardianParameterOrDefault("heavyPunchReuseDelay", attackSelectState_.heavyPunchReuseDelay);
	animationTuning_.walkSpeed = GetGuardianParameterOrDefault("animationWalkSpeed", animationTuning_.walkSpeed);
	animationTuning_.walkAmplitude = GetGuardianParameterOrDefault("animationWalkAmplitude", animationTuning_.walkAmplitude);
	ApplyVisualParameters();
	if (GetAnimationComponent())
	{
		GetAnimationComponent()->SetWalkSpeed(animationTuning_.walkSpeed);
		GetAnimationComponent()->SetWalkAmplitude(animationTuning_.walkAmplitude);
		GetAnimationComponent()->SetAttackDuration(animationTuning_.attackDuration);
	}
	ApplyAttackHitParametersToAttacks(); // 攻撃判定リーチ変更は保存/反映後に実行中の攻撃処理へ再適用する。
	ApplySkinToAllParts(GetGuardianSkinPath()); // スキンパス変更は保存/反映後に実行中モデルへ再適用する。
}

void GuardianBoss::ApplyVisualParameters()
{
	visualTuning_.bodyModelPath = GetGuardianParameterOrDefault("bodyModelPath", visualTuning_.bodyModelPath);
	visualTuning_.headModelPath = GetGuardianParameterOrDefault("headModelPath", visualTuning_.headModelPath);
	visualTuning_.leftArmModelPath = GetGuardianParameterOrDefault("leftArmModelPath", visualTuning_.leftArmModelPath);
	visualTuning_.rightArmModelPath = GetGuardianParameterOrDefault("rightArmModelPath", visualTuning_.rightArmModelPath);
	visualTuning_.leftLegModelPath = GetGuardianParameterOrDefault("leftLegModelPath", visualTuning_.leftLegModelPath);
	visualTuning_.rightLegModelPath = GetGuardianParameterOrDefault("rightLegModelPath", visualTuning_.rightLegModelPath);
	visualTuning_.skinPath = GetGuardianParameterOrDefault("skinPath", visualTuning_.skinPath);
}

std::string GuardianBoss::GetBodyModelPath() const
{
	EnsureGuardianBossParameters();
	return GetGuardianParameterOrDefault("bodyModelPath", visualTuning_.bodyModelPath);
}

std::string GuardianBoss::GetHeadModelPath() const
{
	EnsureGuardianBossParameters();
	return GetGuardianParameterOrDefault("headModelPath", visualTuning_.headModelPath);
}

std::string GuardianBoss::GetLeftArmModelPath() const
{
	EnsureGuardianBossParameters();
	return GetGuardianParameterOrDefault("leftArmModelPath", visualTuning_.leftArmModelPath);
}

std::string GuardianBoss::GetRightArmModelPath() const
{
	EnsureGuardianBossParameters();
	return GetGuardianParameterOrDefault("rightArmModelPath", visualTuning_.rightArmModelPath);
}

std::string GuardianBoss::GetLeftLegModelPath() const
{
	EnsureGuardianBossParameters();
	return GetGuardianParameterOrDefault("leftLegModelPath", visualTuning_.leftLegModelPath);
}

std::string GuardianBoss::GetRightLegModelPath() const
{
	EnsureGuardianBossParameters();
	return GetGuardianParameterOrDefault("rightLegModelPath", visualTuning_.rightLegModelPath);
}


std::string GuardianBoss::GetGuardianSkinPath() const
{
	EnsureGuardianBossParameters();
	return GetGuardianParameterOrDefault("skinPath", visualTuning_.skinPath); // スキンもJSONから参照し、失敗時は既定テクスチャへ戻す
}

/// -------------------------------------------------------------
/// Guardian 固有初期化
/// -------------------------------------------------------------
void GuardianBoss::SetupBoss()
{
	// 人型ボス共通初期化
	HumanoidBossBase::SetupBoss();

	EnsureGuardianBossParameters();
	ApplyVisualParameters();
	movementTuning_.moveSpeed = GetGuardianParameterOrDefault("moveSpeed", movementTuning_.moveSpeed); // Guardianの移動速度をJSON調整値から復元する
	movementTuning_.rotateSpeed = GetGuardianParameterOrDefault("rotateSpeed", movementTuning_.rotateSpeed); // Guardianの旋回速度をJSON調整値から復元する
	attackHitTuning_.attackRange = GetGuardianParameterOrDefault("attackRange", attackHitTuning_.attackRange); // Guardianの攻撃開始距離をJSON調整値から復元する
	attackHitTuning_.hitRange = GetGuardianParameterOrDefault("GuardianAttackHitRange", attackHitTuning_.hitRange); // Guardianの攻撃判定リーチをJSON調整値から復元する
	attackHitTuning_.hitRadius = GetGuardianParameterOrDefault("GuardianAttackHitRadius", attackHitTuning_.hitRadius); // Guardianの攻撃判定半径をJSON調整値から復元する
	attackHitTuning_.forwardOffset = GetGuardianParameterOrDefault("GuardianAttackForwardOffset", attackHitTuning_.forwardOffset); // Guardianの攻撃判定前方オフセットをJSON調整値から復元する
	attackHitTuning_.hitAngleDeg = GetGuardianParameterOrDefault("GuardianAttackHitAngleDeg", attackHitTuning_.hitAngleDeg); // Guardianの攻撃判定角度をJSON調整値から復元する
	shockwaveTuning_.range = GetGuardianParameterOrDefault("GuardianShockwaveRange", shockwaveTuning_.range); // Guardian衝撃波リーチをJSON調整値から復元する
	shockwaveTuning_.angleDeg = GetGuardianParameterOrDefault("GuardianShockwaveAngleDeg", shockwaveTuning_.angleDeg); // Guardian衝撃波角度をJSON調整値から復元する
	shockwaveTuning_.damage = GetGuardianParameterOrDefault("GuardianShockwaveDamage", shockwaveTuning_.damage); // Guardian衝撃波ダメージをJSON調整値から復元する
	shockwaveTuning_.cooldown = GetGuardianParameterOrDefault("GuardianShockwaveCooldown", shockwaveTuning_.cooldown); // Guardian衝撃波クールタイムをJSON調整値から復元する
	shockwaveTuning_.startupSec = GetGuardianParameterOrDefault("GuardianShockwaveStartupSec", shockwaveTuning_.startupSec); // Guardian衝撃波予備動作をJSON調整値から復元する
	shockwaveTuning_.activeSec = GetGuardianParameterOrDefault("GuardianShockwaveActiveSec", shockwaveTuning_.activeSec); // Guardian衝撃波判定時間をJSON調整値から復元する
	shockwaveTuning_.recoverySec = GetGuardianParameterOrDefault("GuardianShockwaveRecoverySec", shockwaveTuning_.recoverySec); // Guardian衝撃波後隙をJSON調整値から復元する
	attackHitTuning_.closeRange = GetGuardianParameterOrDefault("GuardianCloseAttackRange", attackHitTuning_.closeRange);
	attackHitTuning_.middleRange = GetGuardianParameterOrDefault("GuardianMiddleAttackRange", attackHitTuning_.middleRange);
	attackHitTuning_.farRange = GetGuardianParameterOrDefault("GuardianFarAttackRange", attackHitTuning_.farRange);
	chargeTuning_.speed = GetGuardianParameterOrDefault("GuardianChargeSpeed", chargeTuning_.speed);
	chargeTuning_.distance = GetGuardianParameterOrDefault("GuardianChargeDistance", chargeTuning_.distance);
	chargeTuning_.damage = GetGuardianParameterOrDefault("GuardianChargeDamage", chargeTuning_.damage);
	chargeTuning_.startupSec = GetGuardianParameterOrDefault("GuardianChargeStartupSec", chargeTuning_.startupSec);
	chargeTuning_.recoverySec = GetGuardianParameterOrDefault("GuardianChargeRecoverySec", chargeTuning_.recoverySec);
	chargeTuning_.cooldown = GetGuardianParameterOrDefault("GuardianChargeCooldown", chargeTuning_.cooldown);
	attackHitTuning_.middleRange = std::max(attackHitTuning_.closeRange, attackHitTuning_.middleRange); // 距離帯が逆転した設定でも近距離→中距離→遠距離の順を保つ。
	attackHitTuning_.farRange = std::max(attackHitTuning_.middleRange, attackHitTuning_.farRange);
	particleTuning_.spawnCount = static_cast<uint32_t>(std::max(0, GetGuardianParameterOrDefault("ParticleSpawnCount", static_cast<int>(particleTuning_.spawnCount))));
	particleTuning_.spawnRadius = GetGuardianParameterOrDefault("ParticleSpawnRadius", particleTuning_.spawnRadius);
	particleTuning_.lifetime = GetGuardianParameterOrDefault("ParticleLifetime", particleTuning_.lifetime);
	particleTuning_.initialSpeed = GetGuardianParameterOrDefault("ParticleInitialSpeed", particleTuning_.initialSpeed);
	movementTuning_.moveStartDistance = GetGuardianParameterOrDefault("moveStartDistance", movementTuning_.moveStartDistance); // Guardianの移動開始距離をJSON調整値から復元する
	movementTuning_.moveStopDistance = GetGuardianParameterOrDefault("moveStopDistance", movementTuning_.moveStopDistance); // Guardianの移動停止距離をJSON調整値から復元する
	animationTuning_.attackDuration = GetGuardianParameterOrDefault("attackDuration", animationTuning_.attackDuration); // Guardianの攻撃時間をJSON調整値から復元する
	animationTuning_.attackCooldown = GetGuardianParameterOrDefault("attackCooldown", animationTuning_.attackCooldown); // Guardianの攻撃後クールタイムをJSON調整値から復元する
	animationTuning_.staggerDuration = GetGuardianParameterOrDefault("staggerDuration", animationTuning_.staggerDuration); // Guardianのひるみ時間をJSON調整値から復元する
	attackSelectState_.heavyPunchReuseDelay = GetGuardianParameterOrDefault("heavyPunchReuseDelay", attackSelectState_.heavyPunchReuseDelay); // HeavyPunch再使用間隔をJSON調整値から復元する
	animationTuning_.walkSpeed = GetGuardianParameterOrDefault("animationWalkSpeed", animationTuning_.walkSpeed); // 歩行アニメ速度をJSON調整値から復元する
	animationTuning_.walkAmplitude = GetGuardianParameterOrDefault("animationWalkAmplitude", animationTuning_.walkAmplitude); // 歩行アニメ振幅をJSON調整値から復元する

	// フェーズ初期化
	SetPhase(BossPhase::Phase1);

	runtimeState_.stateTimer = 0.0f;
	runtimeState_.attackCooldownTimer = 0.0f;
	runtimeState_.hasAppliedAttackHit = false;
	runtimeState_.receivedHitCount = 0;
	runtimeState_.bulletHitCount = 0;
	runtimeState_.bossAttackHitCount = 0;
	runtimeState_.lastReceivedDamage = 0.0f;
	runtimeState_.lastPlayerDamage = 0.0f;

	// HeavyPunch 連打抑制初期化
	attackSelectState_.lastSelectedAttack = "None";
	attackSelectState_.heavyPunchReuseTimer = 0.0f;

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
		GetAnimationComponent()->SetWalkSpeed(animationTuning_.walkSpeed);
		GetAnimationComponent()->SetWalkAmplitude(animationTuning_.walkAmplitude);
		GetAnimationComponent()->SetAttackDuration(animationTuning_.attackDuration);

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
		++runtimeState_.receivedHitCount;
		runtimeState_.lastReceivedDamage = damage;
	}

	{
		std::ostringstream oss;
		oss << "[GuardianBoss] HP=" << GetHP() << "/" << GetMaxHP()
			<< ", hitCount=" << runtimeState_.receivedHitCount
			<< ", lastDamage=" << runtimeState_.lastReceivedDamage;
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
	runtimeState_.stateTimer = 0.0f;

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
	++runtimeState_.bulletHitCount;
	OnDamaged(damage);

	std::ostringstream oss;
	oss << "[GuardianBoss] Player bullet hit: damage=" << damage
		<< ", bulletHitCount=" << runtimeState_.bulletHitCount
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
	++runtimeState_.bossAttackHitCount;
	runtimeState_.lastPlayerDamage = damage;

	std::ostringstream oss;
	oss << "[GuardianBoss] Boss attack damaged player: damage=" << damage
		<< ", hitCount=" << runtimeState_.bossAttackHitCount;
	Log(oss.str() + "\n");
}

/// -------------------------------------------------------------
/// 状態更新
/// Guardian の思考をここで決める
/// -------------------------------------------------------------
void GuardianBoss::UpdateState(float deltaTime)
{
	runtimeState_.stateTimer += deltaTime;
	runtimeState_.attackCooldownTimer = std::max(0.0f, runtimeState_.attackCooldownTimer - deltaTime);

	// ---------------------------------------------------------
	// HeavyPunch の連打抑制タイマー
	// ---------------------------------------------------------
	attackSelectState_.heavyPunchReuseTimer = std::max(0.0f, attackSelectState_.heavyPunchReuseTimer - deltaTime);

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
			if (runtimeState_.attackCooldownTimer > 0.0f)
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
			else if (distance > movementTuning_.moveStartDistance)
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
			if (runtimeState_.attackCooldownTimer <= 0.0f && hasStartableAttack)
			{
				BeginAttackState();
			}
			// 十分近づいたら待機
			else if (distance <= movementTuning_.moveStopDistance)
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
			if (!attackSelectState_.useManualAttackDebug)
			{
				if (GetAttackComponent() && !GetAttackComponent()->IsAttacking())
				{
					if (runtimeState_.stateTimer <= 0.10f)
					{
						TryStartBestAttack();
					}
				}
			}

			// ---------------------------------------------------------
			// 少し待ってから攻撃終了判定
			// 手動デバッグ中でも、攻撃が終わったら Idle に戻す
			// ---------------------------------------------------------
			if (runtimeState_.stateTimer >= 0.05f)
			{
				if (GetAttackComponent() && !GetAttackComponent()->IsAttacking())
				{
					runtimeState_.attackCooldownTimer = animationTuning_.attackCooldown;
					runtimeState_.hasAppliedAttackHit = false;
					BeginIdleState();
				}
			}
			break;
		}

	case BossState::Stagger:
		{
			if (runtimeState_.stateTimer >= animationTuning_.staggerDuration)
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
	if (runtimeState_.attackCooldownTimer > 0.0f)
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
	newPos.x += toTarget.x * movementTuning_.moveSpeed * deltaTime;
	newPos.z += toTarget.z * movementTuning_.moveSpeed * deltaTime;
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

	const float desiredYaw = std::atan2(-toTarget.x, toTarget.z); // forward(sinYaw, cosYaw)と同じワールド座標系でターゲットへ向ける。
	float currentYaw = GetYaw();

	float diff = WrapAngle(desiredYaw - currentYaw);
	const float maxStep = movementTuning_.rotateSpeed * deltaTime;
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
	runtimeState_.stateTimer = 0.0f;
	runtimeState_.hasAppliedAttackHit = false;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetAttackTimer();
	}

	// ---------------------------------------------------------
	// 手動デバッグ中はここで自動開始しない
	// ImGuiから手動で開始させる
	// ---------------------------------------------------------
	if (!attackSelectState_.useManualAttackDebug)
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
	runtimeState_.stateTimer = 0.0f;

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
	runtimeState_.stateTimer = 0.0f;

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
	runtimeState_.stateTimer = 0.0f;
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
		punch->SetValidRange(0.0f, attackHitTuning_.closeRange);
		punch->SetHitParameters(attackHitTuning_.hitRange, attackHitTuning_.hitRadius, attackHitTuning_.forwardOffset, attackHitTuning_.hitAngleDeg);
		punch->SetImpactParticleParameters(particleTuning_.spawnCount, particleTuning_.spawnRadius, particleTuning_.lifetime, particleTuning_.initialSpeed);
	}

	if (auto* heavy = dynamic_cast<BossHeavyPunchAttack*>(GetAttackComponent()->FindAttackByName("HeavyPunch")))
	{
		heavy->SetValidRange(0.0f, attackHitTuning_.closeRange);
		heavy->SetHitParameters(attackHitTuning_.hitRange, attackHitTuning_.hitRadius, attackHitTuning_.forwardOffset, attackHitTuning_.hitAngleDeg);
		heavy->SetImpactParticleParameters(particleTuning_.spawnCount + particleTuning_.spawnCount / 2, particleTuning_.spawnRadius * 1.2f, particleTuning_.lifetime, particleTuning_.initialSpeed * 1.1f);
	}

	if (auto* shockwave = dynamic_cast<GuardianShockwaveAttack*>(GetAttackComponent()->FindAttackByName("GuardianShockwave")))
	{
		shockwaveTuning_.startRange = attackHitTuning_.middleRange; // 開始条件はAI用、実ヒット範囲はSetShockwaveParameters側で別管理する。
		shockwave->SetValidRange(attackHitTuning_.closeRange, attackHitTuning_.middleRange);
		shockwave->SetShockwaveParameters(shockwaveTuning_.range, shockwaveTuning_.angleDeg, shockwaveTuning_.damage);
		shockwave->SetTimingParameters(shockwaveTuning_.startupSec, shockwaveTuning_.activeSec, shockwaveTuning_.recoverySec, shockwaveTuning_.cooldown);
		shockwave->SetImpactParticleParameters(particleTuning_.spawnCount * 2, std::max(particleTuning_.spawnRadius, 0.8f), particleTuning_.lifetime, particleTuning_.initialSpeed);
	}

	if (auto* charge = dynamic_cast<BossChargeAttack*>(GetAttackComponent()->FindAttackByName("ChargeAttack")))
	{
		charge->SetValidRange(attackHitTuning_.middleRange, attackHitTuning_.farRange);
		charge->SetChargeParameters(chargeTuning_.speed, chargeTuning_.distance, chargeTuning_.damage, chargeTuning_.startupSec, chargeTuning_.recoverySec, chargeTuning_.cooldown);
		charge->SetImpactParticleParameters(particleTuning_.spawnCount * 2, std::max(particleTuning_.spawnRadius, 0.75f), particleTuning_.lifetime, particleTuning_.initialSpeed * 1.2f);
	}
}

bool GuardianBoss::TryStartBestAttack()
{
	// AttackComponent が無いなら何もできない
	if (!GetAttackComponent())
	{
		attackSelectState_.lastSelectedAttack = "None";
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
		attackSelectState_.lastSelectedAttack = "None";
		return false;
	}

	// ---------------------------------------------------------
	// 攻撃選択は Brain に任せる
	// ---------------------------------------------------------
	const std::string selectedAttack = GetBrain()->SelectBestAttackName();
	if (selectedAttack.empty())
	{
		attackSelectState_.lastSelectedAttack = "None";
		return false;
	}

	// ---------------------------------------------------------
	// 実際の開始は Guardian 側で安全に行う
	// ここを通すことで Attack アニメ時間もリセットできる
	// ---------------------------------------------------------
	if (!StartAttackByNameSafe(selectedAttack.c_str()))
	{
		attackSelectState_.lastSelectedAttack = "None";
		return false;
	}

	attackSelectState_.lastSelectedAttack = selectedAttack;

	// HeavyPunch だけ軽い再使用待ちを残す
	if (selectedAttack == "HeavyPunch")
	{
		attackSelectState_.heavyPunchReuseTimer = attackSelectState_.heavyPunchReuseDelay;
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

	// 攻撃開始直前に最新の調整値を攻撃クラスへ反映する
	ApplyAttackHitParametersToAttacks();

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
	ImGui::Text("銃弾ヒット回数: %d", runtimeState_.bulletHitCount);
	ImGui::Text("最後にボスへ与えたダメージ: %.1f", runtimeState_.lastReceivedDamage);
	ImGui::Text("ボス攻撃ヒット回数: %d", runtimeState_.bossAttackHitCount);
	ImGui::Text("最後にプレイヤーが受けたボスダメージ: %.1f", runtimeState_.lastPlayerDamage);

	ImGui::Separator();
	ImGui::Text("StateTimer      : %.2f", runtimeState_.stateTimer);
	ImGui::Text("AttackCooldown  : %.2f", runtimeState_.attackCooldownTimer);
	ImGui::Text("IsCoolingDown   : %s", (runtimeState_.attackCooldownTimer > 0.0f) ? "true" : "false");

	// ---------------------------------------------------------
	// 調整パラメータ
	// ---------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Tuning");

	bool moveTuningChanged = false;
	moveTuningChanged |= ImGui::DragFloat("Move Speed", &movementTuning_.moveSpeed, 0.01f, 0.1f, 50.0f);
	moveTuningChanged |= ImGui::DragFloat("Rotate Speed", &movementTuning_.rotateSpeed, 0.01f, 0.1f, 30.0f);
	moveTuningChanged |= ImGui::DragFloat("Move Start Dist", &movementTuning_.moveStartDistance, 0.01f, 0.1f, 100.0f);
	moveTuningChanged |= ImGui::DragFloat("Move Stop Dist", &movementTuning_.moveStopDistance, 0.01f, 0.1f, 100.0f);

	if (moveTuningChanged)
	{
		// 移動パラメータ変更を通常移動と攻撃選択の両方へ即時反映する
		if (GetMovementComponent())
		{
			GetMovementComponent()->SetMoveSpeed(movementTuning_.moveSpeed);
			GetMovementComponent()->SetTurnSpeed(movementTuning_.rotateSpeed);
			GetMovementComponent()->SetStopDistance(movementTuning_.moveStopDistance);
		}

		if (movementTuning_.moveStopDistance > movementTuning_.moveStartDistance)
		{
			movementTuning_.moveStopDistance = movementTuning_.moveStartDistance;
		}
	}

	bool chargeTuningChanged = false;
	chargeTuningChanged |= ImGui::DragFloat("突進速度", &chargeTuning_.speed, 0.1f, 0.0f, 100.0f);
	chargeTuningChanged |= ImGui::DragFloat("突進距離", &chargeTuning_.distance, 0.1f, 0.0f, 100.0f);
	chargeTuningChanged |= ImGui::DragFloat("突進ダメージ", &chargeTuning_.damage, 0.1f, 0.0f, 999.0f);
	chargeTuningChanged |= ImGui::DragFloat("突進予備動作", &chargeTuning_.startupSec, 0.01f, 0.0f, 10.0f);
	chargeTuningChanged |= ImGui::DragFloat("突進後隙", &chargeTuning_.recoverySec, 0.01f, 0.0f, 10.0f);
	chargeTuningChanged |= ImGui::DragFloat("突進クールタイム", &chargeTuning_.cooldown, 0.01f, 0.0f, 60.0f);

	if (chargeTuningChanged)
	{
		// デバッグUIで変更した突進値を現在の攻撃クラスへ即時反映する
		ApplyAttackHitParametersToAttacks();
	}

	bool attackHitTuningChanged = false;
	attackHitTuningChanged |= ImGui::DragFloat("Attack Range", &attackHitTuning_.attackRange, 0.01f, 0.1f, 20.0f);
	attackHitTuningChanged |= ImGui::DragFloat("攻撃判定リーチ", &attackHitTuning_.hitRange, 0.01f, 0.0f, 30.0f);
	attackHitTuningChanged |= ImGui::DragFloat("攻撃判定半径", &attackHitTuning_.hitRadius, 0.01f, 0.0f, 20.0f);
	attackHitTuningChanged |= ImGui::DragFloat("攻撃判定前方オフセット", &attackHitTuning_.forwardOffset, 0.01f, 0.0f, 30.0f);
	attackHitTuningChanged |= ImGui::DragFloat("攻撃判定角度", &attackHitTuning_.hitAngleDeg, 0.1f, 0.0f, 360.0f);
	if (attackHitTuningChanged)
	{
		ApplyAttackHitParametersToAttacks(); // GuardianBossデバッグUIで直接変えた値も現在の攻撃判定へ即時反映する。
	}
	ImGui::DragFloat("Attack Duration", &animationTuning_.attackDuration, 0.01f, 0.05f, 10.0f);
	ImGui::DragFloat("Attack Cooldown", &animationTuning_.attackCooldown, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Stagger Duration", &animationTuning_.staggerDuration, 0.01f, 0.0f, 10.0f);

	// ---------------------------------------------------------
	// Guardian 専用補助情報
	// ---------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Guardian Attack Context");

	ImGui::Text("LastSelectedAttack : %s", attackSelectState_.lastSelectedAttack.c_str());
	ImGui::Text("HeavyReuseTimer    : %.2f", attackSelectState_.heavyPunchReuseTimer);

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
	// attackSelectState_.useManualAttackDebug が true の間は、
	// BeginAttackState / UpdateState 側でも AI 自動選択を止める前提
	// ---------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Guardian Manual Attack Debug");

	ImGui::Checkbox("Use Manual Attack Debug", &attackSelectState_.useManualAttackDebug);

	const char* attackItems[] =
	{
			"Punch",
			"HeavyPunch",
			"GuardianShockwave",
			"ChargeAttack"
	};
	ImGui::Combo("Manual Attack", &attackSelectState_.manualAttackIndex, attackItems, IM_ARRAYSIZE(attackItems));

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
			if (attackSelectState_.manualAttackIndex == 0)
			{
				if (StartAttackByNameSafe("Punch"))
				{
					attackSelectState_.lastSelectedAttack = "Punch";
				}
			}
			else if (attackSelectState_.manualAttackIndex == 1)
			{
				if (StartAttackByNameSafe("HeavyPunch"))
				{
					attackSelectState_.lastSelectedAttack = "HeavyPunch";
					attackSelectState_.heavyPunchReuseTimer = attackSelectState_.heavyPunchReuseDelay;
				}
			}
			else if (attackSelectState_.manualAttackIndex == 2)
			{
				if (StartAttackByNameSafe("GuardianShockwave"))
				{
					attackSelectState_.lastSelectedAttack = "GuardianShockwave";
				}
			}
			else if (attackSelectState_.manualAttackIndex == 3)
			{
				if (StartAttackByNameSafe("ChargeAttack"))
				{
					attackSelectState_.lastSelectedAttack = "ChargeAttack";
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

		ImGui::Text("LastSelectedAttack : %s", attackSelectState_.lastSelectedAttack.c_str());
		ImGui::Text("HeavyReuseTimer    : %.2f", attackSelectState_.heavyPunchReuseTimer);
		ImGui::Text("DistanceToTargetXZ : %.2f", GetDistanceToTargetXZ());
		ImGui::Text("AttackHitRange    : %.2f", attackHitTuning_.hitRange);
		ImGui::Text("AttackHitRadius   : %.2f", attackHitTuning_.hitRadius);
		ImGui::Text("AttackForwardOff  : %.2f", attackHitTuning_.forwardOffset);
		ImGui::Text("AttackHitAngleDeg : %.2f", attackHitTuning_.hitAngleDeg);
		ImGui::Text("ShockwaveStart    : %.2f", shockwaveTuning_.startRange);
		ImGui::Text("ShockwaveRange    : %.2f", shockwaveTuning_.range);
		ImGui::Text("ShockwaveAngleDeg : %.2f", shockwaveTuning_.angleDeg);
		ImGui::Text("ShockwaveDamage   : %.2f", shockwaveTuning_.damage);
		ImGui::Text("ShockwaveCooldown : %.2f", shockwaveTuning_.cooldown);

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
			ImGui::Text("HeavyReuseOK       : %s", (attackSelectState_.heavyPunchReuseTimer <= 0.0f) ? "true" : "false");
			ImGui::Text("HeavyLastAttackOK  : %s", (attackSelectState_.lastSelectedAttack != "HeavyPunch") ? "true" : "false");
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
	return std::max(0, runtimeState_.receivedHitCount - runtimeState_.bulletHitCount);
}
