#pragma once
//struct BossPhaseThreshold
//{
//    BossPhase phase;
//    float hpRateThreshold;
//};
//
//class BossPhaseComponent
//{
//public:
//    void Initialize(const std::vector<BossPhaseThreshold>& thresholds);
//    void Update(float hpRate);
//
//    BossPhase GetCurrentPhase() const;
//    bool ConsumePhaseChanged();
//
//private:
//    std::vector<BossPhaseThreshold> thresholds_;
//    BossPhase currentPhase_ = BossPhase::Phase1;
//    bool phaseChanged_ = false;
//};