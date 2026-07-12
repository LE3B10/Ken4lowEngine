# SceneDefinition Bindings

## Level binding

`SceneDefinition.Level` が空でない場合、`BaseScene::StartLoad()` は `GetSceneActorWorld()` が返す `ActorWorld` へ Ken4lowLevel JSON を読み込みます。

- Actor / Component
- Actor間の親子関係
- Lighting / Shadow / Global Punctual Lights
- Debug EditorではOutlinerの表示・ロック・Folder

既存Sceneで `StartLoad()` をoverrideしている場合は、override内の先頭で `BaseScene::StartLoad()` を呼び出します。Levelを持たない既存Sceneは従来の初期化結果を維持します。

## Validation scene

Debug Buildでは次のScene IDで、SceneDefinitionからのLevel自動読込を確認できます。

```text
LevelBindingTestScene
```

このSceneは `DebugScene` クラスを再利用し、`Resources/JSON/Levels/DebugSceneLevel.json` を読み込みます。

確認対象は次のとおりです。

- `LevelLoadedTestActor` と `LevelLoadedGround` の2ActorがOutlinerへ表示される
- 両Actorが `Validation` Folderへ復元される
- TestActorのCamera / Rigidbody / LightComponentが復元される
- PlayでRigidbodyが動作し、StopでLevel読込直後のEditor Worldへ戻る
- 通常の `DebugScene` はLevelが空なので従来のC++初期配置を維持する
