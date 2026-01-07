#define NOMINMAX
#include "FadeManager.h"

#include "SpriteManager.h"
#include "DirectXCommon.h"
#include <LinearInterpolation.h>

#include <algorithm>
#include <cmath>

// ★座標系が「画面中心が(0,0)」のエンジン向け
// もしあなたのSprite座標が左上(0,0)なら kCenterOrigin を false にする
static constexpr bool kCenterOrigin = false;

static Vector2 ScreenToSprite(const Vector2& p, float w, float h)
{
	if constexpr (kCenterOrigin)
	{
		return { p.x - w * 0.5f, p.y - h * 0.5f };
	}
	else
	{
		return p; // 左上原点の場合
	}
}

void FadeManager::Initialize()
{
	// 遷移開始時の「初回Sprite生成/Textureロード」によるヒッチを避けるため、
	// ここでタイル(Sprite)を先に生成＆初期化しておく
	EnsureTiles();

	// Spriteの内部でCB確保などが遅延されている場合に備えて一度Updateしておく（任意）
	for (auto& t : tiles_)
	{
		if (t.sp) { t.sp->Update(); }
	}
}

void FadeManager::Finalize()
{
	tiles_.clear();
	state_ = State::None;
	timer_ = 0.0f;
	minHoldSec_ = 0.0f;
	minHoldFrames_ = 0;
	holdTimer_ = 0.0f;
	holdFramesLeft_ = 0;
	InvalidateCache();
}

bool FadeManager::IsHoldMinSatisfied() const
{
	if (state_ != State::Hold) return false;
	return (holdTimer_ >= minHoldSec_) && (holdFramesLeft_ <= 0);
}

bool FadeManager::Update(float dt)
{
	if (state_ == State::None) return false;

	EnsureTiles();

	const float animSec = std::max(tileAnimSec_, 0.0001f);
	const float coverTotal = tileMaxDelay_ + animSec;

	switch (state_)
	{
	case State::TileCover:
	{
		timer_ += dt;
		if (timer_ >= coverTotal)
		{
			// 完全に閉じ切った → Hold へ
			timer_ = coverTotal; // 描画は完全被覆で固定
			state_ = State::Hold;
			holdTimer_ = 0.0f;
			holdFramesLeft_ = minHoldFrames_;
			return true; // "閉じ切り"イベント
		}
		break;
	}
	case State::Hold:
	{
		// タイルは完全被覆のまま保持
		holdTimer_ += dt;
		if (holdFramesLeft_ > 0) { --holdFramesLeft_; }
		break;
	}
	case State::TileUncover:
	{
		timer_ += dt;
		if (timer_ >= coverTotal)
		{
			state_ = State::None;
			timer_ = 0.0f;
			minHoldSec_ = 0.0f;
			minHoldFrames_ = 0;
			holdTimer_ = 0.0f;
			holdFramesLeft_ = 0;
		}
		break;
	}
	default:
		break;
	}

	return false;
}

void FadeManager::Draw2DSprites()
{
	DrawTileOverlay();
}

void FadeManager::StartCover(float minHoldSec, int minHoldFrames)
{
	EnsureTiles();
	state_ = State::TileCover;
	timer_ = 0.0f;
	minHoldSec_ = std::max(0.0f, minHoldSec);
	minHoldFrames_ = std::max(0, minHoldFrames);
	holdTimer_ = 0.0f;
	holdFramesLeft_ = minHoldFrames_;
}

void FadeManager::StartUncover()
{
	EnsureTiles();
	state_ = State::TileUncover;
	timer_ = 0.0f;
	// Hold 設定はリセットしてOK（開き開始後は不要）
	minHoldSec_ = 0.0f;
	minHoldFrames_ = 0;
	holdTimer_ = 0.0f;
	holdFramesLeft_ = 0;
}

void FadeManager::Cancel()
{
	state_ = State::None;
	timer_ = 0.0f;
	minHoldSec_ = 0.0f;
	minHoldFrames_ = 0;
	holdTimer_ = 0.0f;
	holdFramesLeft_ = 0;
}

void FadeManager::SetFadeFrames(int frames)
{
	// 互換：framesを「1タイルの閉じ時間」に反映（60fps基準）
	frames = std::max(frames, 1);
	tileAnimSec_ = static_cast<float>(frames) / 60.0f;
}

void FadeManager::EnsureTiles()
{
	auto* dx = DirectXCommon::GetInstance();
	const float w = static_cast<float>(dx->GetClientWidth());
	const float h = static_cast<float>(dx->GetClientHeight());

	// 画面サイズ変化 or 初回なら作り直し
	if (!tiles_.empty() && w == cachedW_ && h == cachedH_) return;

	cachedW_ = w;
	cachedH_ = h;

	tilesX_ = static_cast<int>(std::ceil(w / tileSizePx_));
	tilesY_ = static_cast<int>(std::ceil(h / tileSizePx_));

	tiles_.clear();
	tiles_.reserve(static_cast<size_t>(tilesX_ * tilesY_));

	tileMaxDelay_ = 0.0f;

	// ★タイル用テクスチャは alpha=255 の白1x1推奨
	static const std::string kTex = "white.png";

	for (int y = 0; y < tilesY_; ++y)
	{
		for (int x = 0; x < tilesX_; ++x)
		{
			const float screenX = x * tileSizePx_ + tileSizePx_ * 0.5f;
			const float screenY = y * tileSizePx_ + tileSizePx_ * 0.5f;

			Tile t;
			t.center = ScreenToSprite({ screenX, screenY }, w, h);

			// ★左上→右→下（走査順）で徐々に並べる
			const int idx = y * tilesX_ + x;
			t.delay = idx * tileStaggerSec_;
			tileMaxDelay_ = std::max(tileMaxDelay_, t.delay);

			t.sp = std::make_unique<Sprite>();
			t.sp->Initialize(kTex);
			t.sp->SetAnchorPoint({ 0.5f, 0.5f });
			t.sp->SetRotation(0.0f);
			t.sp->SetPosition(t.center);
			t.sp->SetColor({ 0,0,0,1 }); // タイル自体は常に不透明（αは使わない）

			tiles_.push_back(std::move(t));
		}
	}
}

void FadeManager::DrawTileOverlay()
{
	if (state_ == State::None) return;

	EnsureTiles();
	SpriteManager::GetInstance()->SetRenderSetting_UI();

	const float animSec = std::max(tileAnimSec_, 0.0001f);

	for (auto& t : tiles_)
	{
		float localT = 0.0f;

		if (state_ == State::TileCover || state_ == State::Hold)
		{
			// Hold は完全被覆なので timer_ は coverTotal に固定される
			localT = (timer_ - t.delay) / animSec;          // 0→1
			localT = Smoothstep01(Saturate(localT));        // 線形なら Smoothstep01 を外してOK
		}
		else // TileUncover：逆順で開く
		{
			const float revDelay = tileMaxDelay_ - t.delay;
			localT = (timer_ - revDelay) / animSec;
			localT = 1.0f - Smoothstep01(Saturate(localT)); // 1→0
		}

		if (localT <= 0.001f) continue;

		// 少し大きめにして隙間対策
		const float size = (tileSizePx_ + 2.0f) * localT;

		t.sp->SetPosition(t.center);
		t.sp->SetSize({ size, size });
		t.sp->Update();
		t.sp->Draw();
	}
}
