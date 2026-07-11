#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

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

	/// <summary>
	/// World Outliner上でオブジェクトを分類する種類です。
	/// </summary>
	enum class EditorObjectKind
	{
		Generic,
		Actor,
		Component,
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
		using DrawObjectIdFunc = std::function<void(uint32_t)>;
		using ReadActiveFunc = std::function<bool()>;
		using WriteActiveFunc = std::function<void(bool)>;
		using RenameFunc = std::function<void(std::string_view)>;
		using RequestActionFunc = std::function<void()>;

		EditorObjectInfo() = default;

		EditorObjectInfo(uint64_t objectId, std::string objectDisplayName, std::string objectTypeName, std::string objectSceneName)
			: id(objectId),
			displayName(std::move(objectDisplayName)),
			typeName(std::move(objectTypeName)),
			sceneName(std::move(objectSceneName))
		{
			// 階層用フィールド追加後も既存の4引数初期化を型安全に維持する。
		}

		uint64_t id = 0;
		uint64_t parentId = 0;
		int sortOrder = 0;
		std::string displayName;
		std::string typeName;
		std::string sceneName;
		std::string icon = "[O]";
		EditorObjectKind objectKind = EditorObjectKind::Generic;
		bool isRootComponent = false;

		bool canEditTransform = false;
		std::string transformUnavailableReason = "Transform editing is not available for this object.";
		EditorInspectorType inspectorType = EditorInspectorType::None;
		std::string inspectorHint;
		ReadTransformFunc readTransform;
		DrawInspectorFunc drawInspector;
		WriteTransformFunc writeTransform;
		ReadTransformFunc readWorldTransform;
		WriteTransformFunc writeWorldTransform;

		bool canToggleActive = false;
		ReadActiveFunc readActive;
		WriteActiveFunc writeActive;
		bool canRename = false;
		RenameFunc rename;
		bool canDuplicate = false;
		RequestActionFunc requestDuplicate;
		bool canDelete = false;
		RequestActionFunc requestDelete;

		bool canDrawObjectId = false;
		DrawObjectIdFunc drawObjectId;

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

		// GizmoはViewport上のWorld座標を扱う。専用入口がない既存オブジェクトはLocal入口をWorldとして再利用する。
		bool TryReadWorldTransform(EditorTransform& outTransform) const
		{
			if (!canEditTransform)
			{
				return false;
			}
			return readWorldTransform ? readWorldTransform(outTransform) : TryReadTransform(outTransform);
		}

		// SceneComponent階層はWorldからLocalへ戻す必要があるため、Detailsの書き戻しとは入口を分ける。
		void WriteWorldTransform(const EditorTransform& transform) const
		{
			if (!canEditTransform)
			{
				return;
			}
			if (writeWorldTransform)
			{
				writeWorldTransform(transform);
				return;
			}
			WriteTransform(transform);
		}

		bool ReadActive(bool& outActive) const
		{
			if (!canToggleActive || !readActive)
			{
				return false;
			}
			outActive = readActive();
			return true;
		}

		void WriteActive(bool active) const
		{
			if (canToggleActive && writeActive)
			{
				writeActive(active);
			}
		}

		void Rename(std::string_view name) const
		{
			if (canRename && rename && !name.empty())
			{
				rename(name);
			}
		}

		void RequestDuplicate() const
		{
			if (canDuplicate && requestDuplicate)
			{
				requestDuplicate();
			}
		}

		void RequestDelete() const
		{
			if (canDelete && requestDelete)
			{
				requestDelete();
			}
		}

		void DrawObjectId(uint32_t objectId) const
		{
			if (canDrawObjectId && drawObjectId && objectId != 0)
			{
				drawObjectId(objectId); // 0は空ピクセル専用IDとして予約する。
			}
		}
	};

} // namespace Ken4lowEngine
