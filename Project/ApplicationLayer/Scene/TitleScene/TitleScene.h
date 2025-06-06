#pragma once
#include <Sprite.h>
#include <TextureManager.h>
#include <SRVManager.h>
#include "Input.h"
#include <BaseScene.h>


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class ImGuiManager;


/// -------------------------------------------------------------
///					　ゲームタイトルシーンクラス
/// -------------------------------------------------------------
class TitleScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 3Dオブジェクトの描画
	void Draw3DObjects() override;

	// 2Dオブジェクトの描画
	void Draw2DSprites() override;

	// 終了処理
	void Finalize() override;

	void DrawImGui() override;

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	TextureManager* textureManager = nullptr;
	Input* input = nullptr;

	std::vector<std::unique_ptr<Sprite>> sprites_;

	// テクスチャのパスをリストで管理
	std::vector<std::string> texturePaths_;

};

