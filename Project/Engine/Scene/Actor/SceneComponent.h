#pragma once
#include "ActorComponent.h"
#include "Vector3.h"

#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// 位置・回転・スケールと親子関係を持つComponentクラス。
	/// -------------------------------------------------------------
	class SceneComponent : public ActorComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// SceneComponentの初期化処理。
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// SceneComponentの1フレーム更新処理。
		/// </summary>
		void Update(float deltaTime) override;

		/// <summary>
		/// SceneComponentのImGui描画処理。
		/// </summary>
		void DrawImGui() override;

	public: /// ---------- 親子関係 ---------- ///

		/// <summary>
		/// 指定したSceneComponentの子として接続する。
		/// </summary>
		void AttachTo(SceneComponent* parent);

		/// <summary>
		/// 現在の親Componentから切り離す。
		/// </summary>
		void Detach();

		/// <summary>
		/// 親SceneComponentを取得する。
		/// </summary>
		SceneComponent* GetParent() const { return parent_; }

		/// <summary>
		/// 子SceneComponent一覧を取得する。
		/// </summary>
		const std::vector<SceneComponent*>& GetChildren() const { return children_; }

	public: /// ---------- Transform Getter ---------- ///

		/// <summary>
		/// 親から見たローカル位置を取得する。
		/// </summary>
		const Vector3& GetLocalPosition() const { return localPosition_; }

		/// <summary>
		/// 親から見たローカル回転を取得する。
		/// </summary>
		const Vector3& GetLocalRotation() const { return localRotation_; }

		/// <summary>
		/// 親から見たローカルスケールを取得する。
		/// </summary>
		const Vector3& GetLocalScale() const { return localScale_; }

		/// <summary>
		/// ワールド位置を取得する。
		/// </summary>
		const Vector3& GetWorldPosition() const { return worldPosition_; }

		/// <summary>
		/// ワールド回転を取得する。
		/// </summary>
		const Vector3& GetWorldRotation() const { return worldRotation_; }

		/// <summary>
		/// ワールドスケールを取得する。
		/// </summary>
		const Vector3& GetWorldScale() const { return worldScale_; }

	public: /// ---------- Transform Setter ---------- ///

		/// <summary>
		/// 親から見たローカル位置を設定する。
		/// </summary>
		void SetLocalPosition(const Vector3& position)
		{
			localPosition_ = position; // 親を基準にした相対位置を更新する。
		}

		/// <summary>
		/// 親から見たローカル回転を設定する。
		/// </summary>
		void SetLocalRotation(const Vector3& rotation)
		{
			localRotation_ = rotation; // 親を基準にした相対回転を更新する。
		}

		/// <summary>
		/// 親から見たローカルスケールを設定する。
		/// </summary>
		void SetLocalScale(const Vector3& scale)
		{
			localScale_ = scale; // 親を基準にした相対スケールを更新する。
		}

	public: /// ---------- Mutable Access ---------- ///

		/// <summary>
		/// ローカル位置を直接編集するための参照を取得する。
		/// </summary>
		Vector3& LocalPosition() { return localPosition_; }

		/// <summary>
		/// ローカル回転を直接編集するための参照を取得する。
		/// </summary>
		Vector3& LocalRotation() { return localRotation_; }

		/// <summary>
		/// ローカルスケールを直接編集するための参照を取得する。
		/// </summary>
		Vector3& LocalScale() { return localScale_; }

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// ローカルTransformからワールドTransformを計算する。
		/// </summary>
		void UpdateWorldTransform();

		/// <summary>
		/// 子Componentから指定したComponentを取り除く。
		/// </summary>
		void RemoveChild(SceneComponent* child);

	private: /// ---------- メンバ変数 ---------- ///

		// 親Component。所有権は持たない
		SceneComponent* parent_ = nullptr;

		// 子Component一覧。所有権はActor側のComponent管理に任せる
		std::vector<SceneComponent*> children_;

		Vector3 localPosition_{ 0.0f, 0.0f, 0.0f }; // 親から見た相対位置。
		Vector3 localRotation_{ 0.0f, 0.0f, 0.0f }; // 親から見た相対回転。
		Vector3 localScale_{ 1.0f, 1.0f, 1.0f };    // 親から見た相対スケール。

		Vector3 worldPosition_{ 0.0f, 0.0f, 0.0f }; // 最終的なワールド位置。
		Vector3 worldRotation_{ 0.0f, 0.0f, 0.0f }; // 最終的なワールド回転。
		Vector3 worldScale_{ 1.0f, 1.0f, 1.0f };    // 最終的なワールドスケール。
	};
}