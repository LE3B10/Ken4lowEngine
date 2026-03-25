#pragma once

#include "PlayerInputSnapshot.h"

class PlayerWeaponComponent;
class PlayerWeaponVisualComponent;
class PlayerBrainComponent;
struct PlayerAPI;

/// -------------------------------------------------------------
/// PlayerWeaponController
/// - プレイヤー入力から「武器切替 / リロードキャンセル / CombatFSM復帰」を担当
/// - 武器ロジック本体は PlayerWeaponComponent に残し、
///   その手前の制御だけをここへ分離する
/// -------------------------------------------------------------
class PlayerWeaponController
{
public:
	void Initialize(
		PlayerWeaponComponent* weapon,
		PlayerWeaponVisualComponent* visual,
		PlayerBrainComponent* brain,
		PlayerAPI* api);

	// ホイールでカテゴリ切替を行う
	void HandleWheelSwitch(InputSnapshot& snap);

	// リロードだけを中断する
	void CancelReloadOnly();

	// リロードを中断しつつ CombatFSM を Hip / Aim に戻す
	void TryCancelReloadAndRestoreCombat(const InputSnapshot& snap, float deltaTime);

private:
	PlayerWeaponComponent* weapon_ = nullptr;
	PlayerWeaponVisualComponent* visual_ = nullptr;
	PlayerBrainComponent* brain_ = nullptr;
	PlayerAPI* api_ = nullptr;
};
