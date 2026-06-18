#pragma once

#include "EnemySpawnCrystal.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class EnemyHPBar;

/// -------------------------------------------------------------
/// クリスタル頭上HPバーの生成・更新・描画・Debug表示を担当するクラス。
///
/// CrystalManagerからHPバー専用の状態を切り離し、クリスタル本体の
/// 生成・破壊・敵スポーン管理とUI表示管理が混ざらないようにする。
/// -------------------------------------------------------------
class CrystalHpBarController
{
public:
	struct Settings
	{
		bool visible = true;
		bool alwaysVisible = true;
		float offsetY = 0.35f;
		float width = 82.0f;
		float height = 9.0f;
		float showTime = 3.0f;
	};

	struct DebugInfo
	{
		int hp = 0;
		int maxHp = 0;
		float hpRate = 0.0f;
		K4E::Vector3 worldPosition{};
		K4E::Vector2 screenPosition{};
		bool active = false;
		bool broken = false;
		bool inFront = false;
		bool inScreen = false;
		bool visible = false;
		std::string hiddenReason;
	};

public:
	~CrystalHpBarController();

	void Initialize(size_t crystalCount);
	void Finalize();
	void Update(const std::vector<EnemySpawnCrystal>& crystals, const K4E::Matrix4x4& viewMatrix, const K4E::Matrix4x4& projMatrix, float screenWidth, float screenHeight, float deltaTime, const EnemySpawnCrystal* aimedCrystal, bool showOnlyWhenAimed, float visibleHoldTime);
	void Draw();
	void DrawImGui() const;

	Settings& GetSettings() { return settings_; }
	const Settings& GetSettings() const { return settings_; }

private:
	void EnsureBarCount(size_t crystalCount);

private:
	Settings settings_{};
	std::vector<std::unique_ptr<EnemyHPBar>> hpBars_;
	std::vector<DebugInfo> debugInfos_;
	std::vector<float> aimTimers_;
	bool drawCalled_ = false;
	int visibleCount_ = 0;
};
