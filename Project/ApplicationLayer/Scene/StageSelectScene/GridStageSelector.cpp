#define NOMINMAX
#include "GridStageSelector.h"
#include "Input.h"

#include <algorithm>
#include <numbers>
#include <AudioManager.h>
#include <LinearInterpolation.h>

GridStageSelector::GridStageSelector()
{
	AudioManager::GetInstance()->PlaySE("negative.mp3", 0.0f, 0.0f, false); // プリロード
}

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GridStageSelector::Initialize(const SelectorContext& context)
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

	// ロックアイコンをサムネ数分用意
	lockIcons_.clear();
	for (size_t i = 0; i < stages_->size(); ++i)
	{
		auto lock = std::make_unique<Sprite>();
		lock->Initialize("lock.png");
		lock->SetAnchorPoint({ 0.5f, 0.5f });
		lock->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		lockIcons_.push_back(std::move(lock));
	}

	// 影
	selShadow_ = std::make_unique<Sprite>();
	selShadow_->Initialize("white.png");
	selShadow_->SetAnchorPoint({ 0.5f, 0.5f });
	selShadow_->SetColor({ 0, 0, 0, 0.25f });

	// 起動時のフォーカス
	scrollX_ = 0.0f;
	velocityX_ = 0.0f;

	// 起動時の中央も通知
	prevCenterIndex_ = GetCenterIndex();
	if (onCenterChanged_) onCenterChanged_(static_cast<uint32_t>(prevCenterIndex_));
}

/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void GridStageSelector::Update(float deltaTime)
{
	if (!(deltaTime > 0.f) || deltaTime > 0.05f) deltaTime = 1.f / 60.f;

	auto* input = context_.input;
	Vector2 mp = input->GetMousePosition();

	// 押下
	UpdatePress(input, mp);

	// ホイール
	UpdateWheel();

	// ドラッグ
	UpdateDrassing(input, mp, deltaTime);

	// 離し
	UpdateRelease(input, mp);

	// クリックTween
	UpdateTween(deltaTime);

	// 慣性（Tween中は停止）
	UpdateInertia(deltaTime);

	// シェイクの時間更新
	UpdateShake(deltaTime);

	// レイアウト更新
	UpdateLayout();
}


/// -------------------------------------------------------------
///				　			3Dオブジェクトの描画
/// -------------------------------------------------------------
void GridStageSelector::Draw3DObjects()
{

}


/// -------------------------------------------------------------
///				　			2Dオブジェクトの描画
/// -------------------------------------------------------------
void GridStageSelector::Draw2DSprites()
{
	// 影
	if (selShadow_) selShadow_->Draw();

	// サムネ
	for (auto& sp : thumbs_) sp->Draw();

	// ロックアイコン
	for (auto& lock : lockIcons_) lock->Draw();
}


/// -------------------------------------------------------------
///				　			モード切替のフック
/// -------------------------------------------------------------
void GridStageSelector::OnEnter()
{
	// エフェクトやサウンドなど
}


/// -------------------------------------------------------------
///				　			モード切替のフック
/// -------------------------------------------------------------
void GridStageSelector::OnExit()
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
int GridStageSelector::HitTestCardIndex(const Vector2& mousePosition) const
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

	for (int i = 0; i < n; ++i)
	{
		float base = i * gapX_;
		float dx = base - sx;
		if (loop_)
		{
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
///				　指定インデックスへトゥイーン開始
/// -------------------------------------------------------------
void GridStageSelector::StartTweenToIndex(int index, float duration)
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
int GridStageSelector::GetSelectedIndex(Vector2& mousePosition) const
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
int GridStageSelector::GetCenterIndex() const
{
	int n = (int)thumbs_.size();
	if (n == 0) return 0;

	float total = gapX_ * n;
	auto wrap = [&](float x) {
		if (!loop_) return x;
		x = std::fmod(x, total);
		if (x < 0) x += total;
		return x;
		};
	float sx = loop_ ? wrap(scrollX_) : scrollX_;

	int selected = 0; float best = 1e9f;
	for (int i = 0; i < n; ++i) {
		float dx = (i * gapX_) - sx;
		if (loop_) {
			float half = total * 0.5f;
			if (dx > half) dx -= total;
			if (dx < -half) dx += total;
		}
		float dist = std::fabs(dx);
		if (dist < best) { best = dist; selected = i; }
	}
	return selected;
}


/// -------------------------------------------------------------
///					  鍵クリック時に呼ぶ
/// -------------------------------------------------------------
void GridStageSelector::TriggerLockedShake()
{
	shakeActive_ = true; // シェイク開始
	shakeTimer_ = 0.0f;  // リセット
}


/// -------------------------------------------------------------
///					  Update から呼ぶ
/// -------------------------------------------------------------
void GridStageSelector::UpdateShake(float dt)
{
	if (!shakeActive_) return;
	shakeTimer_ += dt;
	if (shakeTimer_ >= shakeDuration_)
	{
		shakeActive_ = false;
		shakeTimer_ = 0.0f;
	}
}


/// -------------------------------------------------------------
///					  現在のオフセット(px)
/// -------------------------------------------------------------
float GridStageSelector::GetShakeOffsetX() const
{
	if (!shakeActive_) return 0.0f;
	// 減衰サイン：env=(1-t)^2 で終端に向けて収束
	float t = std::clamp(shakeTimer_ / shakeDuration_, 0.0f, 1.0f);
	float env = (1.0f - t); env *= env;
	float phase = 2.0f * std::numbers::pi_v<float> *shakeFreqHz_ * shakeTimer_;
	return std::sin(phase) * shakeAmpPx_ * env;
}


/// -------------------------------------------------------------
///				指定インデックスへフォーカス
/// -------------------------------------------------------------
void GridStageSelector::FocusToIndex(int index, bool tween)
{
	// （typoのまま合わせていますが FocusToIndex に改名を推奨）
	index = std::clamp(index, 0, (int)thumbs_.size() - 1);
	if (tween) StartTweenToIndex(index, 0.28f);
	else       scrollX_ = gapX_ * (float)index;
}


/// -------------------------------------------------------------
///				　		　押下
/// -------------------------------------------------------------
void GridStageSelector::UpdatePress(Input* input, Vector2& mp)
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
///				　		　ホイール
/// -------------------------------------------------------------
void GridStageSelector::UpdateWheel()
{
	int wheel = context_.input->GetMouseWheel(); // 環境により GetMouseWheel()

	// ホイール
	if (wheel != 0 && !dragging_ && !tweenActive_)
	{
		// Windows標準の1ノッチ=120を想定（必要に応じて係数調整）
		int steps = std::clamp(wheel / 120, -3, 3);
		if (steps != 0)
		{
			int curIdx = GetCenterIndex();
			// 上方向スクロールで左（前）に送る…などは好みで反転
			int targetIdx = curIdx - steps;
			if (loop_)
			{
				// ループ時はモジュロで回す
				int n = (int)thumbs_.size();
				targetIdx = (targetIdx % n + n) % n;
			}
			else
			{
				targetIdx = std::clamp(targetIdx, 0, (int)thumbs_.size() - 1);
			}
			StartTweenToIndex(targetIdx, 0.22f);
			velocityX_ = 0.0f;
		}
	}
}


/// -------------------------------------------------------------
///				　		　ドラッグ更新
/// -------------------------------------------------------------
void GridStageSelector::UpdateDrassing(Input* input, Vector2& mp, float deltaTime)
{
	if (pressIndex_.has_value() && input->PushMouse(0))
	{
		float rawDx = mp.x - lastMouse_.x;
		float dx = std::clamp(rawDx, -maxDxPerFrame_, maxDxPerFrame_);
		// 端で軽く
		float minX = 0.f, maxX = gapX_ * std::max(0, (int)thumbs_.size() - 1);
		bool over = !loop_ && ((scrollX_ < minX) || (scrollX_ > maxX));
		float k = over ? overdragFactor_ : 1.f;

		dragging_ = true;
		scrollX_ -= dx * k;                    // 右ドラッグで左送る
		clickDeltaAccum_ += std::fabs(dx);
		lastDxPerSec_ = dx / std::max(deltaTime, 1e-4f);
		lastMouse_ = mp;
	}
}


/// -------------------------------------------------------------
///				　		　離し
/// -------------------------------------------------------------
void GridStageSelector::UpdateRelease(Input* input, Vector2& mp)
{
	if (pressIndex_.has_value() && input->ReleaseMouse(0))
	{
		int releaseHit = HitTestCardIndex(mp);
		bool click = (clickDeltaAccum_ < 8.f);

		if (click && clickStartedOnCard_ && releaseHit == *pressIndex_)
		{
			int centerIdx = GetCenterIndex();

			if (*pressIndex_ == centerIdx)
			{
				// 中央クリック：ロックなら遷移しない
				if ((*stages_)[centerIdx].locked)
				{
					// TODO: 効果音/点滅など
					AudioManager::GetInstance()->PlaySE("negative02.mp3", 0.5f, 0.7f);
					TriggerLockedShake();
				}
				else
				{
					if (context_.onRequestMap) context_.onRequestMap((uint32_t)centerIdx);
				}
			}
			else
			{
				// 非中央クリック：まずそこへ緩急で寄せる
				StartTweenToIndex(*pressIndex_, 0.28f);
				velocityX_ = 0.f;
			}
		}
		else
		{
			// ドラッグ終端：慣性
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
void GridStageSelector::UpdateTween(float deltaTime)
{
	if (tweenActive_)
	{
		tweenTimer_ += deltaTime;
		float t = std::clamp(tweenTimer_ / tweenDuration_, 0.f, 1.f);
		// EaseInOutCubic
		float u = t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
		scrollX_ = Lerp(tweenStartX_, tweenTargetX_, u);
		if (t >= 1.f) CancelTween();
	}
}


/// -------------------------------------------------------------
///				　		　慣性更新
/// -------------------------------------------------------------
void GridStageSelector::UpdateInertia(float deltaTime)
{
	if (!tweenActive_ && !dragging_)
	{
		int n = (int)thumbs_.size();
		if (n > 0)
		{
			int idx = GetCenterIndex();
			float target = gapX_ * idx;
			if (loop_)
			{
				float total = gapX_ * n;
				float cur = std::fmod(scrollX_, total); if (cur < 0) cur += total;
				float d = target - cur;
				if (d > total * 0.5f) d -= total;
				if (d < -total * 0.5f) d += total;
				target = scrollX_ + d;
			}
			scrollX_ = Lerp(scrollX_, target, std::clamp(deltaTime * snapK_, 0.f, 1.f));
		}
	}

	// 端の弾性クリップ
	float minX = 0.f, maxX = gapX_ * std::max(0, (int)thumbs_.size() - 1);
	if (!loop_)
	{
		scrollX_ = std::clamp(scrollX_, minX - 80.f, maxX + 80.f);
	}
}


/// -------------------------------------------------------------
///				　		　レイアウト更新
/// -------------------------------------------------------------
void GridStageSelector::UpdateLayout()
{
	int n = (int)thumbs_.size();
	if (n > 0)
	{
		float total = gapX_ * n;
		// scrollX_ を 0..total に正規化（視点側の基準）
		auto wrap = [&](float x) {
			if (!loop_) return x;
			x = std::fmod(x, total);
			if (x < 0) x += total;
			return x;
			};
		float sx = loop_ ? wrap(scrollX_) : scrollX_;

		// 選択カード（最も center に近い）
		int selected = 0;
		{
			float best = 1e9f;
			for (int i = 0; i < n; ++i)
			{
				float dx = (i * gapX_) - sx;
				if (loop_)
				{
					float half = total * 0.5f;
					if (dx > half) dx -= total;
					if (dx < -half) dx += total;
				}
				float dist = std::fabs(dx);
				if (dist < best) { best = dist; selected = i; }
			}
		}

		for (int i = 0; i < n; ++i)
		{
			// i 番目カードの“視差”を [-total/2, total/2] に折り返して配置
			float base = i * gapX_;
			float dx = base - sx;
			if (loop_)
			{
				float half = total * 0.5f;
				if (dx > half) dx -= total;
				if (dx < -half) dx += total;
			}
			else
			{
				// 非ループ時は端の外に出たらそのまま
			}
			float cx = center_.x + dx;
			float cy = center_.y;

			// 中央が“ロック中”でシェイク中なら、中央カードだけ横ブレ
			if (i == selected && (*stages_)[i].locked) {
				cx += GetShakeOffsetX();
			}

			float dist = std::fabs(cx - center_.x);
			float scale = 1.0f + std::max(0.0f, 1.0f - dist / gapX_) * focusScale_;
			bool locked = (*stages_)[i].locked;

			thumbs_[i]->SetPosition({ cx, cy });
			thumbs_[i]->SetSize({ baseW_ * scale, baseH_ * scale }); // 大きさはここをいじればOK
			float tint = locked ? 0.35f : 1.0f;
			thumbs_[i]->SetColor({ tint,tint,tint,1 });
			thumbs_[i]->Update();

			// ロック表示
			if (i < (int)lockIcons_.size())
			{
				auto& icon = lockIcons_[i];
				if ((*stages_)[i].locked)
				{
					icon->SetPosition({ cx + lockOffset_.x, cy + lockOffset_.y });
					icon->SetSize({ baseW_ * scale * lockScale_, baseH_ * scale * lockScale_ });
					icon->SetColor({ 1,1,1,0.95f });  // ほぼ不透明
				}
				else
				{
					// アンロック時は透過させておく（描画順は上だが見えない）
					icon->SetPosition({ cx + lockOffset_.x, cy + lockOffset_.y });
					icon->SetSize({ baseW_ * scale * lockScale_, baseH_ * scale * lockScale_ });
					icon->SetColor({ 1,1,1,0.0f });
				}
				icon->Update();
			}
		}

		if (!thumbs_.empty())
		{
			auto& p = thumbs_[selected]->GetPosition();
			selShadow_->SetPosition({ p.x, p.y + 6 }); selShadow_->SetSize({ 320,200 }); selShadow_->Update();
		}

		// 中央変更の通知
		if (selected != prevCenterIndex_)
		{
			prevCenterIndex_ = selected; // 更新
			if (onCenterChanged_) onCenterChanged_((uint32_t)selected); // コールバック呼び出し
		}
	}
}
