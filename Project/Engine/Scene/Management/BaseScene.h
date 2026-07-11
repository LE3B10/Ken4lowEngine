#pragma once

#include <Editor/EditorObjectInfo.h>

#include <vector>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
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
	public: /// ---------- 純粋仮想関数 ---------- ///

		// 仮想デストラクタ
		virtual ~BaseScene() = default;

		// 仮想初期化処理
		virtual void Initialize() = 0;

		// 仮想更新処理
		virtual void Update() = 0;

		// Editor Mode中にゲーム進行を止めたまま確認用更新だけ行う。
		virtual void UpdateEditor(float /*deltaTime*/) {}

		// ShadowSystemがライト種別と行列を確定する直前に、Scene側のライト情報を同期する。
		virtual void PrepareShadowPass() {}

		// 仮想3D描画処理
		virtual void Draw3DObjects() = 0;

		// 仮想シャドウマップ描画処理
		virtual void DrawShadowObjects() = 0;

		// 仮想2D描画処理
		virtual void Draw2DSprites() = 0;

		// 仮想終了処理
		virtual void Finalize() = 0;

		// ImGui描画処理
		virtual void DrawImGui() = 0;

		// UI主体のSceneはデフォルトでカーソル表示のMain Viewportクリック入力にする。
		virtual EditorInputPolicy GetEditorInputPolicy() const { return EditorInputPolicy::UiMouse; }

		// World Outliner用の安全な表示情報をScene側から収集する入口です。
		virtual void CollectEditorObjects(std::vector<Ken4lowEngine::EditorObjectInfo>& /*outObjects*/) {}

		/// <summary>
		/// ViewportへのEditor配置を受け入れるActorWorldを返します。
		/// Actor編集に対応していないSceneはnullptrのままにします。
		/// </summary>
		virtual ActorWorld* GetEditorActorWorld() { return nullptr; }

		// シーンマネージャーをセット
		virtual void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

		// ロード開始
		virtual void StartLoad() {};

		// ロード更新
		virtual void UpdateLoad() {};

		// アンロード開始
		virtual void StartUnload() {};

		// アンロード更新
		virtual void UpdateUnload() {};

		// フェードイン開始準備完了か
		virtual bool IsReadyToStartUncover() const { return true; }

		// シーン差し替え可能か
		virtual bool IsReadyToSwapOut() const { return true; }

	protected: /// ---------- メンバ変数 ---------- ///

		// シーンマネージャーを借りてくる
		SceneManager* sceneManager_ = nullptr;

	};

} // namespace Ken4lowEngine
