#pragma once
#include "Camera.h"

// 複数のカメラを管理するマネージャ

/// -------------------------------------------------------------
///					　カメラマネージャークラス
/// -------------------------------------------------------------
class CameraManager
{

public:	/// ---------- セッタ ---------- ///

	// デフォルトカメラを取得
	void SetDefaultCamera(Camera* defaultCamera) { defaultCamera_ = defaultCamera; }

public:	/// ---------- ゲッタ ---------- ///

	// デフォルトカメラを取得
	Camera* GetDefaultCamera() const { return defaultCamera_; }

private:
	Camera* defaultCamera_ = nullptr;


};

