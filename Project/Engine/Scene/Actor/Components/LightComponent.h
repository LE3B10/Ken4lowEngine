#pragma once
#include "SceneComponent.h"

#include "Vector3.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///			Actorにライト情報を追加するComponentクラス
	/// -------------------------------------------------------------
	class LightComponent : public SceneComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// LightComponentのImGui描画処理
		/// </summary>
		void DrawImGui() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentのクラス種別を取得する
		/// </summary>
		std::string GetClassTypeName() const override
		{
			return "LightComponent"; // LightComponentとして保存する
		}

		/// <summary>
		/// LightComponent固有情報をJSONへ保存する
		/// </summary>
		void ToJson(nlohmann::json& outJson) const override;

		/// <summary>
		/// JSONからLightComponent固有情報を復元する
		/// </summary>
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定取得 ---------- ///

		const Vector3& GetColor() const { return color_; }
		void SetColor(const Vector3& color) { color_ = color; }

		float GetIntensity() const { return intensity_; }
		void SetIntensity(float intensity) { intensity_ = intensity; }

		float GetRange() const { return range_; }
		void SetRange(float range) { range_ = range; }

		bool IsEnabled() const { return enabled_; }
		void SetEnabled(bool enabled) { enabled_ = enabled; }

	private: /// ---------- メンバ変数 ---------- ///

		Vector3 color_ = { 1.0f, 1.0f, 1.0f };
		float intensity_ = 1.0f;
		float range_ = 10.0f;
		bool enabled_ = true;
	};
}
