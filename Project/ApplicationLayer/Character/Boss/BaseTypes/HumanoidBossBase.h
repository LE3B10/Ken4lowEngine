#pragma once
#include "Core/BossBase.h"

/// 人型ボスの部位所有はHumanoidVisualComponentへ統一し、この型はBossBaseへの薄い構成指定だけを残す。
class HumanoidBossBase : public BossBase
{
public:
	~HumanoidBossBase() override = default;

	/// BossBaseの生成手順から共通Character/Humanoid Component構成を初期化する。
	void BuildBossParts() override
	{
		BaseCharacter::Initialize(); // 個別Object3D生成を行わず、HumanoidVisualComponentの定義から一括生成する。
	}

	/// 具体的なボス初期値は派生Bossが設定するため、共通側では追加処理を持たない。
	void SetupBoss() override {}

	/// 旧専用Debug経路を作らずBossBaseのDetails/Debug表示へ統一する。
	void DrawImGui() override { BossBase::DrawImGui(); }
};
