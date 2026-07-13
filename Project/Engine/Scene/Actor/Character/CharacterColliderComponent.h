#pragma once

#include "ColliderComponent.h"

namespace Ken4lowEngine
{
	/// Characterで共有する当たり判定設定とActor Transformへの追従処理を提供する。
	class CharacterColliderComponent : public ColliderComponent
	{
	public:
		/// Character向けの扱いやすい既定形状を設定する。
		CharacterColliderComponent();

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "CharacterColliderComponent"; }
	};
} // namespace Ken4lowEngine
