# Player Migration Status

## 現在の正本

- GamePlayのPlayer本体は `Ken4lowEngine::PlayerActor`。
- 所有は `CharacterWorld -> ActorWorld -> PlayerActor`。
- Player固有HUDは `PlayerHudPresenterComponent` が担当する。
- 旧 `Player` インスタンスはGamePlayでは生成しない。

## 完了した項目

- Input Snapshot統一
- Walk / Sprint / Jump / Blink
- Camera Look / ADS FOV / ADS感度
- Health / Damage / Death / GameOver / Retry
- Primary WeaponのSemi / Auto / Reload
- PlayerActorのActorWorld所有
- Enemy / Boss / Item / Tutorialの `IPlayerRuntime` 接続
- Player固有HUDの新式化
- 旧HUDManagerのPlayer専用表示を既定で停止

## HUDの現在の役割

### PlayerHudPresenterComponent

- Player HPバーと数値
- Magazine / Reserve / Fire Mode表示
- Crosshair
- Damage時のPlayer表示反応

### HUDManager

- Stage1 Tutorial / Objective
- Boss HP / Boss Guide
- Wave表示
- 被弾方向Indicator

旧ハート、旧Weapon Slot、旧Reload Circle、旧Crosshair、旧NoAmmo表示、旧Control Guideは `legacyPlayerHudVisible_ = false` により描画しない。
旧Playerを比較用に接続した場合は `SetLegacyPlayerHudVisible(true)` で一時的に復帰できる。

## 後から対応するHUD項目

- 新Player HUDをBar / Heartから選べる構造
- Heart表示を採用する場合の1HeartあたりHPと半Heart仕様
- Reload進捗の新HUD化
- No Ammo表示の新HUD化
- Weapon Slot / Weapon Iconの新HUD化
- 操作ガイドの新HUDレイアウト
- Hit / Kill Markerの新Crosshair統合

## 未移行Gameplay機能

- Ladder完全Parity
- Player Meleeの実攻撃とHit判定
- Fall Damage / Fall Death
- Damage Knockback
- 死亡時の吹き飛び物理
- 詳細な被弾方向・部位演出
- 複数武器カテゴリと6Slot完全Parity
- Weapon Master全パラメータ
- Weapon Master Editorから新WeaponComponentへの即時反映
- Weapon固有Reticle / ADS Reticle
- Reload Animation / Weapon View Animation
- Weapon切替VFX / SE

## 削除方針

旧 `Player.cpp` と旧Player Component群は、対応する未移行項目を新式へ移し、参照が0件になったファイルから削除する。
旧機能を先に削除してから作り直すことはしない。
