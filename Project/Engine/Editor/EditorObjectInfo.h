#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "Vector3.h"

namespace Ken4lowEngine
{

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
	/// World OutlinerとDetailsへ安全に渡すための軽量なエディタ表示用オブジェクト情報です。
	/// </summary>
	struct EditorObjectInfo
	{
		using ReadTransformFunc = std::function<bool(EditorTransform&)>;
		using WriteTransformFunc = std::function<void(const EditorTransform&)>;

		uint64_t id = 0;
		std::string displayName;
		std::string typeName;
		std::string sceneName;
		bool canEditTransform = false;
		std::string transformUnavailableReason = "Transform editing is not available for this object.";
		ReadTransformFunc readTransform;
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
