#pragma once
#include "SceneComponent.h"
#include <InstancedObject3DRenderer.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///   Actorにインスタンシング描画機能を追加するComponentクラス
	/// -------------------------------------------------------------
	class InstancedModelComponent : public SceneComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// InstancedModelComponentの初期化処理
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// InstancedModelComponentの1フレーム更新処理
		/// </summary>
		void Update(float deltaTime) override;

		/// <summary>
		/// PhysicsWorld更新後にインスタンス配置へ最新Transformを反映する。
		/// </summary>
		void PostPhysicsUpdate(float deltaTime) override;

		/// <summary>
		/// InstancedModelComponentの通常描画処理
		/// </summary>
		void Draw() override;

		/// <summary>
		/// InstancedModelComponentのImGui描画処理
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// InstancedModelComponentの終了処理
		/// </summary>
		void Finalize() override;

	public: /// ---------- Jsonシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentのクラス種別を取得する。
		/// </summary>
		std::string GetClassTypeName() const override
		{
			return "InstancedModelComponent"; // InstancedModelComponentとして保存する。
		}

		/// <summary>
		/// InstancedModelComponent固有情報をJSONへ保存する。
		/// </summary>
		void ToJson(nlohmann::json& outJson) const override;

	public: /// ---------- 設定処理 ---------- ///

		/// <summary>
		/// インスタンシング描画するモデルファイルを設定する
		/// </summary>
		void SetModelPath(std::string_view modelPath);

		/// <summary>
		/// 描画するインスタンス数を設定する
		/// </summary>
		void SetInstanceCount(int instanceCount);

		/// <summary>
		/// インスタンス同士の間隔を設定する
		/// </summary>
		void SetSpacing(float spacing);

		/// <summary>
		/// インスタンス全体のスケールを設定する
		/// </summary>
		void SetInstanceScale(const Vector3& scale);

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// 現在の設定からインスタンス配置を再構築する
		/// </summary>
		void RebuildInstances();

		/// <summary>
		/// 設定が変更された時に再構築を予約する。
		/// </summary>
		void RequestRebuild();

	private: /// ---------- メンバ変数 ---------- ///

		std::unique_ptr<InstancedObject3DRenderer> renderer_; // GPUインスタンシング描画を担当するRenderer
		std::string modelPath_;                               // 描画に使用するモデルファイルパス

		int instanceCount_ = 100;      // 描画するインスタンス数
		float spacing_ = 2.0f;         // インスタンス同士の間隔
		Vector3 instanceScale_{ 1.0f, 1.0f, 1.0f }; // 各インスタンスの基本スケール

		bool isRebuildRequested_ = true; // 配置再構築が必要かどうか
		bool isInitializedRenderer_ = false; // Renderer初期化済みかどうか

		Vector3 lastWorldPosition_{};		 // 前回のワールド位置
		Vector3 lastWorldRotation_{};		 // 前回のワールド回転
		Vector3 lastWorldScale_{};			 // 前回のワールドスケール
		bool hasLastWorldTransform_ = false; // 前回のワールドTransformが有効かどうか
	};
}