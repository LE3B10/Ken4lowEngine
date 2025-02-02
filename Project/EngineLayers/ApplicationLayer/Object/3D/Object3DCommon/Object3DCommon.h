#pragma once
#include "Camera.h"


class Object3DCommon
{
public: /// ---------- メンバ関数 ---------- ///

public:	/// ---------- セッタ ---------- ///

	// シングルトンインスタンス
	static Object3DCommon* GetInstance();

	// デフォルトカメラを取得
	void SetDefaultCamera(Camera* defaultCamera) { defaultCamera_ = defaultCamera; }

public:	/// ---------- ゲッタ ---------- ///

	// デフォルトカメラを取得
	Camera* GetDefaultCamera() const { return defaultCamera_; }

private:
	Camera* defaultCamera_ = nullptr;

};

