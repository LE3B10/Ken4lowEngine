#pragma once
#include <AbstractSceneFactory.h>
#include <SceneManager.h>
#include <Camera.h>
#include <ImGuiManager.h>


/// ---------- 前方宣言 ---------- ///
class WinApp;
class DirectXCommon;


/// -------------------------------------------------------------
///				　			ゲーム全体
/// -------------------------------------------------------------
class Framework
{
public: /// ---------- メンバ変数 ---------- ///

	// 実行処理
	void Run();

public: /// ---------- 仮想メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~Framework() = default;

	// 初期化処理
	virtual void Initialize();

	// 更新処理
	virtual void Update();

	// 描画処理
	virtual void Draw() = 0;

	// 終了処理
	virtual void Finalize();

	// 終了チェック
	virtual bool IsEndRequest() { return endRequest_; }

protected: /// ---------- メンバ変数 ---------- ///

	// ウィンドウアプリケーション
	WinApp* winApp_ = nullptr;

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// カメラ
	std::unique_ptr<Camera> defaultCamera_;

	// シーンマネージャー
	std::unique_ptr<SceneManager> sceneManager_;

	// シーンファクトリー
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;

	// 終了リクエスト
	bool endRequest_ = false;
};

