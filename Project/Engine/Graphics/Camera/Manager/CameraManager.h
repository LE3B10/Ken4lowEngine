#pragma once
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

	public: /// ---------- セッタ ---------- ///

		// 登録
		void SetMainCamera(Camera* camera);
		void SetRenderCamera(Camera* camera);
		void SetGameCamera(Camera* camera);
		void SetDebugCamera(DebugCamera* camera);
		void SetUseDebugCamera(bool useDebugCamera);

	public: /// ---------- ゲッタ ---------- ///

		Camera* GetMainCamera() const { return mainCamera_; }
		Camera* GetRenderCamera() const { return renderCamera_ ? renderCamera_ : gameCamera_; }
		Camera* GetGameCamera() const { return gameCamera_; }
		DebugCamera* GetDebugCamera() const { return debugCamera_; }

		bool IsUsingDebugCamera() const { return useDebugCamera_; }

		Matrix4x4 GetActiveViewMatrix() const;
		Matrix4x4 GetActiveProjectionMatrix() const;
		Matrix4x4 GetActiveViewProjectionMatrix() const;
		Vector3 GetActiveCameraPosition() const;

		
	private: /// ---------- コピー禁止 ---------- ///

		CameraManager() = default;
		~CameraManager() = default;
		CameraManager(const CameraManager&) = delete;
		CameraManager& operator=(const CameraManager&) = delete;

	private: /// ---------- メンバ変数 ---------- ///

		Camera* mainCamera_ = nullptr;
		Camera* gameCamera_ = nullptr;
		Camera* renderCamera_ = nullptr;
		DebugCamera* debugCamera_ = nullptr;
		bool useDebugCamera_ = false;
	};
}