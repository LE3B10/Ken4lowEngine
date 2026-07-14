#pragma once

#include <string>

namespace Ken4lowEngine
{
	class ActorWorld;
	class BossActor;
	class CharacterActor;
}

namespace K4E = ::Ken4lowEngine;

/// DebugSceneへBossActorを配置し、Phase 9のComponent分離とJSON経路を検証する補助クラス。
class BossMigrationValidation
{
public:
	/// 初期化済みActorWorldへBossActorと攻撃Targetを配置する。
	void Initialize(K4E::ActorWorld& actorWorld);

	/// Play中にRuntime Worldの参照を接続し、予約操作を処理する。
	void Update();

	/// Edit・Pause中もEditor WorldのJSON操作と参照再接続を安全に行う。
	void UpdateEditor();

	/// Bossの共通Component、専用Component、弱点、JSON状態を一覧表示する。
	void DrawImGui();

	/// ActorWorldへの非所有参照を解除する。
	void Finalize();

private:
	/// PIE複製後の現在Worldから名前でActorを再取得してTargetを接続する。
	void RefreshActorReferencesAndBindings();

	/// UIから予約された弱点ダメージ、Reset、JSON操作を更新フェーズで実行する。
	void ProcessRequests();

private:
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::BossActor* boss_ = nullptr;
	K4E::CharacterActor* target_ = nullptr;
	std::string bossName_ = "ComponentBoss";
	std::string targetName_ = "ComponentBossTarget";
	std::string jsonPath_ = "../Generated/Intermediate/BossActorValidation.json";
	std::string lastMessage_ = "未検証";
	bool lastSucceeded_ = false;
	bool requestHeadDamage_ = false;
	bool requestBodyDamage_ = false;
	bool requestReset_ = false;
	bool requestSave_ = false;
	bool requestReload_ = false;
};
