#pragma once

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	class ActorWorld;
	class CharacterActor;
}

namespace K4E = ::Ken4lowEngine;

/// DebugScene上でCharacterActor Phase 2の責務境界とライフサイクルを検証する。
class CharacterActorPhase2Validation
{
public:
	/// 初期化済みActorWorldへ検証用CharacterActorを生成する。
	void Initialize(K4E::ActorWorld& actorWorld);

	/// Debug UIから予約された操作をActorWorld更新前に処理する。
	void ProcessRequests();

	/// CharacterActor Phase 2の状態と検証操作をImGuiへ表示する。
	void DrawImGui();

	/// Scene終了時にActorWorldとListenerの非所有参照を解除する。
	void Finalize();

private:
	/// 名前で検証対象CharacterActorを検索する。
	K4E::CharacterActor* FindTarget() const;

	/// 検証用のActor設定と死亡Listenerを重複なく適用する。
	void ConfigureTarget(K4E::CharacterActor& actor);

	/// HP、移動、ターゲット、死亡通知、JSON復元をまとめて検証する。
	void RunValidation();

private:
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::CharacterActor* listenerActor_ = nullptr;
	std::uint64_t listenerId_ = 0;
	std::string targetActorName_ = "Phase2CharacterActor";
	std::string jsonPath_ = "../Generated/Intermediate/CharacterActorPhase2Validation.json";
	std::string lastMessage_ = "未検証";
	float lastAppliedDamage_ = 0.0f;
	float lastDeathDamage_ = 0.0f;
	int deathEventCount_ = 0;
	bool lastSucceeded_ = false;
	bool requestSpawn_ = false;
	bool requestDamage_ = false;
	bool requestLethalDamage_ = false;
	bool requestRestore_ = false;
	bool requestMove_ = false;
	bool requestStop_ = false;
	bool requestSave_ = false;
	bool requestReload_ = false;
	bool requestValidation_ = true;
};
