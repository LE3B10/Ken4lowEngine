#define NOMINMAX
#include "DebugScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include "SkyBoxManager.h"
#include "Wireframe.h"
#include "AudioManager.h"
#include <SceneManager.h>
#include "LevelLoader.h"

#include <TextureManager.h>

#include <algorithm>
#include <chrono>

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI

namespace
{
	constexpr float kScreenW = 1280.0f;
	constexpr float kScreenH = 720.0f;

	inline double ToMs(std::chrono::high_resolution_clock::duration d)
	{
		return std::chrono::duration<double, std::milli>(d).count();
	}
}

void DebugScene::BuildTextureListIfEmpty_()
{
	if (!texturePaths_.empty()) return;

	// 生成テクスチャ256個
	texturePaths_.reserve(256);
	for (int i = 0; i < 256; ++i) {
		char key[64];
		sprintf_s(key, "Generated/Color_%03d", i);

		// 適当に色を散らす（例：r,g,b を回す）
		uint8_t r = (uint8_t)i;
		uint8_t g = (uint8_t)(i * 73);
		uint8_t b = (uint8_t)(i * 151);

		TextureManager::GetInstance()->CreateSolidColorTexture(key, r, g, b, 255, 64, 64);
		texturePaths_.push_back(key);
	}
}

void DebugScene::RebuildSprites_()
{
	BuildTextureListIfEmpty_();

	// uniqueTextureCount_ を安全に丸める
	uniqueTextureCount_ = std::clamp(uniqueTextureCount_, 1, (int)texturePaths_.size());

	sprites_.clear();
	sprites_.reserve(spriteCount_);

	// 先に使う分だけロード（TextureManagerがキャッシュしてるなら実質ノーコスト）
	for (int i = 0; i < uniqueTextureCount_; ++i) {
		TextureManager::GetInstance()->LoadTexture(texturePaths_[i]);
	}

	const int cols = std::max(1, (int)(kScreenW / std::max(1.0f, spriteSize_.x)));
	const int maxRows = std::max(1, (int)(kScreenH / std::max(1.0f, spriteSize_.y)));

	for (int i = 0; i < spriteCount_; ++i)
	{
		const int xIdx = (i % cols);
		const int yIdx = ((i / cols) % maxRows);

		const float x = xIdx * spriteSize_.x;
		const float y = yIdx * spriteSize_.y;

		auto spr = std::make_unique<Sprite>();
		spr->Initialize(texturePaths_[i % uniqueTextureCount_]);
		spr->SetAnchorPoint({ 0.0f, 0.0f });
		spr->SetSize(spriteSize_);
		spr->SetPosition({ x, y });

		// 1回はUpdateして頂点/行列を書き込む
		spr->Update();

		sprites_.push_back(std::move(spr));
	}

	rebuildRequested_ = false;
	avgDrawMs_ = 0.0;
	avgCounter_ = 0;
}

void DebugScene::Initialize()
{
	rebuildRequested_ = true;
	RebuildSprites_();
}

void DebugScene::Update()
{
	// ImGuiで値が変わったら作り直す
	if (rebuildRequested_) {
		RebuildSprites_();
	}

	// 描画負荷だけ測りたいならOFFにできるようにする
	if (updateEveryFrame_) {
		for (auto& sprite : sprites_) {
			sprite->Update();
		}
	}
}

void DebugScene::Draw3DObjects()
{


#ifdef _DEBUG

#endif // _DEBUG
}

void DebugScene::Draw2DSprites()
{
	auto t0 = std::chrono::high_resolution_clock::now();

	SpriteManager::GetInstance()->SetRenderSetting_Background();


	SpriteManager::GetInstance()->SetRenderSetting_UI();

	for (auto& sprite : sprites_)
	{
		sprite->Draw();
	}

	auto t1 = std::chrono::high_resolution_clock::now();
	lastDrawMs_ = ToMs(t1 - t0);

	// 移動平均っぽく表示（60フレームくらいで馴染む）
	avgDrawMs_ += lastDrawMs_;
	avgCounter_++;
}

void DebugScene::Finalize()
{
	for (auto& sprite : sprites_)
	{
		sprite->Finalize();
	}
	sprites_.clear();
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	bool changed = false;

	ImGui::Begin("Sprite Draw Load Test");

	changed |= ImGui::SliderInt("Sprite Count", &spriteCount_, 1, 20000);
	changed |= ImGui::SliderInt("Unique Textures", &uniqueTextureCount_, 1, 1024);
	changed |= ImGui::InputFloat2("Sprite Size", &spriteSize_.x);

	ImGui::Checkbox("Update Every Frame", &updateEveryFrame_);

	if (changed) {
		rebuildRequested_ = true;
	}

	if (ImGui::Button("Rebuild Now")) {
		rebuildRequested_ = true;
	}

	ImGui::Separator();
	ImGui::Text("Last Draw CPU: %.3f ms", lastDrawMs_);

	if (avgCounter_ > 0) {
		ImGui::Text("Avg  Draw CPU: %.3f ms", (avgDrawMs_ / (float)avgCounter_));
	}
	ImGui::Text("Sprites: %d", (int)sprites_.size());

	// いま texturePaths_ が1個しか入ってないので、ここも出しておくと分かりやすい
	ImGui::Text("Texture paths loaded: %d", (int)texturePaths_.size());

	ImGui::End();

#endif // USE_IMGUI

}
