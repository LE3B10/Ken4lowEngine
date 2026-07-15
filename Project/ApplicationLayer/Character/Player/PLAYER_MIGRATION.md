# Player Migration Status

## 方針

- GamePlayの正本は `PlayerActor` / `IPlayerRuntime` とする。
- 旧 `Player` は新式が実戦で安定するまで比較用に残したが、P13以降はGamePlayから生成・更新しない。
- 旧実装の削除は、対応する新式機能の実戦確認と参照0件を確認してから行う。
- 旧ファイルを残していても、GamePlay実行経路から外れているものは `Legacy / 未使用` として扱う。

## 完了済み

- InputSnapshot共通化
- WASD / Mouse Look
- ADS感度低下
- Sprint
- Jump
- Blink
- Collider / Rigidbody / Grounded
- Camera / ADS FOV / Sprint FOV
- HP / Damage / Heal
- 死亡時の入力・移動・武器停止
- 死亡Camera傾き演出
- GameOverReady / Retry
- Primary Weaponの射撃
- SEMI / AUTO
- Reload
- Reload中の移動減速
- Magazine / Reserve Ammo
- TutorialのMove / Shoot / Reload制限
- Tutorial進行
- Item取得のRuntime接続
- GamePlay実ステージ投入
- CharacterWorld所有ActorWorld / PhysicsWorldへのPlayerActor所有統一
- Pause / Result UIの入力復旧

## P13でGamePlay経路から外す旧式

- `CharacterWorld::player_` の旧 `Player` 所有
- Legacy Player ProxyのTransform同期
- Legacy Player ProxyのHP同期
- Legacy Player ProxyのAmmo / Reload同期
- Legacy Player死亡シーケンスをGameOver判定に使う経路
- Enemy / Boss / Item / Tutorialから具象 `Player*` を参照する経路
- GamePlay HUDが旧 `Player` を直接参照する経路

## まだ新式へ完全移行していない機能

### Player Gameplay

- Ladderの完全Parity
- Melee攻撃の実Hit判定・演出・武器切替連携
- Fall Damage / Fall Deathの旧式Parity
- Damage Knockback / 死亡時の吹き飛び物理
- 旧Playerの詳細な被弾方向演出

### Weapon / Inventory

- Primary以外の武器カテゴリ完全Parity
- 旧WeaponMaster / WeaponRuntimeSystem由来の全パラメータ
- 旧WeaponSlotの6スロット完全Parity
- 武器固有Reticle設定
- NoAmmo UIの完全Parity
- Weapon View Animation / Reload Animationの完全Parity
- 武器切替時の旧VFX / SEの完全Parity

### HUD / Presentation

- 旧HUDManagerのPlayer専用高度表示
  - 武器固有Reticle
  - WeaponSlot詳細
  - NoAmmo UI
  - 旧ReloadCircleの全武器対応
- 新Player HUDの最終デザイン統一
- 旧PlayerVfxの全演出Parity

### Audio

- 旧PlayerのFire / Reload / Hit / Death SEの全イベント移行
- 武器ごとのSE差し替え

### Cleanup

- `Player.cpp / Player.h` と旧Player Component群の参照0確認
- 未使用になった `PlayerDeathComponent` など旧Componentの削除
- `GamePlayPlayerMigrationRuntime` の名称変更または通常Runtime Controllerへの昇格
- `PlayerTutorialRestrictionBridge` をTutorial APIへ正式統合
- Debug用 `TestActor / DebugPlayer.json` の最終整理

## 削除条件

各旧処理は以下をすべて満たしてから削除する。

1. 新式に同等機能がある。
2. DebugSceneで確認済み。
3. GamePlay実ステージで確認済み。
4. Retry / Scene遷移後も正常。
5. 旧処理への参照が0件。
