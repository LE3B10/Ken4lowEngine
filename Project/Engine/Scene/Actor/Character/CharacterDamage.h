#pragma once

#include "Vector3.h"

namespace Ken4lowEngine
{
	class Actor;
	class CharacterActor;

	/// CharacterActorへ渡すダメージ要求を、発生元や命中位置とまとめて保持する。
	struct CharacterDamageInfo
	{
		float amount = 0.0f;
		const Actor* sourceActor = nullptr;
		Vector3 hitPosition{};
		bool hasHitPosition = false;
	};

	/// CharacterHealthComponentが実際に適用したダメージ結果を返す。
	struct CharacterDamageResult
	{
		bool accepted = false;
		bool killed = false;
		float requestedDamage = 0.0f;
		float appliedDamage = 0.0f;
		float healthBefore = 0.0f;
		float healthAfter = 0.0f;
	};

	/// 生存状態がダメージによって死亡へ遷移した際の通知内容を保持する。
	struct CharacterDeathEvent
	{
		CharacterActor* character = nullptr;
		CharacterDamageInfo damage{};
		CharacterDamageResult result{};
	};
} // namespace Ken4lowEngine
