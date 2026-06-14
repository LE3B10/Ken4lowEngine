#pragma once
#include <cstdint>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         物理種別
	/// -------------------------------------------------------------
	enum class BodyType : uint8_t
	{
		Static,
		Dynamic,
		Kinematic,
	};

	/// -------------------------------------------------------------
	///                         衝突モード
	/// -------------------------------------------------------------
	enum class CollisionMode : uint8_t
	{
		Collision,
		Trigger,
	};

} // namespace Ken4lowEngine
