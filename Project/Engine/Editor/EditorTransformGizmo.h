#pragma once

#ifdef USE_IMGUI
namespace Ken4lowEngine
{
	/// <summary>
	/// Main Viewport上で選択Objectの移動・回転・拡縮を操作します。
	/// </summary>
	class EditorTransformGizmo
	{
	public:
		static EditorTransformGizmo* GetInstance();

		/// <summary>Viewport Toolbar描画後にショートカットとImGuizmoを更新します。</summary>
		void Draw();

		/// <summary>現在Gizmo本体をドラッグしているか返します。</summary>
		bool IsUsing() const;

		/// <summary>マウスがGizmoの軸・平面・回転リング上にあるか返します。</summary>
		bool IsOver() const;

	private:
		EditorTransformGizmo() = default;
		~EditorTransformGizmo() = default;
		EditorTransformGizmo(const EditorTransformGizmo&) = delete;
		EditorTransformGizmo& operator=(const EditorTransformGizmo&) = delete;
	};
} // namespace Ken4lowEngine
#endif // USE_IMGUI
