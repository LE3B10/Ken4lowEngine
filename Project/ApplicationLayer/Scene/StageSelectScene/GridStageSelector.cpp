#define NOMINMAX
#include "GridStageSelector.h"
#include "DirectXCommon.h"
#include "Input.h"

#include <algorithm>
#include <numbers>
#include <AudioManager.h>
#include <LinearInterpolation.h>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　			　コンストラクタ
/// -------------------------------------------------------------
GridStageSelector::GridStageSelector()
{
	K4E::AudioManager::GetInstance()->PlaySE("negative.mp3", 0.0f, 0.0f, false); // プリロード
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
		auto sprite = std::make_unique<K4E::Sprite>();
		sprite->Initialize(stage.thumbPath.empty() ? "Effects/white.dds" : stage.thumbPath.c_str());
		sprite->SetAnchorPoint({ 0.5f, 0.5f }); // 中心
		sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		thumbs_.push_back(std::move(sprite));// moveで追加
	}

	// ロックアイコンをサムネ数分用意
	lockUI_.sprites.clear();
	for (size_t i = 0; i < stages_->size(); ++i)
	{
		auto lock = std::make_unique<K4E::Sprite>();
		lock->Initialize("UI/Common/lock.dds");
		lock->SetAnchorPoint({ 0.5f, 0.5f });
		lock->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		lockUI_.sprites.push_back(std::move(lock));
	}

	// 影
	selShadow_ = std::make_unique<K4E::Sprite>();
	selShadow_->Initialize("Effects/white.dds");
	selShadow_->SetAnchorPoint({ 0.5f, 0.5f });
	selShadow_->SetColor({ 0, 0, 0, 0.25f });

	// 起動時のフォーカス
	scroll_.scrollX = 0.0f;
	scroll_.velocityX = 0.0f;

	K4E::DirectXCommon* dxCommon = K4E::DirectXCommon::GetInstance();
	layout_.center = { dxCommon->GetClientWidth() * 0.5f, dxCommon->GetClientHeight() * 0.5f };

	// 起動時の中央も通知
	prevCenterIndex_ = GetCenterIndex();
	if (onCenterChanged_) onCenterChanged_(static_cast<uint32_t>(prevCenterIndex_));

	// アンロックアニメーション用タイマー初期化
	unlockAnim_.timers.assign(stages_->size(), 0.0f);
}

/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void GridStageSelector::Update(float deltaTime)
{
	if (!(deltaTime > 0.f) || deltaTime > 0.05f) deltaTime = K4E::DirectXCommon::GetInstance()->GetFPSCounter().GetDeltaTime(); // 異常値補正

	auto* input = context_.input;
	K4E::Vector2 mp = input->GetMousePosition();

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

	// 中央カード変更通知
	CheckCenterCardChanged(deltaTime);
}


/// -------------------------------------------------------------
///				　			3Dオブジェクトの描画
/// -------------------------------------------------------------
void GridStageSelector::Draw3DObjects()
{
	// 今はスプライトで描画しているが、将来3Dオブジェクトを描画する予定かもしれない
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
	for (auto& lock : lockUI_.sprites) lock->Draw();
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
	scroll_.dragging = false;
	scroll_.pressIndex.reset();
	scroll_.velocityX = 0.f;
	scroll_.lastDxPerSec = 0.f;
}


/// -------------------------------------------------------------
///				　		指定インデックスへフォーカス
/// -------------------------------------------------------------
int GridStageSelector::HitTestCardIndex(const K4E::Vector2& mousePosition) const
{
	int n = (int)thumbs_.size();
	if (n == 0) return -1;

	float total = layout_.gapX * n;
	auto wrap = [&](float x) {
		if (!scroll_.loop) return x;
		x = std::fmod(x, total);
		if (x < 0) x += total;
		return x;
		};
	float sx = scroll_.loop ? wrap(scroll_.scrollX) : scroll_.scrollX;

	for (int i = 0; i < n; ++i)
	{
		float base = i * layout_.gapX;
		float dx = base - sx;
		if (scroll_.loop)
		{
			float half = total * 0.5f;
			if (dx > half) dx -= total;
			if (dx < -half) dx += total;
		}
		float cx = layout_.center.x + dx;
		float cy = layout_.center.y;
		float dist = std::fabs(cx - layout_.center.x);
		float scale = 1.0f + std::max(0.0f, 1.0f - dist / layout_.gapX) * layout_.focusScale;
		float w = layout_.baseW * scale, h = layout_.baseH * scale;

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

	tween_.active = true;
	tween_.startX = scroll_.scrollX;

	float target = layout_.gapX * (float)index;
	if (scroll_.loop)
	{
		float total = layout_.gapX * n;
		// 現在位置 scroll_.scrollX から target への最短方向へ
		float cur = std::fmod(scroll_.scrollX, total); if (cur < 0) cur += total;
		float d = target - cur;
		if (d > total * 0.5f) d -= total;
		if (d < -total * 0.5f) d += total;
		tween_.targetX = scroll_.scrollX + d;
	}
	else {
		tween_.targetX = target;
	}

	tween_.timer = 0.0f;
	tween_.duration = std::max(duration, 0.01f);
	scroll_.velocityX = 0.0f;
}


/// -------------------------------------------------------------
///	   マウス位置からヒットしているカードのインデックスを取得
/// -------------------------------------------------------------
int GridStageSelector::GetSelectedIndex(K4E::Vector2& mousePosition) const
{
	int n = (int)thumbs_.size();
	if (n == 0) return -1;

	float total = layout_.gapX * n;
	auto wrap = [&](float x) {
		if (!scroll_.loop) return x;
		x = std::fmod(x, total);
		if (x < 0) x += total;
		return x;
		};
	float sx = scroll_.loop ? wrap(scroll_.scrollX) : scroll_.scrollX;

	for (int i = 0; i < n; ++i) {
		float base = i * layout_.gapX;
		float dx = base - sx;
		if (scroll_.loop) {
			float half = total * 0.5f;
			if (dx > half) dx -= total;
			if (dx < -half) dx += total;
		}
		float cx = layout_.center.x + dx;
		float cy = layout_.center.y;
		float dist = std::fabs(cx - layout_.center.x);
		float scale = 1.0f + std::max(0.0f, 1.0f - dist / layout_.gapX) * layout_.focusScale;
		float w = layout_.baseW * scale, h = layout_.baseH * scale;

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

	float total = layout_.gapX * n;
	auto wrap = [&](float x) {
		if (!scroll_.loop) return x;
		x = std::fmod(x, total);
		if (x < 0) x += total;
		return x;
		};
	float sx = scroll_.loop ? wrap(scroll_.scrollX) : scroll_.scrollX;

	int selected = 0; float best = 1e9f;
	for (int i = 0; i < n; ++i) {
		float dx = (i * layout_.gapX) - sx;
		if (scroll_.loop) {
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
	shake_.active = true; // シェイク開始
	shake_.timer = 0.0f;  // リセット
}


/// -------------------------------------------------------------
///					  Update から呼ぶ
/// -------------------------------------------------------------
void GridStageSelector::UpdateShake(float dt)
{
	if (!shake_.active) return;
	shake_.timer += dt;
	if (shake_.timer >= shake_.duration)
	{
		shake_.active = false;
		shake_.timer = 0.0f;
	}
}


/// -------------------------------------------------------------
///					  現在のオフセット(px)
/// -------------------------------------------------------------
float GridStageSelector::GetShakeOffsetX() const
{
	if (!shake_.active) return 0.0f;
	// 減衰サイン：env=(1-t)^2 で終端に向けて収束
	float t = std::clamp(shake_.timer / shake_.duration, 0.0f, 1.0f);
	float env = (1.0f - t); env *= env;
	float phase = 2.0f * std::numbers::pi_v<float> *shake_.freqHz * shake_.timer;
	return std::sin(phase) * shake_.ampPx * env;
}


/// -------------------------------------------------------------
///				指定インデックスへフォーカス
/// -------------------------------------------------------------
void GridStageSelector::FocusToIndex(int index, bool tween)
{
	// 範囲制限
	index = std::clamp(index, 0, (int)thumbs_.size() - 1);

	// トゥイーン開始または即座に移動
	if (tween) StartTweenToIndex(index, 0.28f);
	else       scroll_.scrollX = layout_.gapX * (float)index;
}

/// -------------------------------------------------------------
///				アンロックアニメーション再生
/// -------------------------------------------------------------
void GridStageSelector::PlayUnlockAnim(int index)
{
	// 範囲チェック
	if (index < 0 || index >= (int)thumbs_.size()) return;

	// タイマーリセット
	if (unlockAnim_.timers.size() != thumbs_.size()) {
		unlockAnim_.timers.assign(thumbs_.size(), 0.0f);
	}

	// 再生
	unlockAnim_.timers[index] = unlockAnim_.duration;
}

/// -------------------------------------------------------------
///				　		　押下
/// -------------------------------------------------------------
void GridStageSelector::UpdatePress(K4E::Input* input, K4E::Vector2& mp)
{
	// 押下開始
	if (input->TriggerMouse(0))
	{
		// ヒットテスト
		int hit = HitTestCardIndex(mp);

		// 押下情報保存
		if (hit >= 0) { scroll_.pressIndex = hit; scroll_.clickStartedOnCard = true; }
		else { scroll_.pressIndex.reset(); scroll_.clickStartedOnCard = false; }

		// ドラッグ準備
		scroll_.dragging = false;
		scroll_.dragStart = scroll_.lastMouse = mp;
		scroll_.velocityX = 0.f;
		scroll_.clickDeltaAccum = 0.f;

		// トゥイーン中断
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
	if (wheel != 0 && !scroll_.dragging && !tween_.active)
	{
		// Windows標準の1ノッチ=120を想定（必要に応じて係数調整）
		int steps = std::clamp(wheel / 120, -3, 3);
		if (steps != 0)
		{
			int curIdx = GetCenterIndex();
			// 上方向スクロールで左（前）に送る…などは好みで反転
			int targetIdx = curIdx - steps;
			if (scroll_.loop)
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
			scroll_.velocityX = 0.0f;
		}
	}
}


/// -------------------------------------------------------------
///				　		　ドラッグ更新
/// -------------------------------------------------------------
void GridStageSelector::UpdateDrassing(K4E::Input* input, K4E::Vector2& mp, float deltaTime)
{
	if (scroll_.pressIndex.has_value() && input->PushMouse(0))
	{
		float rawDx = mp.x - scroll_.lastMouse.x;
		float dx = std::clamp(rawDx, -tuning_.maxDxPerFrame, tuning_.maxDxPerFrame);
		// 端で軽く
		float minX = 0.f, maxX = layout_.gapX * std::max(0, (int)thumbs_.size() - 1);
		bool over = !scroll_.loop && ((scroll_.scrollX < minX) || (scroll_.scrollX > maxX));
		float k = over ? tuning_.overdragFactor : 1.f;

		scroll_.dragging = true;
		scroll_.scrollX -= dx * k;                    // 右ドラッグで左送る
		scroll_.clickDeltaAccum += std::fabs(dx);
		scroll_.lastDxPerSec = dx / std::max(deltaTime, 1e-4f);
		scroll_.lastMouse = mp;
	}
}


/// -------------------------------------------------------------
///				　		　離し
/// -------------------------------------------------------------
void GridStageSelector::UpdateRelease(K4E::Input* input, K4E::Vector2& mp)
{
	// 押下中かつ離された時
	if (scroll_.pressIndex.has_value() && input->ReleaseMouse(0))
	{
		// ヒットテスト
		int releaseHit = HitTestCardIndex(mp);
		bool click = (scroll_.clickDeltaAccum < 8.f);

		// クリック処理
		if (click && scroll_.clickStartedOnCard && releaseHit == *scroll_.pressIndex)
		{
			// クリック：押下開始位置と離した位置が同じカード上
			int centerIdx = GetCenterIndex();

			// 中央カードかどうかで処理分岐
			if (*scroll_.pressIndex == centerIdx)
			{
				// 中央クリック：ロックなら遷移しない
				if ((*stages_)[centerIdx].locked)
				{
					// TODO: 効果音/点滅など
					K4E::AudioManager::GetInstance()->PlaySE("negative02.mp3", 0.5f, 0.7f);

					// シェイク
					TriggerLockedShake();
				}
				else
				{
					// アンロックアニメーション中なら無視
					if (context_.onRequestMap) context_.onRequestMap((uint32_t)centerIdx);
				}
			}
			else
			{
				// 非中央クリック：まずそこへ緩急で寄せる
				StartTweenToIndex(*scroll_.pressIndex, 0.28f);
				scroll_.velocityX = 0.f;
			}
		}
		else
		{
			// ドラッグ終端：慣性
			scroll_.velocityX = std::clamp(-scroll_.lastDxPerSec, -tuning_.maxVel, tuning_.maxVel);
		}

		// 押下情報クリア
		scroll_.pressIndex.reset();
		scroll_.clickStartedOnCard = false;
		scroll_.dragging = false;
	}
}

/// -------------------------------------------------------------
///				　		　クリックTween更新
/// -------------------------------------------------------------
void GridStageSelector::UpdateTween(float deltaTime)
{
	// クリックトゥイーン更新
	if (tween_.active)
	{
		tween_.timer += deltaTime;
		float t = std::clamp(tween_.timer / tween_.duration, 0.f, 1.f);
		float u = K4E::EaseInOutCubic(t); // イージング関数適用
		scroll_.scrollX = K4E::Lerp(tween_.startX, tween_.targetX, u);
		if (t >= 1.f) CancelTween();
	}
}


/// -------------------------------------------------------------
///				　		　慣性更新
/// -------------------------------------------------------------
void GridStageSelector::UpdateInertia(float deltaTime)
{
	if (!tween_.active && !scroll_.dragging)
	{
		int n = (int)thumbs_.size();
		if (n > 0)
		{
			int idx = GetCenterIndex();
			float target = layout_.gapX * idx;
			if (scroll_.loop)
			{
				float total = layout_.gapX * n;
				float cur = std::fmod(scroll_.scrollX, total); if (cur < 0) cur += total;
				float d = target - cur;
				if (d > total * 0.5f) d -= total;
				if (d < -total * 0.5f) d += total;
				target = scroll_.scrollX + d;
			}
			scroll_.scrollX = K4E::Lerp(scroll_.scrollX, target, std::clamp(deltaTime * tuning_.snapK, 0.f, 1.f));
		}
	}

	// 端の弾性クリップ
	float minX = 0.f, maxX = layout_.gapX * std::max(0, (int)thumbs_.size() - 1);
	if (!scroll_.loop)
	{
		scroll_.scrollX = std::clamp(scroll_.scrollX, minX - 80.f, maxX + 80.f);
	}
}


/// -------------------------------------------------------------
///				　		　レイアウト更新
/// -------------------------------------------------------------
void GridStageSelector::UpdateLayout()
{
	// サムネ数
	int n = (int)thumbs_.size();

	// レイアウト更新
	if (n > 0)
	{
		float total = layout_.gapX * n;
		// scroll_.scrollX を 0..total に正規化（視点側の基準）
		auto wrap = [&](float x) {
			if (!scroll_.loop) return x;
			x = std::fmod(x, total);
			if (x < 0) x += total;
			return x;
			};
		float sx = scroll_.loop ? wrap(scroll_.scrollX) : scroll_.scrollX;

		// 選択カード（最も center に近い）
		int selected = 0;
		{
			float best = 1e9f;
			for (int i = 0; i < n; ++i)
			{
				float dx = (i * layout_.gapX) - sx;
				if (scroll_.loop)
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
			float base = i * layout_.gapX;
			float dx = base - sx;
			if (scroll_.loop)
			{
				float half = total * 0.5f;
				if (dx > half) dx -= total;
				if (dx < -half) dx += total;
			}
			else
			{
				// 非ループ時は端の外に出たらそのまま
			}
			float cx = layout_.center.x + dx;
			float cy = layout_.center.y;

			// 中央が“ロック中”でシェイク中なら、中央カードだけ横ブレ
			if (i == selected && (*stages_)[i].locked) {
				cx += GetShakeOffsetX();
			}

			float dist = std::fabs(cx - layout_.center.x);
			float scale = 1.0f + std::max(0.0f, 1.0f - dist / layout_.gapX) * layout_.focusScale;
			bool locked = (*stages_)[i].locked;

			thumbs_[i]->SetPosition({ cx, cy });
			thumbs_[i]->SetSize({ layout_.baseW * scale, layout_.baseH * scale }); // 大きさはここをいじればOK
			float tint = locked ? 0.35f : 1.0f;
			thumbs_[i]->SetColor({ tint,tint,tint,1 });
			thumbs_[i]->Update();

			// ロック表示
			if (i < (int)lockUI_.sprites.size())
			{
				auto& icon = lockUI_.sprites[i];
				if ((*stages_)[i].locked)
				{
					icon->SetPosition({ cx + lockUI_.offset.x, cy + lockUI_.offset.y });
					icon->SetSize({ layout_.baseW * scale * lockUI_.scale, layout_.baseH * scale * lockUI_.scale });
					icon->SetColor({ 1,1,1,0.95f });  // ほぼ不透明
				}
				else
				{
					// アンロック時は透過させておく（描画順は上だが見えない）
					icon->SetPosition({ cx + lockUI_.offset.x, cy + lockUI_.offset.y });
					icon->SetSize({ layout_.baseW * scale * lockUI_.scale, layout_.baseH * scale * lockUI_.scale });
					icon->SetColor({ 1,1,1,0.0f });
				}
				icon->Update();
			}

			// 解除演出中ならポンっと大きく
			if (!unlockAnim_.timers.empty() && unlockAnim_.timers[i] > 0.0f)
			{
				float u = unlockAnim_.timers[i] / unlockAnim_.duration; // 0..1
				// 最初大きく→徐々に1.0に戻る
				float extra = 0.35f * u;
				scale *= (1.0f + extra);
			}
		}

		// 選択影の更新
		if (!thumbs_.empty())
		{
			// 選択影の位置更新
			auto& p = thumbs_[selected]->GetPosition();

			// 影の更新
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

/// -------------------------------------------------------------
///				　		中央カード変更検出
/// -------------------------------------------------------------
void GridStageSelector::CheckCenterCardChanged(float deltaTime)
{
	// 中央カード変更検出
	if (!unlockAnim_.timers.empty())
	{
		// アンロックアニメーション中は中央変更通知しない
		for (auto& t : unlockAnim_.timers)
		{
			// タイマー更新
			if (t > 0.0f)
			{
				t -= deltaTime;
				if (t < 0.0f) t = 0.0f;
			}
		}
	}
}
