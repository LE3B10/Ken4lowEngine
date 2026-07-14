#pragma once

#include <AABB.h>

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class ActorWorld;
	class CharacterActor;
	class EnemyActor;
}

namespace K4E = ::Ken4lowEngine;

/// DebugSceneへ移行済みEnemyActorを配置し、操作中のDebugPlayerを実戦Targetとして検証する。
class EnemyMigrationValidation
{
public:
	/// 初期化済みActorWorldへEnemyActorだけを生成する。Target用Dummyは生成しない。
	void Initialize(K4E::ActorWorld& actorWorld);

	/// Play中に現在WorldのDebugPlayerへTarget参照を張り直す。
	void Update(float deltaTime);

	/// Edit・Pause中もPIE復元後の参照だけを安全に張り直す。
	void UpdateEditor();

	/// EnemyActorのAI・攻撃・HP・Target状態を表示する。
	void DrawImGui();

	/// ActorWorldへの非所有参照を解除する。
	void Finalize();

private:
	/// PIE複製後の現在WorldからEnemyとDebugPlayerを再取得して接続する。
	void RefreshActorReferencesAndBindings();

	/// UIから予約されたDamageとResetを更新フェーズで処理する。
	void ProcessRequests();

private:
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::EnemyActor* enemy_ = nullptr;
	K4E::CharacterActor* target_ = nullptr;
	std::vector<K4E::AABB> navigationObstacles_;
	std::string enemyName_ = "ComponentEnemy";
	std::string targetName_ = "DebugPlayer";
	std::string lastMessage_ = "DebugPlayerへのTarget接続を待機中";
	bool lastSucceeded_ = false;
	bool requestDamage_ = false;
	bool requestLethalDamage_ = false;
	bool requestReset_ = false;
};