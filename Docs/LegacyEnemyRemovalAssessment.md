# 旧Enemy削除前の確認事項

## 通常ゲームでの生成状況

`fps_stage00.json` の最初のウェーブでは、段階置き換え確認用として近接雑魚敵を3体、中距離雑魚敵を2体生成する。残り7体は `enemyType` 未指定のため、互換動作として旧Enemyを生成する。

| stage JSON | EnemySpawnPoint数 | `enemyType` 指定済み | `enemyType` 未指定（旧Enemy生成） |
| --- | ---: | ---: | ---: |
| `fps_stage00.json` | 12 | 5 | 7 |
| `fps_stage01.json` | 24 | 0 | 24 |
| `fps_stage02.json` | 21 | 0 | 21 |
| `fps_stage03.json` | 0 | 0 | 0 |
| `fps_stage04.json` | 0 | 0 | 0 |

`enemyType` が未指定、未知の文字列、または `Legacy` の場合は、既存stage JSONとの互換性を維持するため旧Enemyを生成する。

## 旧Enemyがまだ必要な理由

- `fps_stage00.json` の未移行7スポーン、`fps_stage01.json` の24スポーン、`fps_stage02.json` の21スポーンが旧Enemyへフォールバックする。
- 旧Enemyは従来の銃撃雑魚敵として、通常弾の生成、遮蔽利用、射線判定、索敵、後退、回避、スタック解消をまとめて担当している。
- 中距離雑魚敵は爆弾投射を担当する別アーキタイプであり、旧Enemyの通常銃撃雑魚敵をそのまま置換するものではない。

## 旧Enemyでしかできない処理

- `BulletManager` を利用した通常敵弾の射撃。
- `CollisionManager` のワールドSegmentCastを利用した射線判定。
- 旧State群と連携した Idle / CombatMove / Shoot / Search / Dead の状態遷移。
- 遮蔽選択、低HP時の後退、回避、スタック解消など、旧Enemy内に統合されている戦術行動。

## MeleeEnemy / MidRangeEnemy または新アーキタイプへ移植が必要な処理

- 旧Enemy相当の銃撃雑魚敵を残す場合、通常敵弾、射線判定、索敵、遮蔽、後退、回避、スタック解消を新しい責務へ移す必要がある。
- stage JSONの `archetype` と `enemyType` の役割を整理し、必要なら銃撃雑魚敵用の新しい `enemyType` を追加する。
- `CharacterWorld::InjectEnemyDeps` に残る旧Enemy固有の `BulletManager` / `CollisionManager` 注入を、新アーキタイプ側へ移す。

## 削除できそうなEnemyState群

以下は旧Enemyからのみ参照されるため、旧Enemyの生成がゼロになり、旧銃撃処理の移植が完了した後はまとめて削除候補になる。

- `IEnemyState`
- `EnemyIdleState`
- `EnemyCombatMoveState`
- `EnemyShootState`
- `EnemySearchState`
- `EnemyDeadState`

## まだ削除してはいけないEnemyState群

現時点では上記State群を削除してはいけない。通常stage JSONに未指定スポーンが残っており、旧EnemyがこれらのState群を実行するためである。

## 次の削除ステップ

1. Enemy Debugの「旧Enemy数」が通常プレイでゼロになるよう、未指定スポーンを段階的に移行する。
2. 従来の銃撃雑魚敵を廃止するか、新アーキタイプとして再実装するか決定する。
3. `CharacterWorld::InjectEnemyDeps` の旧Enemy分岐を削除する。
4. `EnemyFactory` の `Legacy` 生成と互換フォールバックの廃止時期を決定する。
5. 旧Enemyと旧State群をまとめて削除する。
