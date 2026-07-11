#pragma once

#include <Editor/EditorObjectInfo.h>

#include <vector>

namespace Ken4lowEngine
{
	class ActorWorld;
	class SceneManager;

	/// -------------------------------------------------------------
	///　　　　　　　　　　シーンの基底クラス
	/// -------------------------------------------------------------
	enum class EditorInputPolicy
	{
		UiMouse,
		FpsCapture,
	};

	class BaseScene
	{
	public:
		virtual ~BaseScene() = default;
		virtual void Initialize() = 0;
		virtual void Update() = 0;

		/// <summary>Editor Mode中にゲーム進行を止めたまま確認用更新だけ行う。</summary>
		virtual void UpdateEditor(float /*deltaTime*/) {}

		/// <summary>EditorのPlay開始直前に編集状態を保存する。</summary>
		virtual void BeginEditorPlay() {}

		/// <summary>EditorのStop時にPlay前の編集状態へ戻す。</summary>
		virtual void EndEditorPlay() {} // Play用の一時変化をEdit Worldへ残さないための復元入口。

		virtual void PrepareShadowPass() {}
		virtual void Draw3DObjects() = 0;
		virtual void DrawShadowObjects() = 0;
		virtual void Draw2DSprites() = 0;
		virtual void Finalize() = 0;
		virtual void DrawImGui() = 0;
		virtual EditorInputPolicy GetEditorInputPolicy() const { return EditorInputPolicy::UiMouse; }
		virtual void CollectEditorObjects(std::vector<Ken4lowEngine::EditorObjectInfo>& /*outObjects*/) {}
		virtual ActorWorld* GetEditorActorWorld() { return nullptr; }
		virtual void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }
		virtual void StartLoad() {};
		virtual void UpdateLoad() {};
		virtual void StartUnload() {};
		virtual void UpdateUnload() {};
		virtual bool IsReadyToStartUncover() const { return true; }
		virtual bool IsReadyToSwapOut() const { return true; }

	protected:
		SceneManager* sceneManager_ = nullptr;
	};
} // namespace Ken4lowEngine
