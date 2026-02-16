#pragma once
#ifdef USE_IMGUI

#include <functional>
#include <filesystem>
#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataEditor.h"

class WeaponDevPanel
{
public:
    // root例: "Resources/JSON/weapons" や "Resources/WeaponMasterData"
    void Initialize(const std::filesystem::path& root,
        std::function<void(int32_t weaponID)> onApply);

    void DrawImGui(); // 編集UI（DebugSceneで使う）
    void DrawEquipOnlyImGui(); // 装備切替だけ（GamePlaySceneで使う）

    WeaponMasterDataDatabase& DB() { return db_; }

private:
    void EnsureLoadedOnce();

private:
    std::filesystem::path root_;
    WeaponMasterDataDatabase db_;
    WeaponMasterDataEditor editor_;
    WeaponEditorHooks hooks_;
    bool initialized_ = false;

    std::function<void(int32_t weaponID)> onApply_;
    int32_t lastAppliedID_ = 0;
};

#endif