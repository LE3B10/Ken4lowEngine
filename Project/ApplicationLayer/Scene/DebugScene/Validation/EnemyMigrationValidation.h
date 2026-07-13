#pragma once

#include <AABB.h>

#include <memory>
#include <vector>

class EnemyParticleEffectSystem;
class MeleeEnemy;

namespace Ken4lowEngine
{
	class ActorWorld;
	class CharacterActor;
	class Collider;
	class EnemyActor;
}

namespace K4E = ::Ken4lowEngine;

/// DebugSceneへ旧通常敵とComponent通常敵を同時配置し、実戦条件を比較する検証器。
class EnemyMigrationValidation
{
public:
	/// 前方宣言した所有型を実装側で安全に破棄する。
	~EnemyMigrationValidation();

	/// 初期化済みActorWorldへ新通常敵とTargetを生成し、旧通常敵も同条件で準備する。
	void Initialize(K4E::ActorWorld& actorWorld);

	/// Play中だけ旧通常敵を更新し、新通常敵はActorWorldのComponent経路へ任せる。
	void Update(float deltaTime);

	/// Edit・Pause中はゲームロジックを進めず、UI操作要求だけを処理する。
	void UpdateEditor();

	/// ActorWorldに属さない旧通常敵だけを通常描画する。
	void DrawLegacy();

	/// ActorWorldに属さない旧通常敵だけをShadow Passへ描画する。
	void DrawLegacyShadow();

	/// 旧・新通常敵の移動、A*、攻撃、死亡、Effect、Collider、描画を一覧比較する。
	void DrawImGui();

	/// Scene終了時にActorWorldへの非所有参照を解除し、旧敵の所有物を解放する。
	void Finalize();

private:
	/// UIから予約されたダメージ操作を描画開始前の更新フェーズで処理する。
	void ProcessRequests();

	/// 同じ被弾条件を旧HP経路と共通Health経路へ一度ずつ適用する。
	void ApplyDamageToBoth(int amount);

private:
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::EnemyActor* componentEnemy_ = nullptr;
	K4E::CharacterActor* componentTarget_ = nullptr;
	std::unique_ptr<MeleeEnemy> legacyEnemy_;
	std::unique_ptr<K4E::Collider> legacyTarget_;
	std::unique_ptr<EnemyParticleEffectSystem> legacyEffectSystem_;
	std::vector<K4E::AABB> navigationObstacles_;
	int legacyHitEffectCount_ = 0;
	int legacyDeathEffectCount_ = 0;
	bool requestDamage_ = false;
	bool requestLethalDamage_ = false;
};
