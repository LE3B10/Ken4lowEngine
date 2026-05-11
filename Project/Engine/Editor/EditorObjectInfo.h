#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "Vector3.h"

namespace Ken4lowEngine
{

	inline uint64_t MakeStableEditorObjectId(std::string_view path)
	{
		uint64_t hash = 14695981039346656037ull;
		for (const char c : path)
		{
			// 固定パス文字列からFNV-1aでIDを作り、フレームごとのアドレス変化へ依存しないようにする。
			hash ^= static_cast<unsigned char>(c);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	/// <summary>
	/// Detailsで表示・編集するための汎用Transformスナップショットです。
	/// </summary>
	struct EditorTransform
	{
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 rotation = { 0.0f, 0.0f, 0.0f };
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
	};

	enum class EditorInspectorType
	{
		None,
		Transform,
		PunctualLights,
		FadeManager,
		StageSelectTextLayout,
		// GamePlaySceneの主要項目をDetailsで見分け、安全な概要Inspectorへ振り分ける。
		PlayerInfo,
		EnemyManagerInfo,
		BulletManagerInfo,
		WaveManagerInfo,
		StageInfo,
		HudInfo,
		CollisionManagerInfo,
		ManagerInfo,
	};

	inline const char* ToString(EditorInspectorType type)
	{
		switch (type)
		{
		case EditorInspectorType::None: return "None";
		case EditorInspectorType::Transform: return "Transform";
		case EditorInspectorType::PunctualLights: return "PunctualLights";
		case EditorInspectorType::FadeManager: return "FadeManager";
		case EditorInspectorType::StageSelectTextLayout: return "StageSelectTextLayout";
		case EditorInspectorType::PlayerInfo: return "PlayerInfo";
		case EditorInspectorType::EnemyManagerInfo: return "EnemyManagerInfo";
		case EditorInspectorType::BulletManagerInfo: return "BulletManagerInfo";
		case EditorInspectorType::WaveManagerInfo: return "WaveManagerInfo";
		case EditorInspectorType::StageInfo: return "StageInfo";
		case EditorInspectorType::HudInfo: return "HudInfo";
		case EditorInspectorType::CollisionManagerInfo: return "CollisionManagerInfo";
		case EditorInspectorType::ManagerInfo: return "ManagerInfo";
		default: return "Unknown";
		}
	}


	/// <summary>
	/// World OutlinerとDetailsへ安全に渡すための軽量なエディタ表示用オブジェクト情報です。
	/// </summary>
	struct EditorObjectInfo
	{
		using ReadTransformFunc = std::function<bool(EditorTransform&)>;
		using WriteTransformFunc = std::function<void(const EditorTransform&)>;
		using DrawInspectorFunc = std::function<void()>;

		uint64_t id = 0;
		std::string displayName;
		std::string typeName;
		std::string sceneName;
		bool canEditTransform = false;
		std::string transformUnavailableReason = "Transform editing is not available for this object.";
		EditorInspectorType inspectorType = EditorInspectorType::None;
		std::string inspectorHint;
		ReadTransformFunc readTransform;
		DrawInspectorFunc drawInspector;
		WriteTransformFunc writeTransform;

		// Detailsは毎フレーム再収集された入口だけを使い、古いraw pointerへ直接触れないようにする。
		bool TryReadTransform(EditorTransform& outTransform) const
		{
			return canEditTransform && readTransform && readTransform(outTransform);
		}

		// Transform反映はScene側が用意した安全な書き戻し関数へ集約する。
		void WriteTransform(const EditorTransform& transform) const
		{
			if (canEditTransform && writeTransform)
			{
				writeTransform(transform);
			}
		}
	};

} // namespace Ken4lowEngine
