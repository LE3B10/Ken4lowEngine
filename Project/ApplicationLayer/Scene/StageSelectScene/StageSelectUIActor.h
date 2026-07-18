#pragma once

#include "IStageSelector.h"

#include <Actor.h>
#include <SceneComponent.h>
#include <TextComponent.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// ステージセレクトの文字情報をTextComponent単位で所有するScreen Space UI Actor。
class StageSelectUIActor final : public K4E::Actor
{
public:
	void Initialize() override
	{
		const bool hasSavedLayout = FindComponentByName("Title") != nullptr;
		if (!GetRootComponent())
		{
			auto& root = CreateRootComponent<K4E::SceneComponent>();
			root.SetName("Stage Select UI Root");
		}

		title_ = EnsureTextComponent("Title", 100);
		stageNumber_ = EnsureTextComponent("Stage Number", 101);
		stageName_ = EnsureTextComponent("Stage Name", 102);
		category_ = EnsureTextComponent("Category", 103);
		objective_ = EnsureTextComponent("Objective", 104);
		description_ = EnsureTextComponent("Description", 105);
		unlockCondition_ = EnsureTextComponent("Unlock Condition", 106);
		guide_ = EnsureTextComponent("Guide", 107);

		if (title_) title_->SetText("STAGE SELECT");
		if (guide_) guide_->SetText("CLICK : SELECT   WHEEL / DRAG : MOVE   ESC : BACK");
		if (!hasSavedLayout) ApplyDefaultLayout(); // Prefab復元時は保存済みの位置と文字サイズを維持する。
		ApplyStageText();
		ApplyPresentation();
	}

	void Update(float deltaTime) override
	{
		if (ResolveTextComponents())
		{
			ApplyStageText(); // JSON再読込でComponent実体が変わった直後に動的なステージ文字を戻す。
		}
		const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
		transitionTimer_ = std::min(transitionDuration_, transitionTimer_ + safeDeltaTime);
		guidePulseTimer_ += safeDeltaTime;
		ApplyPresentation();
		K4E::Actor::Update(safeDeltaTime);
	}

	void UpdateEditor(float deltaTime) override
	{
		Update(deltaTime);
	}

	std::string GetClassTypeName() const override { return "StageSelectUIActor"; }

	void SetViewportSize(float width, float height)
	{
		const float safeWidth = std::max(1.0f, width);
		const float safeHeight = std::max(1.0f, height);
		if (std::fabs(viewportWidth_ - safeWidth) < 0.5f && std::fabs(viewportHeight_ - safeHeight) < 0.5f)
		{
			return;
		}
		viewportWidth_ = safeWidth;
		viewportHeight_ = safeHeight;
		ApplyDefaultLayout();
	}

	void SetStageInfo(const StageInfo& stage)
	{
		const bool changed = stageId_ != stage.id || stageNameText_ != stage.name || categoryText_ != stage.category ||
			descriptionText_ != stage.description || unlockText_ != stage.unlockCondition || locked_ != stage.locked;
		stageId_ = stage.id;
		stageNameText_ = stage.name;
		categoryText_ = stage.category;
		descriptionText_ = stage.description;
		unlockText_ = stage.unlockCondition;
		locked_ = stage.locked;
		if (changed) transitionTimer_ = 0.0f;
		ApplyStageText();
	}

private:
	K4E::TextComponent* EnsureTextComponent(const char* name, int drawOrder)
	{
		if (auto* existing = dynamic_cast<K4E::TextComponent*>(FindComponentByName(name)))
		{
			return existing;
		}
		auto& text = AddComponent<K4E::TextComponent>();
		text.SetName(name);
		text.SetDrawOrder(drawOrder);
		text.SetAnchor({ 0.5f, 0.5f });
		text.SetFontName("DotGothic16");
		return &text;
	}

	bool ResolveTextComponents()
	{
		const auto resolve = [this](const char* name) -> K4E::TextComponent*
		{
			return dynamic_cast<K4E::TextComponent*>(FindComponentByName(name));
		};
		K4E::TextComponent* newTitle = resolve("Title");
		K4E::TextComponent* newStageNumber = resolve("Stage Number");
		K4E::TextComponent* newStageName = resolve("Stage Name");
		K4E::TextComponent* newCategory = resolve("Category");
		K4E::TextComponent* newObjective = resolve("Objective");
		K4E::TextComponent* newDescription = resolve("Description");
		K4E::TextComponent* newUnlockCondition = resolve("Unlock Condition");
		K4E::TextComponent* newGuide = resolve("Guide");
		const bool changed = title_ != newTitle || stageNumber_ != newStageNumber || stageName_ != newStageName ||
			category_ != newCategory || objective_ != newObjective || description_ != newDescription ||
			unlockCondition_ != newUnlockCondition || guide_ != newGuide;
		title_ = newTitle;
		stageNumber_ = newStageNumber;
		stageName_ = newStageName;
		category_ = newCategory;
		objective_ = newObjective;
		description_ = newDescription;
		unlockCondition_ = newUnlockCondition;
		guide_ = newGuide;
		return changed;
	}

	void ApplyDefaultLayout()
	{
		const float scaleX = viewportWidth_ / 1920.0f;
		const float scaleY = viewportHeight_ / 1080.0f;
		const float uiScale = std::max(0.50f, std::min(scaleX, scaleY));
		const float centerX = viewportWidth_ * 0.5f;
		const auto setLayout = [centerX, scaleY, uiScale](K4E::TextComponent* component, float y, float fontSize)
		{
			if (!component) return;
			component->SetPosition({ centerX, y * scaleY });
			component->SetFontSize(fontSize * uiScale);
		};

		// サムネイル上端と下端を空け、見出しと説明を別々の情報帯として配置する。
		setLayout(title_, 64.0f, 48.0f);
		setLayout(stageNumber_, 348.0f, 26.0f);
		setLayout(stageName_, 734.0f, 40.0f);
		setLayout(category_, 790.0f, 28.0f);
		setLayout(objective_, 838.0f, 26.0f);
		setLayout(description_, 880.0f, 22.0f);
		setLayout(unlockCondition_, 926.0f, 21.0f);
		setLayout(guide_, 1032.0f, 18.0f);
	}

	void ApplyStageText()
	{
		char stageNumberText[32]{};
		std::snprintf(stageNumberText, sizeof(stageNumberText), "STAGE %02u", stageId_ + 1u);
		if (title_) title_->SetText("STAGE SELECT");
		if (stageNumber_) stageNumber_->SetText(stageNumberText);
		if (stageName_) stageName_->SetText(stageNameText_);
		if (category_) category_->SetText(GetCategoryDisplayName(categoryText_));
		if (objective_) objective_->SetText(std::string("OBJECTIVE : ") + GetCategoryObjective(categoryText_));
		if (description_) description_->SetText(descriptionText_);
		if (unlockCondition_)
		{
			unlockCondition_->SetVisible(locked_);
			unlockCondition_->SetText(unlockText_.empty() ? "前ステージクリアで解放" : unlockText_);
		}
		if (guide_) guide_->SetText("CLICK : SELECT   WHEEL / DRAG : MOVE   ESC : BACK");
	}

	void ApplyPresentation()
	{
		const float normalized = transitionDuration_ > 0.0f ? std::clamp(transitionTimer_ / transitionDuration_, 0.0f, 1.0f) : 1.0f;
		const float remain = 1.0f - normalized;
		const float enterAlpha = 1.0f - remain * remain * remain;
		const float pulse = 0.5f + 0.5f * std::sin(guidePulseTimer_ * 2.0f);
		const K4E::Vector4 accent = GetCategoryAccentColor(categoryText_);

		if (title_) title_->SetColor({ 1.0f, 1.0f, 1.0f, 0.96f });
		if (stageNumber_) stageNumber_->SetColor({ 0.86f, 0.94f, 1.0f, enterAlpha });
		if (stageName_) stageName_->SetColor({ 1.0f, 1.0f, 1.0f, enterAlpha });
		if (category_) category_->SetColor({ accent.x, accent.y, accent.z, enterAlpha });
		if (objective_) objective_->SetColor({ 1.0f, 0.96f, 0.80f, enterAlpha });
		if (description_) description_->SetColor({ 0.88f, 0.93f, 0.96f, enterAlpha });
		if (unlockCondition_) unlockCondition_->SetColor({ 1.0f, 0.72f, 0.72f, enterAlpha });
		if (guide_) guide_->SetColor({ 1.0f, 1.0f, 1.0f, 0.68f + pulse * 0.25f });
	}

	static const char* GetCategoryDisplayName(const std::string& category)
	{
		if (category == "WAVE") return "WAVE STAGE";
		if (category == "SEARCH") return "SEARCH STAGE";
		if (category == "DEFENSE") return "DEFENSE STAGE";
		if (category == "ESCAPE") return "ESCAPE STAGE";
		if (category == "BOSS") return "BOSS STAGE";
		return "UNKNOWN STAGE";
	}

	static const char* GetCategoryObjective(const std::string& category)
	{
		if (category == "WAVE") return "正面突破で戦況を切り開け";
		if (category == "SEARCH") return "探索して進路を切り開け";
		if (category == "DEFENSE") return "拠点を守り抜け";
		if (category == "ESCAPE") return "敵をかわして脱出せよ";
		if (category == "BOSS") return "最終決戦に挑め";
		return "ステージ目標を達成せよ";
	}

	static K4E::Vector4 GetCategoryAccentColor(const std::string& category)
	{
		if (category == "WAVE") return { 0.80f, 0.96f, 1.00f, 1.0f };
		if (category == "SEARCH") return { 1.00f, 0.90f, 0.60f, 1.0f };
		if (category == "DEFENSE") return { 0.68f, 0.88f, 1.00f, 1.0f };
		if (category == "ESCAPE") return { 1.00f, 0.76f, 0.62f, 1.0f };
		if (category == "BOSS") return { 1.00f, 0.55f, 0.55f, 1.0f };
		return { 1.0f, 1.0f, 1.0f, 1.0f };
	}

private:
	K4E::TextComponent* title_ = nullptr;
	K4E::TextComponent* stageNumber_ = nullptr;
	K4E::TextComponent* stageName_ = nullptr;
	K4E::TextComponent* category_ = nullptr;
	K4E::TextComponent* objective_ = nullptr;
	K4E::TextComponent* description_ = nullptr;
	K4E::TextComponent* unlockCondition_ = nullptr;
	K4E::TextComponent* guide_ = nullptr;

	float viewportWidth_ = 1920.0f;
	float viewportHeight_ = 1080.0f;
	float transitionTimer_ = 0.28f;
	float transitionDuration_ = 0.28f;
	float guidePulseTimer_ = 0.0f;
	std::uint32_t stageId_ = 0u;
	std::string stageNameText_ = "始まりの平原";
	std::string categoryText_ = "WAVE";
	std::string descriptionText_ = "基本戦闘を学ぶウェーブ制ステージ";
	std::string unlockText_;
	bool locked_ = false;
};
