#pragma once
#include "SceneComponent.h"

#include <memory>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///

	// CameraComponentが所有するCameraを描画処理に渡すための参照を返す
	class Camera;

	/// -------------------------------------------------------------
	///		  Actorにカメラ機能を追加するSceneComponentクラス
	/// -------------------------------------------------------------
	class CameraComponent : public SceneComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// CameraComponentの初期化処理
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// CameraComponentの1フレーム更新処理
		/// </summary>
		void Update(float deltaTime) override;

		/// <summary>
		/// PhysicsWorld更新後にCameraのTransformを更新する
		/// </summary>
		void PostPhysicsUpdate(float deltaTime) override;

		/// <summary>
		/// CameraComponentのImGui描画処理
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// CameraComponentの終了処理
		/// </summary>
		void Finalize() override;

	public: /// ---------- Getter ---------- ///

		/// <summary>
		/// Componentが所有しているCameraを取得する
		/// </summary>
		Camera* GetCamera() const
		{
			return camera_; // CameraManagerや描画処理に渡すため、生ポインタで参照だけ返す
		}

	public: /// ---------- 設定 ---------- ///

		/// <summary>
		/// このCameraComponentをCameraManagerのMainCameraとして登録するか設定する
		/// </summary>
		void SetAutoRegisterMainCamera(bool enabled);

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// SceneComponentのWorldTransformをCameraへ反映する
		/// </summary>
		void SyncTransformToCamera();

	private: /// ---------- メンバ変数 ---------- ///

		// CameraComponentが所有する通常Camera
		Camera* camera_ = nullptr;

		// trueならInitialize時にMainCameraへ登録する
		bool autoRegisterMainCamera_ = false;
	};
}