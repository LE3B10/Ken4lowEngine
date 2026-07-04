#pragma once
#include "Engine/System/Audio/Listener/AudioListener.h"
#include "Matrix4x4.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class Camera;
	class DebugCamera;

	class CameraManager
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// シングルトンインスタンスを取得する。
		/// </summary>
		static CameraManager* GetInstance();

		void Initialize();
		void Finalize();
		void Update();
		void UpdateAudioListener(float deltaTime);

	public: /// ---------- セッタ ---------- ///

		// 登録
		void SetMainCamera(Camera* camera);
		void SetUseDebugCamera(bool useDebugCamera);

	public: /// ---------- ゲッタ ---------- ///

		Camera* GetMainCamera() const { return mainCamera_; }
		DebugCamera* GetDebugCamera() const { return debugCamera_; }

		bool IsUsingDebugCamera() const { return useDebugCamera_; }

		Matrix4x4 GetActiveViewMatrix() const;
		Matrix4x4 GetActiveProjectionMatrix() const;
		Matrix4x4 GetActiveViewProjectionMatrix() const;
		Vector3 GetActiveCameraPosition() const;
		const AudioListener& GetAudioListener() const { return audioListener_; }

		// Object3D や FPS など「Camera* が欲しい側」用
		// DebugCamera は型が違うので、通常カメラが必要な場所では main を返す
		Camera* GetRenderCamera() const { return mainCamera_; }

	private: /// ---------- コピー禁止 ---------- ///

		CameraManager() = default;
		~CameraManager() = default;
		CameraManager(const CameraManager&) = delete;
		CameraManager& operator=(const CameraManager&) = delete;

	private: /// ---------- メンバ変数 ---------- ///

		Camera* mainCamera_ = nullptr;
		DebugCamera* debugCamera_ = nullptr;
		bool useDebugCamera_ = false;
		AudioListener audioListener_;
	};
}
