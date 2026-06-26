#pragma once
#include "SceneComponent.h"
#include "Object3D.h"

#include <memory>
#include <string>
#include <string_view>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class Camera;

	/// -------------------------------------------------------------
	/// Actorに3Dモデル描画機能を追加するComponentクラス。
	/// -------------------------------------------------------------
	class ModelComponent : public SceneComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ModelComponentの初期化処理。
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// ModelComponentの1フレーム更新処理。
		/// </summary>
		void Update(float deltaTime) override;

		/// <summary>
		/// ModelComponentの通常描画処理。
		/// </summary>
		void Draw() override;

		/// <summary>
		/// ModelComponentのシャドウ描画処理。
		/// </summary>
		void DrawShadow() override;

		/// <summary>
		/// ModelComponentのImGui描画処理。
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// ModelComponentの終了処理。
		/// </summary>
		void Finalize() override;

	public: /// ---------- 設定処理 ---------- ///

		/// <summary>
		/// 描画するモデルファイルを設定する。
		/// </summary>
		void SetModelPath(std::string_view modelPath);

		/// <summary>
		/// 描画時に使用するカメラを設定する。
		/// </summary>
		void SetCamera(Camera* camera);

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// OwnerActorのTransformComponentからObject3DへTransformを同期する。
		/// </summary>
		void SyncTransformToObject3D();

	private: /// ---------- メンバ変数 ---------- ///

		std::unique_ptr<Object3D> object3D_; // 実際の3Dモデル描画を担当するObject3D。
		std::string modelPath_;              // Object3Dに読み込ませるモデルファイルパス。
		Camera* camera_ = nullptr;           // 描画に使用するカメラ。所有権は持たない。
	};
}