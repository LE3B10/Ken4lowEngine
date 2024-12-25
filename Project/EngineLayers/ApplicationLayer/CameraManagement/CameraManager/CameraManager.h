#pragma once
#include <Camera.h>
#include <Transform.h>
#include <unordered_map>
#include <string>
#include <memory>


/// -------------------------------------------------------------
///				複数のカメラを管理するクラス
/// -------------------------------------------------------------
class CameraManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 更新処理
	void Update(float deltaTime);

	// カメラを追加
	void AddCamera(const std::string& name, std::unique_ptr<Camera> camera);

	// カメラの挙動を設定
	void SetActiveCamera(const std::string& name);

	// ゲッタ
	Camera* GetActiveCamera() const { return activeCamera_; }
	Camera* GetCamera(const std::string& name) const;
	

private: /// ---------- メンバ変数 ---------- ///

	std::unordered_map<std::string, std::unique_ptr<Camera>> cameras_;
	Camera* activeCamera_ = nullptr; // 現在有効なカメラ	
};

