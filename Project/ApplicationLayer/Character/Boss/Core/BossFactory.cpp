#include "BossFactory.h"

/// -------------------------------------------------------------
///							生成処理
/// -------------------------------------------------------------
std::unique_ptr<BossBase> BossFactory::Create(BossType type)
{
	(void)type; // 未使用
	return std::unique_ptr<BossBase>();
}
