# SceneDefinition Bindings

## Level binding

`SceneDefinition.Level` が空でない場合、`BaseScene::StartLoad()` は `GetSceneActorWorld()` が返す `ActorWorld` へ Ken4lowLevel JSON を読み込みます。

- Actor / Component
- Actor間の親子関係
- Lighting / Shadow / Global Punctual Lights
- Debug EditorではOutlinerの表示・ロック・Folder

既存Sceneで `StartLoad()` をoverrideしている場合は、必要に応じてoverride内の先頭で `BaseScene::StartLoad()` を呼び出します。

## Validation scene

Debug Buildでは次のScene IDで空Level読込を確認できます。

```text
LevelBindingTestScene
```

このSceneは `DebugScene` クラスを再利用し、`Resources/JSON/Levels/EmptyLevel.json` を読み込みます。
