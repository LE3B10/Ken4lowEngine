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
			hash ^= static_cast<unsigned char>(c);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	struct EditorTransform
	{
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 rotation = { 0.0f, 0.0f, 0.0f };
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
	};

	enum class EditorObjectKind
	{
		Generic,
		Actor,
		Component,
		Instance, // GPU Instancing内の1要素を独立したEditor選択対象として扱う。
	};

	enum class EditorInspectorType
	{
		None,
		Transform,
		PunctualLights,
		FadeManager,
		StageSelectTextLayout,
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

	struct EditorObjectInfo
	{
		using ReadTransformFunc = std::function<bool(EditorTransform&)>;
		using WriteTransformFunc = std::function<void(const EditorTransform&)>;
		using DrawInspectorFunc = std::function<void()>;
		using DrawObjectIdFunc = std::function<void(uint32_t)>;
		using BuildObjectIdEntryFunc = std::function<bool(uint32_t, EditorObjectInfo&)>;
		using ReadActiveFunc = std::function<bool()>;
		using WriteActiveFunc = std::function<void(bool)>;
		using RenameFunc = std::function<void(std::string_view)>;
		using RequestActionFunc = std::function<void()>;

		EditorObjectInfo() = default;
		EditorObjectInfo(uint64_t objectId, std::string objectDisplayName, std::string objectTypeName, std::string objectSceneName)
			: id(objectId), displayName(std::move(objectDisplayName)), typeName(std::move(objectTypeName)), sceneName(std::move(objectSceneName)) {}

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
		uint32_t objectIdSpan = 1;
		BuildObjectIdEntryFunc buildObjectIdEntry;

		bool TryReadTransform(EditorTransform& outTransform) const
		{
			return canEditTransform && readTransform && readTransform(outTransform);
		}

		void WriteTransform(const EditorTransform& transform) const
		{
			if (canEditTransform && writeTransform) writeTransform(transform);
		}

		bool TryReadWorldTransform(EditorTransform& outTransform) const
		{
			if (!canEditTransform) return false;
			return readWorldTransform ? readWorldTransform(outTransform) : TryReadTransform(outTransform);
		}

		void WriteWorldTransform(const EditorTransform& transform) const
		{
			if (!canEditTransform) return;
			if (writeWorldTransform)
			{
				writeWorldTransform(transform);
				return;
			}
			WriteTransform(transform);
		}

		bool ReadActive(bool& outActive) const
		{
			if (!canToggleActive || !readActive) return false;
			outActive = readActive();
			return true;
		}

		void WriteActive(bool active) const
		{
			if (canToggleActive && writeActive) writeActive(active);
		}

		void Rename(std::string_view name) const
		{
			if (canRename && rename && !name.empty()) rename(name);
		}

		void RequestDuplicate() const
		{
			if (canDuplicate && requestDuplicate) requestDuplicate();
		}

		void RequestDelete() const
		{
			if (canDelete && requestDelete) requestDelete();
		}

		void DrawObjectId(uint32_t objectId) const
		{
			if (canDrawObjectId && drawObjectId && objectId != 0) drawObjectId(objectId);
		}

		bool BuildObjectIdEntry(uint32_t offset, EditorObjectInfo& outEntry) const
		{
			if (buildObjectIdEntry)
			{
				return buildObjectIdEntry(offset, outEntry);
			}
			if (offset != 0) return false;
			outEntry = *this;
			return true;
		}
	};
} // namespace Ken4lowEngine
