#pragma once

#include <AABB.h>

#include <cstdio>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class ActorWorld;
	class BossActor;
	class CharacterActor;
	class EnemyActor;
}

namespace K4E = ::Ken4lowEngine;

/// DebugSceneへ移行済みEnemyActorを配置し、DebugPlayerの射撃・照準・HP HUDまで実戦形式で検証する。
class EnemyMigrationValidation
{
public:
	/// 初期化済みActorWorldへEnemyActorだけを生成する。Target用Dummyは生成しない。
	void Initialize(K4E::ActorWorld& actorWorld);

	/// Play中にTarget接続、Camera中心照準、Weapon発射成立後のDamage適用を更新する。
	void Update(float deltaTime);

	/// Edit・Pause中もPIE復元後の参照だけを安全に張り直す。
	void UpdateEditor();

	/// EnemyActorの検証情報と、Player/Enemy/Bossのゲーム用HP HUDを描画する。
	void DrawImGui();

	/// ActorWorldへの非所有参照を解除する。
	void Finalize();

private:
	/// PIE複製後の現在WorldからEnemy、Boss、DebugPlayerを再取得して接続する。
	void RefreshActorReferencesAndBindings();

	/// Camera中心RayとCharacter ColliderのAABBから現在照準対象を決める。
	void UpdateAimTarget();

	/// WeaponComponentのShotRevision増加を1回だけDamageへ変換する。
	void ProcessWeaponShot();

	/// Player固定HUD、照準中Enemy頭上HP、Boss上部HP、Crosshair、弾薬を描画する。
	void DrawGameplayHud();

	/// UIから予約されたDamageとResetを更新フェーズで処理する。
	void ProcessRequests();

private:
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::EnemyActor* enemy_ = nullptr;
	K4E::BossActor* boss_ = nullptr;
	K4E::CharacterActor* target_ = nullptr;
	K4E::EnemyActor* aimedEnemy_ = nullptr;
	K4E::BossActor* aimedBoss_ = nullptr;
	std::vector<K4E::AABB> navigationObstacles_;
	std::string enemyName_ = "ComponentEnemy";
	std::string bossName_ = "ComponentBoss";
	std::string targetName_ = "DebugPlayer";
	std::string lastMessage_ = "DebugPlayerへのTarget接続を待機中";
	unsigned int lastProcessedShotRevision_ = 0;
	bool lastSucceeded_ = false;
	bool requestDamage_ = false;
	bool requestLethalDamage_ = false;
	bool requestReset_ = false;
};