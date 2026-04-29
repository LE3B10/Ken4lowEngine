#include "StageMission.h"
#include "GamePlayStageContext.h"
#include "GamePlayWorld.h"
#include "EnemyBase.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>

void StageMissionBase::Initialize(GamePlayWorld* world, const GamePlayStageContext& stageContext, const StageMissionConfig& config) {
    world_ = world; stageContext_ = &stageContext; config_ = config; isCleared_ = false; isFailed_ = false;
}
void StageMissionBase::DrawDebugImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Mission: %s", GetDebugName());
    ImGui::Text("Cleared: %s", isCleared_ ? "true" : "false");
    ImGui::Text("Failed : %s", isFailed_ ? "true" : "false");
#endif
}

class WaveStageMission : public StageMissionBase { public: void Update(float) override { if (world_) { isCleared_ = world_->IsAllWavesCleared() && world_->GetCharacters().GetEnemyCount() == 0; } } const char* GetDebugName() const override { return "WaveStageMission"; } };
class ExploreStageMission : public StageMissionBase { public: void Update(float) override; void DrawDebugImGui() override; const char* GetDebugName() const override { return "ExploreStageMission"; } private: float distance_ = 0.0f; };
void ExploreStageMission::Update(float) { if (!world_) return; auto* p = world_->GetCharacters().GetPlayer(); if (!p || !p->GetWorldTransform()) return; distance_ = K4E::Vector3::Length(p->GetWorldTransform()->translate_ - config_.clearPoint); isCleared_ = distance_ <= config_.clearRadius; }
void ExploreStageMission::DrawDebugImGui() { StageMissionBase::DrawDebugImGui();
#ifdef USE_IMGUI
    ImGui::Text("ExploreDist: %.2f / %.2f", distance_, config_.clearRadius);
#endif
}

class DefenseStageMission : public StageMissionBase { public: void Initialize(GamePlayWorld* w, const GamePlayStageContext& c, const StageMissionConfig& cfg) override { StageMissionBase::Initialize(w, c, cfg); hp_ = cfg.defenseTargetHp; } void Update(float dt) override; void DrawDebugImGui() override; const char* GetDebugName() const override { return "DefenseStageMission"; } private: float elapsed_ = 0.0f; float hp_ = 0.0f; };
void DefenseStageMission::Update(float dt) { if (!world_ || isFailed_) return; elapsed_ += dt; for (auto* e : world_->GetCharacters().GetEnemyRawList()) { if (!e || e->IsDead()) continue; if (K4E::Vector3::Length(e->GetCenterPosition() - config_.clearPoint) < 4.0f) { hp_ -= dt * 5.0f; } } if (hp_ <= 0.0f) { hp_ = 0.0f; isFailed_ = true; } isCleared_ = (elapsed_ >= config_.defenseTime) && !isFailed_; }
void DefenseStageMission::DrawDebugImGui() { StageMissionBase::DrawDebugImGui();
#ifdef USE_IMGUI
    ImGui::Text("DefenseRemain: %.2f", std::max(0.0f, config_.defenseTime - elapsed_));
    ImGui::Text("DefenseHP: %.1f", hp_);
#endif
}

class EscapeStageMission : public StageMissionBase { public: void Update(float dt) override; void DrawDebugImGui() override; const char* GetDebugName() const override { return "EscapeStageMission"; } private: float elapsed_ = 0.0f; float distance_ = 0.0f; };
void EscapeStageMission::Update(float dt) { if (!world_) return; elapsed_ += dt; auto* p = world_->GetCharacters().GetPlayer(); if (!p || !p->GetWorldTransform()) return; distance_ = K4E::Vector3::Length(p->GetWorldTransform()->translate_ - config_.escapePoint); isCleared_ = distance_ <= config_.escapeRadius; if (config_.timeLimit > 0.0f && elapsed_ >= config_.timeLimit && !isCleared_) isFailed_ = true; }
void EscapeStageMission::DrawDebugImGui() { StageMissionBase::DrawDebugImGui();
#ifdef USE_IMGUI
    ImGui::Text("EscapeDist: %.2f / %.2f", distance_, config_.escapeRadius);
    ImGui::Text("TimeLimit: %.2f / %.2f", elapsed_, config_.timeLimit);
#endif
}

class BossStageMission : public StageMissionBase { public: void Update(float) override; void DrawDebugImGui() override; const char* GetDebugName() const override { return "BossStageMission"; } private: float bossHp_ = 0.0f; };
void BossStageMission::Update(float) { if (!world_) return; auto enemies = world_->GetCharacters().GetEnemyRawList(); if (enemies.empty()) { isCleared_ = true; bossHp_ = 0.0f; return; } bossHp_ = static_cast<float>(enemies.front()->GetHp()); isCleared_ = std::all_of(enemies.begin(), enemies.end(), [](EnemyBase* e) { return !e || e->IsDead(); }); }
void BossStageMission::DrawDebugImGui() { StageMissionBase::DrawDebugImGui();
#ifdef USE_IMGUI
    ImGui::Text("BossHP: %.1f", bossHp_);
#endif
}

std::unique_ptr<IStageMission> CreateStageMissionByType(StageType t) { switch (t) { case StageType::Wave: return std::make_unique<WaveStageMission>(); case StageType::Explore: return std::make_unique<ExploreStageMission>(); case StageType::Defense: return std::make_unique<DefenseStageMission>(); case StageType::Escape: return std::make_unique<EscapeStageMission>(); case StageType::Boss: return std::make_unique<BossStageMission>(); } return std::make_unique<WaveStageMission>(); }
StageType ResolveStageTypeFromStageIndex(int i) { switch (i) { case 0: return StageType::Wave; case 1: return StageType::Explore; case 2: return StageType::Defense; case 3: return StageType::Escape; case 4: return StageType::Boss; default: return StageType::Wave; } }
const char* ToStageTypeName(StageType t) { switch (t) { case StageType::Wave: return "Wave"; case StageType::Explore: return "Explore"; case StageType::Defense: return "Defense"; case StageType::Escape: return "Escape"; case StageType::Boss: return "Boss"; } return "Unknown"; }
