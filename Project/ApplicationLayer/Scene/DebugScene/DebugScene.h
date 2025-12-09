#pragma once
#include "BaseScene.h"
#include "Sprite.h"

#include <vector>
#include <memory>

/// -------------------------------------------------------------
///					　	デバッグシーン
/// -------------------------------------------------------------
class DebugScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想初期化処理
	void Initialize() override;

	// 仮想更新処理
	void Update() override;

	// 仮想3D描画処理
	void Draw3DObjects() override;

	// 仮想2D描画処理
	void Draw2DSprites() override;

	// 仮想終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- 内部メンバ関数 ---------- ///

	void RebuildSprites_();
	void BuildTextureListIfEmpty_();

private: /// ---------- メンバ変数 ---------- ///

	std::vector<std::unique_ptr<Sprite>> sprites_; // スプライトコンテナ
	std::vector<std::string> texturePaths_;	  // テクスチャパスコンテナ

	int spriteCount_ = 1024;          // 描画枚数
	int uniqueTextureCount_ = 1;      // 使うテクスチャ種類数（texturePaths_の先頭から使う）
	Vector2 spriteSize_ = { 32.0f, 32.0f };

	bool updateEveryFrame_ = false;   // trueにすると Update()で全スプライト更新（CPU負荷も増える）
	bool rebuildRequested_ = true;

	double lastDrawMs_ = 0.0;
	double avgDrawMs_ = 0.0;
	int avgCounter_ = 0;
};

