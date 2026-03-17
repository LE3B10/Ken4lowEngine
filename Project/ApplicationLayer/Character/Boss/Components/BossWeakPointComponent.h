//#pragma once
//struct WeakPoint
//{
//    std::string name;
//    Vector3 localOffset;
//    float radius = 1.0f;
//    float damageMultiplier = 2.0f;
//    bool isEnabled = true;
//};
//
//class BossWeakPointComponent
//{
//public:
//    void AddWeakPoint(const WeakPoint& weakPoint);
//    void SetEnabled(const std::string& name, bool enabled);
//
//    float GetDamageMultiplierAtHit(const std::string& weakPointName) const;
//
//private:
//    std::vector<WeakPoint> weakPoints_;
//};