#pragma once

#include "Actor.h"
#include "CharacterDamage.h"

#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

namespace Ken4lowEngine
{
	class CharacterHealthComponent;
	class CharacterMovementComponent;
	class CharacterTargetComponent;

	/// Player、通常Enemy、Bossが共有するCharacter用Actorの最小基底クラス。
	class CharacterActor : public Actor
	{
	public:
		using DeathListenerId = std::uint64_t;
		using DeathListener = std::function<void(const CharacterDeathEvent&)>;

	public:
		/// Characterの既定Componentを不足分だけ生成し、処理本体は各Componentへ委譲する。
		void Initialize() override;

		/// 死亡通知の外部参照を解除してからActor共通終了処理を行う。
		void Finalize() override;

		/// JSON保存・復元で使用するActorクラス名を返す。
		std::string GetClassTypeName() const override { return "CharacterActor"; }

		/// HP Componentへダメージ処理を委譲し、死亡へ遷移した場合だけイベントを通知する。
		CharacterDamageResult ApplyDamage(const CharacterDamageInfo& damageInfo);

		/// 数値だけでダメージを渡す簡易入口を提供する。
		CharacterDamageResult ApplyDamage(float amount);

		/// HP Componentへ問い合わせて現在の生存状態を返す。
		bool IsAlive() const;

		/// HP Componentへ問い合わせて現在の死亡状態を返す。
		bool IsDead() const { return !IsAlive(); }

		/// Target ComponentのWorld座標を返し、未設定時はRoot座標へフォールバックする。
		Vector3 GetTargetPosition() const;

		/// CharacterのHP処理を担当するComponentを返す。
		CharacterHealthComponent* GetHealthComponent();

		/// CharacterのHP処理を担当するComponentを返す。
		const CharacterHealthComponent* GetHealthComponent() const;

		/// Characterの代表位置を担当するComponentを返す。
		CharacterTargetComponent* GetTargetComponent();

		/// Characterの代表位置を担当するComponentを返す。
		const CharacterTargetComponent* GetTargetComponent() const;

		/// Characterの移動処理を担当するComponentを返す。
		CharacterMovementComponent* GetMovementComponent();

		/// Characterの移動処理を担当するComponentを返す。
		const CharacterMovementComponent* GetMovementComponent() const;

		/// Characterが所有する指定型Componentへ統一した入口からアクセスする。
		template<class T>
		T* GetCharacterComponent()
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");
			return GetComponent<T>();
		}

		/// Characterが所有する指定型Componentへ統一した入口からアクセスする。
		template<class T>
		const T* GetCharacterComponent() const
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");
			const auto components = GetComponents<T>();
			return components.empty() ? nullptr : components.front();
		}

		/// 死亡通知を受け取るListenerを登録し、解除用IDを返す。
		DeathListenerId AddDeathListener(DeathListener listener);

		/// 登録IDに対応する死亡Listenerを解除する。
		bool RemoveDeathListener(DeathListenerId listenerId);

		/// 登録IDに対応する死亡Listenerが現在も有効か返す。
		bool HasDeathListener(DeathListenerId listenerId) const;

		/// Actorが保持する死亡Listenerをすべて解除する。
		void ClearDeathListeners();

	protected:
		/// 派生Characterが死亡時の固有処理を追加するための拡張点。
		virtual void OnDeath(const CharacterDeathEvent& deathEvent) { (void)deathEvent; }

	private:
		/// 派生処理と登録Listenerへ死亡イベントを一度だけ通知する。
		void NotifyDeath(const CharacterDamageInfo& damageInfo, const CharacterDamageResult& damageResult);

	private:
		struct DeathListenerEntry
		{
			DeathListenerId id = 0;
			DeathListener listener;
		};

		std::vector<DeathListenerEntry> deathListeners_;
		DeathListenerId nextDeathListenerId_ = 1;
		bool deathNotificationSent_ = false;
	};
} // namespace Ken4lowEngine
