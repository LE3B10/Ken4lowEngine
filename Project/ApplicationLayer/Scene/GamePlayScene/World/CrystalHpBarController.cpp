#define NOMINMAX
#include "CrystalHpBarController.h"

#include "EnemyHPBar.h"
#include "EnemyHPBarProjector.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

CrystalHpBarController::~CrystalHpBarController()
{
	Finalize();
}

void CrystalHpBarController::Initialize(size_t crystalCount)
{
	Finalize();
	EnsureBarCount(crystalCount);
}

void CrystalHpBarController::Finalize()
{
	hpBars_.clear();
	debugInfos_.clear();
	aimTimers_.clear();
	drawCalled_ = false;
	visibleCount_ = 0;
}

void CrystalHpBarController::Update(const std::vector<EnemySpawnCrystal>& crystals, const K4E::Matrix4x4& viewMatrix, const K4E::Matrix4x4& projMatrix, float screenWidth, float screenHeight, float deltaTime, const EnemySpawnCrystal* aimedCrystal, bool showOnlyWhenAimed, float visibleHoldTime)
{
	drawCalled_ = false;
	visibleCount_ = 0;
	EnsureBarCount(crystals.size());

	if (!settings_.visible)
	{
		for (auto& bar : hpBars_)
		{
			if (bar) { bar->SetVisible(false); }
		}
		return;
	}

	for (size_t i = 0; i < crystals.size(); ++i)
	{
		const EnemySpawnCrystal& crystal = crystals[i];
		DebugInfo debug{};
		debug.hp = crystal.GetHp();
		debug.maxHp = crystal.GetMaxHp();
		debug.hpRate = std::clamp(crystal.GetHpRate(), 0.0f, 1.0f);
		debug.active = crystal.IsAlive();
		debug.broken = crystal.IsDestroyed();

		bool visible = true;
		if (&crystal == aimedCrystal)
		{
			aimTimers_[i] = std::max(0.0f, visibleHoldTime);
		}
		else if (aimTimers_[i] > 0.0f)
		{
			aimTimers_[i] = std::max(0.0f, aimTimers_[i] - deltaTime);
		}

		if (crystal.IsDestroyed())
		{
			visible = false;
			debug.hiddenReason = "Broken";
		}
		else if (showOnlyWhenAimed && &crystal != aimedCrystal && aimTimers_[i] <= 0.0f)
		{
			// 照準対象だけHPバーを表示し、外れ際は短い保持時間でチラつきを抑える。
			visible = false;
			debug.hiddenReason = "Not aimed";
		}
		else if (!settings_.alwaysVisible && crystal.GetHp() >= crystal.GetMaxHp())
		{
			visible = false;
			debug.hiddenReason = "Full HP";
		}

		const K4E::Vector3& pos = crystal.GetPosition();
		const K4E::Vector3& scale = crystal.GetScale();
		// クリスタル頭上にHPバーを出すため、モデルScaleと調整用Offsetを加味して表示位置を作る。
		const K4E::Vector3 hpBarWorldPos{ pos.x, pos.y + std::abs(scale.y) * 0.65f + settings_.offsetY, pos.z };
		debug.worldPosition = hpBarWorldPos;
		const HpBarProjectResult projected = ProjectWorldToScreen(hpBarWorldPos, viewMatrix, projMatrix, screenWidth, screenHeight);
		debug.screenPosition = projected.screenPos;
		debug.inFront = projected.inFront;
		debug.inScreen = projected.inScreen;
		if (visible && (!projected.inFront || !projected.inScreen))
		{
			visible = false;
			debug.hiddenReason = projected.inFront ? "Out of screen" : "Behind camera";
		}

		debug.visible = visible;
		if (visible)
		{
			++visibleCount_;
		}

		if (i < hpBars_.size() && hpBars_[i])
		{
			hpBars_[i]->Update(projected.screenPos, debug.hpRate, visible, deltaTime, settings_.width, settings_.height);
		}
		debugInfos_[i] = debug;
	}
}

void CrystalHpBarController::Draw()
{
	drawCalled_ = true;
	for (auto& bar : hpBars_)
	{
		if (bar)
		{
			bar->Draw();
		}
	}
}

void CrystalHpBarController::DrawImGui() const
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Crystal HP Bar");
	ImGui::Text("表示: %s", settings_.visible ? "ON" : "OFF");
	ImGui::Text("常時表示: %s", settings_.alwaysVisible ? "ON" : "OFF");
	ImGui::Text("OffsetY / Size: %.2f / %.1f x %.1f", settings_.offsetY, settings_.width, settings_.height);
	ImGui::Text("被弾後表示時間: %.2f 秒", settings_.showTime);
	ImGui::Text("Draw呼び出し: %s", drawCalled_ ? "はい" : "いいえ");
	ImGui::Text("表示対象数: %d / %d", visibleCount_, static_cast<int>(debugInfos_.size()));
	for (size_t i = 0; i < debugInfos_.size(); ++i)
	{
		const auto& info = debugInfos_[i];
		ImGui::Text(
			"CrystalHPBar[%d] HP:%d/%d Rate:%.2f World:(%.2f,%.2f,%.2f) Screen:(%.1f,%.1f) visible:%s reason:%s",
			static_cast<int>(i),
			info.hp,
			info.maxHp,
			info.hpRate,
			info.worldPosition.x,
			info.worldPosition.y,
			info.worldPosition.z,
			info.screenPosition.x,
			info.screenPosition.y,
			info.visible ? "true" : "false",
			info.hiddenReason.empty() ? "-" : info.hiddenReason.c_str());
	}
#endif
}

void CrystalHpBarController::EnsureBarCount(size_t crystalCount)
{
	if (debugInfos_.size() != crystalCount)
	{
		debugInfos_.resize(crystalCount);
	}
	if (aimTimers_.size() != crystalCount)
	{
		aimTimers_.assign(crystalCount, 0.0f);
	}
	if (hpBars_.size() == crystalCount)
	{
		return;
	}

	hpBars_.clear();
	hpBars_.reserve(crystalCount);
	for (size_t i = 0; i < crystalCount; ++i)
	{
		auto bar = std::make_unique<EnemyHPBar>();
		bar->Initialize();
		hpBars_.push_back(std::move(bar));
	}
	aimTimers_.assign(crystalCount, 0.0f);
}
