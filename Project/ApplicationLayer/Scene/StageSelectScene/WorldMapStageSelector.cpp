#define NOMINMAX
#include "WorldMapStageSelector.h"
#include "Input.h"

#include <algorithm>
#include <numbers>

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void WorldMapStageSelector::Initialize(const SelectorContext& context)
{
	context_ = context; // コンテキスト保存
	stages_ = context_.stages; // ステージ情報リスト保存
	thumbs_.clear(); // クリア

	// サムネ生成
	for (const auto& stage : *stages_)
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(stage.thumbPath.empty() ? "white.png" : stage.thumbPath.c_str());
		sprite->SetAnchorPoint({ 0.5f, 0.5f }); // 中心
		sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		thumbs_.push_back(std::move(sprite));// moveで追加
	}

	// 選択枠と影
	selFrame_ = std::make_unique<Sprite>();
	selFrame_->Initialize("white.png");
	selFrame_->SetAnchorPoint({ 0.5f, 0.5f });

	selShadow_ = std::make_unique<Sprite>();
	selShadow_->Initialize("white.png");
	selShadow_->SetAnchorPoint({ 0.5f, 0.5f });
	selShadow_->SetColor({ 0, 0, 0, 0.25f });

	// 起動時のフォーカス
	scrollX_ = 0.0f;
	velocityX_ = 0.0f;

	// 9面スロットを画面座標で用意
	BuildSlots();

	// ワールドマップは“全件見せ”にするので、スクロール系は無効気味に
	loop_ = false;
	scrollX_ = 0.0f;
	velocityX_ = 0.0f;

	// カードサイズは画面向けに少し小さめ推奨
	baseW_ = 60.f;
	baseH_ = 60.f;
	focusScale_ = 0.10f; // お好みで

	// --- aura sprite (白テクを下地に使う) ---
	aura_ = std::make_unique<Sprite>();
	aura_->Initialize("white.png");
	aura_->SetAnchorPoint({ 0.5f, 0.5f });
	aura_->SetColor({ 1, 1, 0.2f, 0.0f }); // 黄っぽい光、αは後でアニメ
}

/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void WorldMapStageSelector::Update(float deltaTime)
{
	if (!(deltaTime > 0.f) || deltaTime > 0.05f) deltaTime = 1.f / 60.f;

	auto* input = context_.input;
	Vector2 mp = input->GetMousePosition();
	hoverIndex_ = HitTestCardIndex(mp);   // 今フレームのホバー
	auraTimer_ += deltaTime;              // オーラの脈動時間

	// 押下
	UpdatePress(input, mp);

	// 離し
	UpdateRelease(input, mp);

	// クリックTween
	UpdateTween(deltaTime);

	// シェイクの時間更新
	UpdateShake(deltaTime);

	// レイアウト更新
	UpdateLayout();
}


/// -------------------------------------------------------------
///				　			3Dオブジェクトの描画
/// -------------------------------------------------------------
void WorldMapStageSelector::Draw3DObjects()
{

}


/// -------------------------------------------------------------
///				　			2Dオブジェクトの描画
/// -------------------------------------------------------------
void WorldMapStageSelector::Draw2DSprites()
{
	if (aura_)      aura_->Draw();   // 一番下
	// 影 → カード → 枠 の順で
	if (selShadow_) selShadow_->Draw();
	for (auto& sp : thumbs_) sp->Draw();
	if (selFrame_)  selFrame_->Draw();
}


/// -------------------------------------------------------------
///				　			モード切替のフック
/// -------------------------------------------------------------
void WorldMapStageSelector::OnEnter()
{
	// エフェクトやサウンドなど
}


/// -------------------------------------------------------------
///				　			モード切替のフック
/// -------------------------------------------------------------
void WorldMapStageSelector::OnExit()
{
	CancelTween(); // トゥイーンキャンセル
	dragging_ = false;
	pressIndex_.reset();
	velocityX_ = 0.f;
	lastDxPerSec_ = 0.f;
}


/// -------------------------------------------------------------
///				　		指定インデックスへフォーカス
/// -------------------------------------------------------------
int WorldMapStageSelector::HitTestCardIndex(const Vector2& mousePosition) const
{
	int n = (int)thumbs_.size();
	if (n == 0) return -1;

	// ほんの少し甘めにするなら 1.1f などを掛けてください
	const float w = baseW_;
	const float h = baseH_;

	for (int i = 0; i < std::min(n, 9); ++i) {
		const auto& p = slots_[i];
		if (mousePosition.x >= p.x - w * 0.5f && mousePosition.x <= p.x + w * 0.5f &&
			mousePosition.y >= p.y - h * 0.5f && mousePosition.y <= p.y + h * 0.5f) {
			return i;
		}
	}
	return -1;
}


/// -------------------------------------------------------------
///				　指定インデックスへトゥイーン開始
/// -------------------------------------------------------------
void WorldMapStageSelector::StartTweenToIndex(int index, float duration)
{
	int n = (int)thumbs_.size();
	if (n == 0) return;
	index = std::clamp(index, 0, n - 1);

	tweenActive_ = true;
	tweenStartX_ = scrollX_;

	float target = gapX_ * (float)index;
	if (loop_)
	{
		float total = gapX_ * n;
		// 現在位置 scrollX_ から target への最短方向へ
		float cur = std::fmod(scrollX_, total); if (cur < 0) cur += total;
		float d = target - cur;
		if (d > total * 0.5f) d -= total;
		if (d < -total * 0.5f) d += total;
		tweenTargetX_ = scrollX_ + d;
	}
	else {
		tweenTargetX_ = target;
	}

	tweenTimer_ = 0.0f;
	tweenDuration_ = std::max(duration, 0.01f);
	velocityX_ = 0.0f;
}


/// -------------------------------------------------------------
///	   マウス位置からヒットしているカードのインデックスを取得
/// -------------------------------------------------------------
int WorldMapStageSelector::GetSelectedIndex(Vector2& mousePosition) const
{
	int n = (int)thumbs_.size();
	if (n == 0) return -1;

	float total = gapX_ * n;
	auto wrap = [&](float x) {
		if (!loop_) return x;
		x = std::fmod(x, total);
		if (x < 0) x += total;
		return x;
		};
	float sx = loop_ ? wrap(scrollX_) : scrollX_;

	for (int i = 0; i < n; ++i) {
		float base = i * gapX_;
		float dx = base - sx;
		if (loop_) {
			float half = total * 0.5f;
			if (dx > half) dx -= total;
			if (dx < -half) dx += total;
		}
		float cx = center_.x + dx;
		float cy = center_.y;
		float dist = std::fabs(cx - center_.x);
		float scale = 1.0f + std::max(0.0f, 1.0f - dist / gapX_) * focusScale_;
		float w = baseW_ * scale, h = baseH_ * scale;

		if (mousePosition.x >= cx - w * 0.5f && mousePosition.x <= cx + w * 0.5f &&
			mousePosition.y >= cy - h * 0.5f && mousePosition.y <= cy + h * 0.5f) {
			return i;
		}
	}
	return -1;
}


/// -------------------------------------------------------------
///				中央に最も近いインデックスを取得
/// -------------------------------------------------------------
int WorldMapStageSelector::GetCenterIndex() const
{
	int n = (int)thumbs_.size();
	if (n == 0) return -1;

	const Vector2 screenCenter{
		(float)context_.screenWidth * 0.5f,
		(float)context_.screenHeight * 0.5f
	};

	int selected = 0;
	float best = 1e9f;
	for (int i = 0; i < std::min(n, 9); ++i) {
		float d = std::hypot(slots_[i].x - screenCenter.x, slots_[i].y - screenCenter.y);
		if (d < best) { best = d; selected = i; }
	}
	return selected;
}


/// -------------------------------------------------------------
///					  鍵クリック時に呼ぶ
/// -------------------------------------------------------------
void WorldMapStageSelector::TriggerLockedShake()
{
	shakeActive_ = true; // シェイク開始
	shakeTimer_ = 0.0f;  // リセット
}


/// -------------------------------------------------------------
///					  Update から呼ぶ
/// -------------------------------------------------------------
void WorldMapStageSelector::UpdateShake(float dt)
{
	if (!shakeActive_) return;

	shakeTimer_ += dt;  // ★ 時間を進める
	if (shakeTimer_ >= shakeDuration_) {
		shakeActive_ = false;
		shakeTimer_ = 0.0f;
		shakeTargetIndex_ = -1; // 対象解除
	}
}


/// -------------------------------------------------------------
///					  現在のオフセット(px)
/// -------------------------------------------------------------
float WorldMapStageSelector::GetShakeOffsetX() const
{
	if (!shakeActive_) return 0.0f;
	// 減衰サイン：env=(1-t)^2 で終端に向けて収束
	float t = std::clamp(shakeTimer_ / shakeDuration_, 0.0f, 1.0f);
	float env = (1.0f - t); env *= env;
	float phase = 2.0f * std::numbers::pi_v<float> *shakeFreqHz_ * shakeTimer_;
	return std::sin(phase) * shakeAmpPx_ * env;
}

void WorldMapStageSelector::BuildSlots()
{
	// 横は 20%/50%/80%、縦は 55%/70%/85% に配置（下寄り）
	const float col[3] = {
		context_.screenWidth * 0.20f,
		context_.screenWidth * 0.50f,
		context_.screenWidth * 0.80f,
	};
	const float row[3] = {
		context_.screenHeight * 0.55f, // 上段
		context_.screenHeight * 0.70f, // 中段（ここに3面を並べる）
		context_.screenHeight * 0.85f, // 下段
	};

	// 行優先 0..8
	slots_[0] = { col[0], row[0] }; slots_[1] = { col[1], row[0] }; slots_[2] = { col[2], row[0] };
	slots_[3] = { col[0], row[1] }; slots_[4] = { col[1], row[1] }; slots_[5] = { col[2], row[1] };
	slots_[6] = { col[0], row[2] }; slots_[7] = { col[1], row[2] }; slots_[8] = { col[2], row[2] };

	// 3面以下なら中央段（3,4,5）に詰める
	if (stages_ && (int)stages_->size() <= 3) {
		for (int i = 0; i < (int)stages_->size(); ++i) {
			slots_[i] = slots_[3 + i];
		}
	}
}

int WorldMapStageSelector::GetNextPlayableIndex() const
{
	if (!stages_ || stages_->empty()) return -1;

	// 1) まだロックされていない中で最小インデックスを返す
	//    （StageInfo に cleared 等があるならそこに合わせて条件を広げてください）
	for (int i = 0; i < (int)stages_->size() && i < 9; ++i) {
		if (!(*stages_)[i].locked) return i;
	}
	return -1;
}

void WorldMapStageSelector::TriggerLockedShake(int index)
{
	shakeTargetIndex_ = index;
	shakeActive_ = true;
	shakeTimer_ = 0.0f;
}


/// -------------------------------------------------------------
///				指定インデックスへフォーカス
/// -------------------------------------------------------------
void WorldMapStageSelector::FocusToIndex(int index, bool tween)
{
	// （typoのまま合わせていますが FocusToIndex に改名を推奨）
	index = std::clamp(index, 0, (int)thumbs_.size() - 1);
	if (tween) StartTweenToIndex(index, 0.28f);
	else       scrollX_ = gapX_ * (float)index;
}


/// -------------------------------------------------------------
///				　		　押下
/// -------------------------------------------------------------
void WorldMapStageSelector::UpdatePress(Input* input, Vector2& mp)
{
	if (input->TriggerMouse(0))
	{
		int hit = HitTestCardIndex(mp);
		if (hit >= 0) { pressIndex_ = hit; clickStartedOnCard_ = true; }
		else { pressIndex_.reset(); clickStartedOnCard_ = false; }
		dragging_ = false;
		dragStart_ = lastMouse_ = mp;
		velocityX_ = 0.f;
		clickDeltaAccum_ = 0.f;
		CancelTween();
	}
}

/// -------------------------------------------------------------
///				　		　離し
/// -------------------------------------------------------------
void WorldMapStageSelector::UpdateRelease(Input* input, Vector2& mp)
{
	if (pressIndex_.has_value() && input->ReleaseMouse(0))
	{
		int releaseHit = HitTestCardIndex(mp);
		bool click = (clickDeltaAccum_ < 8.f);

		if (click && clickStartedOnCard_ && releaseHit == *pressIndex_)
		{
			int idx = *pressIndex_;
			if ((*stages_)[idx].locked)
			{
				TriggerLockedShake(idx);  // 対象カードだけ震える
			}
			else
			{
				if (context_.onRequestPlay) context_.onRequestPlay((*stages_)[idx].id);
			}
		}
		else
		{
			// （ドラッグ終端の慣性は今のまま）
			velocityX_ = std::clamp(-lastDxPerSec_, -maxVel_, maxVel_);
		}
		pressIndex_.reset();
		clickStartedOnCard_ = false;
		dragging_ = false;
	}
}

/// -------------------------------------------------------------
///				　		　クリックTween更新
/// -------------------------------------------------------------
void WorldMapStageSelector::UpdateTween(float deltaTime)
{
	if (tweenActive_)
	{
		tweenTimer_ += deltaTime;
		float t = std::clamp(tweenTimer_ / tweenDuration_, 0.f, 1.f);
		// EaseInOutCubic
		float u = t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
		scrollX_ = std::lerp(tweenStartX_, tweenTargetX_, u);
		if (t >= 1.f) CancelTween();
	}
}


/// -------------------------------------------------------------
///				　		　レイアウト更新
/// -------------------------------------------------------------
void WorldMapStageSelector::UpdateLayout()
{
	int n = (int)thumbs_.size();
	if (n <= 0) return;

	const float hoverScale = 0.18f; // ホバー拡大
	const float pressBump = 0.12f; // 押下上乗せ
	const float normalTint = 0.85f; // 通常
	const float hoverTint = 1.15f; // ホバー強調

	// オーラの優先順位：押下中 > ホバー中 > 次に攻略
	int auraIdx = -1;
	if (pressIndex_.has_value())          auraIdx = *pressIndex_;
	else if (hoverIndex_ >= 0)            auraIdx = hoverIndex_;
	else                                  auraIdx = GetNextPlayableIndex();

	for (int i = 0; i < std::min(n, 9); ++i) {
		auto p = slots_[i];

		const bool isHover = (i == hoverIndex_);
		const bool isPress = (pressIndex_.has_value() && i == *pressIndex_);
		float scale = 1.0f
			+ (isHover ? hoverScale : 0.0f)
			+ (isPress ? pressBump : 0.0f);

		float tint = (*stages_)[i].locked ? 0.40f
			: (isHover ? hoverTint : normalTint);

		if (i == shakeTargetIndex_) p.x += GetShakeOffsetX() * scale;

		thumbs_[i]->SetPosition(p);
		thumbs_[i]->SetSize({ baseW_ * scale, baseH_ * scale });
		thumbs_[i]->SetColor({ tint, tint, tint, 1.0f });
		thumbs_[i]->Update();
	}

	// --- オーラ（選択スケールと同じ大きさで、さらに脈動を掛ける）
	if (aura_ && auraIdx >= 0 && auraIdx < n) {
		auto pAura = slots_[auraIdx];
		const bool auraHover = (auraIdx == hoverIndex_);
		const bool auraPressed = (pressIndex_.has_value() && auraIdx == *pressIndex_);
		float selectedScale = 1.0f
			+ (auraHover ? hoverScale : 0.0f)
			+ (auraPressed ? pressBump : 0.0f);
		// 揺れも同じスケールで同期
		if (auraIdx == shakeTargetIndex_) pAura.x += GetShakeOffsetX() * selectedScale;

		// 脈動（0..1）
		float s = (std::sin(auraTimer_ * auraSpeed_) + 1.0f) * 0.5f;
		float pulse = std::lerp(auraMin_, auraMax_, s);
		float alpha = std::lerp(0.15f, 0.50f, s); // ちょい強めに

		// ★ 最終スケール = 選択スケール × 脈動
		float finalScale = selectedScale * pulse;
		aura_->SetPosition(pAura);
		aura_->SetSize({ baseW_ * finalScale, baseH_ * finalScale });
		aura_->SetColor({ 1.0f, 1.0f, 0.2f, alpha });
		aura_->Update();
	}

	// 選択枠はホバー中だけ表示（不要なら外してOK）
	if (hoverIndex_ >= 0) {
		const auto& p = slots_[hoverIndex_];
		selShadow_->SetPosition({ p.x, p.y + 6 }); selShadow_->SetSize({ baseW_ + 20, baseH_ + 20 }); selShadow_->Update();
		selFrame_->SetPosition(p);                 selFrame_->SetSize({ baseW_ + 20, baseH_ + 20 }); selFrame_->Update();
	}
	else {
		selShadow_->SetPosition({ -9999, -9999 });
		selFrame_->SetPosition({ -9999, -9999 });
	}
}
