#pragma once

namespace Ken4lowEngine
{
	class CharacterHealthComponent;
}

/// Boss固有コードへ状態参照APIを提供し、HPの正本はCharacterHealthComponentへ委譲する。
class BossStatusComponent
{
public:
	/// 共通Healthへ接続し、Boss初期最大HPを適用する。
	void Initialize(Ken4lowEngine::CharacterHealthComponent* health, float maxHP);

	/// 時限無敵だけを更新し、HP値そのものは保持しない。
	void Update(float deltaTime);

	/// 非所有参照を解除する。
	void Finalize();

	void ApplyDamage(float damage);
	void Heal(float value);
	void FullRecover();
	void SetMaxHP(float maxHP);
	void SetHP(float hp);

	void SetInvincible(bool isInvincible);
	void SetInvincibleTimer(float timeSec);
	bool IsInvincible() const;

	float GetHP() const;
	float GetMaxHP() const;
	float GetHPRate() const;
	bool IsDead() const;
	bool IsAlive() const { return !IsDead(); }

private:
	Ken4lowEngine::CharacterHealthComponent* health_ = nullptr; // HP実体はBoss Actorの共通Componentだけが所有する。
	float invincibleTimer_ = 0.0f;
};
