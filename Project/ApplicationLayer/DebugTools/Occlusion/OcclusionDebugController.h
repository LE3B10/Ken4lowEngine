#pragma once

namespace Ken4lowEngine { class Stage; }
namespace K4E = ::Ken4lowEngine;

/// <summary>
/// Lv4 簡易 Occlusion Culling の ImGui 操作と統計表示を担当する Controller。
/// </summary>
class OcclusionDebugController
{
public:
	void DrawImGui(K4E::Stage* stage);
	void DrawImGuiContent(K4E::Stage* stage);
};
