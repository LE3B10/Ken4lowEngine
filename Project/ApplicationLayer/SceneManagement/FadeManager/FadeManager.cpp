#define NOMINMAX
#include "FadeManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <DirectXCommon.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

// ------------------------------
// 補助
// ------------------------------
float FadeManager::Clamp01(float v)
{
	return std::max(0.0f, std::min(1.0f, v));
}

float FadeManager::Lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

Vector2 FadeManager::Lerp(const Vector2& a, const Vector2& b, float t)
{
	return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t) };
}

// ちょい「ドン！」と収束する設置感（Back ease）
float FadeManager::EaseOutBack(float t)
{
	t = Clamp01(t);
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;
	return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

float FadeManager::EaseOutCubic(float t)
{
	t = Clamp01(t);
	float u = 1.0f - t;
	return 1.0f - (u * u * u);
}


float FadeManager::RandRange(std::mt19937& rng, float a, float b)
{
	std::uniform_real_distribution<float> dist(a, b);
	return dist(rng);
}

// ------------------------------
// 初期化
// ------------------------------
void FadeManager::Initialize()
{
	auto* dx = DirectXCommon::GetInstance();
	screenW_ = dx->GetClientWidth();
	screenH_ = dx->GetClientHeight();

	RebuildTiles(screenW_, screenH_);

#ifdef USE_IMGUI
	strncpy_s(tileTexBuf_, sizeof(tileTexBuf_), tileTexturePath_.c_str(), _TRUNCATE);
	strncpy_s(crackTexBuf_, sizeof(crackTexBuf_), crackAtlasPath_.c_str(), _TRUNCATE);
#endif

	state_ = State::None;
	stateTime_ = 0.0f;
	crackDone_ = false;
	dropDone_ = false;
}

// ------------------------------
// タイル再構築（画面全体を埋める）
// ------------------------------
void FadeManager::RebuildTiles(int screenW, int screenH)
{
	tiles_.clear();

	tilesX_ = (int)std::ceil(screenW / tileSize_.x);
	tilesY_ = (int)std::ceil(screenH / tileSize_.y);

	const int total = tilesX_ * tilesY_;
	if (total <= 0)
	{
		return;
	}

	tiles_.reserve((size_t)total);

	// -----------------------------------------
	// orderCover: 置く順（中心から外へ + ちょいランダム）
	// orderCrack: 割る順（別の揺らぎで波を作る）
	// ※重要：sort comparator の中で乱数を呼ばない（invalid comparator対策）
	// -----------------------------------------
	std::vector<int> orderCover;
	orderCover.reserve((size_t)total);
	for (int i = 0; i < total; ++i) orderCover.push_back(i);

	std::vector<int> orderCrack = orderCover;

	const Vector2 center = { screenW * 0.5f, screenH * 0.5f };

	std::vector<float> keyCover(total);
	std::vector<float> keyCrack(total);

	for (int idx = 0; idx < total; ++idx)
	{
		int x = idx % tilesX_;
		int y = idx / tilesX_;

		Vector2 c = { (x + 0.5f) * tileSize_.x, (y + 0.5f) * tileSize_.y };
		float dx = c.x - center.x;
		float dy = c.y - center.y;
		float d = std::sqrt(dx * dx + dy * dy);

		// 乱数はここで1回だけ確定（比較中には呼ばない）
		keyCover[idx] = d + RandRange(rng_, -40.0f, 40.0f);
		keyCrack[idx] = d + RandRange(rng_, -80.0f, 80.0f);
	}

	std::sort(orderCover.begin(), orderCover.end(), [&](int a, int b)
		{
			return keyCover[a] < keyCover[b];
		});

	std::sort(orderCrack.begin(), orderCrack.end(), [&](int a, int b)
		{
			return keyCrack[a] < keyCrack[b];
		});

	// idx -> crackRank 変換（delayCrack を割り当てるため）
	std::vector<int> crackRank(total, 0);
	for (int rank = 0; rank < total; ++rank)
	{
		crackRank[orderCrack[rank]] = rank;
	}

	// delay を 0..Total に割り当て
	const int N = std::max(1, total);

	for (int rank = 0; rank < total; ++rank)
	{
		int idx = orderCover[rank];
		int x = idx % tilesX_;
		int y = idx / tilesX_;

		Tile t{};

		// -----------------------------------------
		// base
		// -----------------------------------------
		t.base = std::make_unique<Sprite>();
		t.base->Initialize(tileTexturePath_);
		t.base->SetAnchorPoint({ 0.5f, 0.5f }); // 回転中心

		// -----------------------------------------
		// crack
		// -----------------------------------------
		t.crack = std::make_unique<Sprite>();
		t.crack->Initialize(crackAtlasPath_);
		t.crack->SetAnchorPoint({ 0.5f, 0.5f });
		t.crack->SetColor({ 1,1,1,0 }); // 初期は非表示

		// 完成位置（中心）
		t.targetCenter = { (x + 0.5f) * tileSize_.x, (y + 0.5f) * tileSize_.y };

		// 出現開始：上から落ちてくる + ちょい横ブレ
		t.startCenter = t.targetCenter;
		t.startCenter.x += RandRange(rng_, -160.0f, 160.0f);
		t.startCenter.y -= coverSpawnYOffset_ + RandRange(rng_, 0.0f, 260.0f);

		t.pos = t.startCenter;

		// 初期回転とスケール（設置感）
		t.startRot = RandRange(rng_, -3.14159f, 3.14159f);
		t.rot = t.startRot;

		t.startScale = RandRange(rng_, 0.15f, 0.35f);
		t.scale = t.startScale;

		// 設置の遅延（0..coverStaggerTotal_）
		{
			float u = (N <= 1) ? 0.0f : (float)rank / (float)(N - 1);
			t.delayCover = u * coverStaggerTotal_;
		}

		// ひび割れの遅延（0..crackStaggerTotal_）
		{
			int r = crackRank[idx];
			float u = (N <= 1) ? 0.0f : (float)r / (float)(N - 1);
			t.delayCrack = u * crackStaggerTotal_;
		}

		// ドロップの遅延（0..dropStaggerTotal_） ※ヒビの順と揃えると気持ちいい
		{
			int r = crackRank[idx];
			float u = (N <= 1) ? 0.0f : (float)r / (float)(N - 1);
			t.delayDrop = u * dropStaggerTotal_;
		}

		t.placed = false;
		t.dead = false;

		// 初期反映（base）
		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		// 初期反映（crack）
		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->SetColor({ 1,1,1,0 });
		t.crack->SetUVRect({ 0.0f, 0.0f }, crackFrameSizePx_); // stage0
		t.crack->Update();

		tiles_.push_back(std::move(t));
	}

	crackDone_ = false;
	dropDone_ = false;
}

// ------------------------------
// 開始：カバー
// ------------------------------
void FadeManager::StartCover()
{
	// 画面サイズが変わってたら作り直す
	auto* dx = DirectXCommon::GetInstance();
	int w = dx->GetClientWidth();
	int h = dx->GetClientHeight();

	if (w != screenW_ || h != screenH_)
	{
		screenW_ = w;
		screenH_ = h;
		RebuildTiles(screenW_, screenH_);
	}

	// 位置などを初期化（再スタート用）
	for (auto& t : tiles_)
	{
		t.pos = t.startCenter;
		t.rot = t.startRot;
		t.scale = t.startScale;
		t.placed = false;
		t.dead = false;

		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->SetColor({ 1,1,1,0 }); // カバー開始時はヒビ無し
		t.crack->SetUVRect({ 0.0f, 0.0f }, crackFrameSizePx_);
		t.crack->Update();
	}

	state_ = State::TileCover;
	stateTime_ = 0.0f;
	crackDone_ = false;
	dropDone_ = false;
}

// ------------------------------
// 開始：ひび割れ（覆った後に呼ぶ）
// ------------------------------
void FadeManager::StartCrack()
{
	// まだ何もないなら無視
	if (tiles_.empty())
	{
		return;
	}

	// 覆っていない状態で呼ばれても動くようにはしておく（デバッグ用途）
	// 本運用では「Cover完了→シーン切替→StartCrack」を想定
	for (auto& t : tiles_)
	{
		// crackを stage0 に戻して透明に
		t.crack->SetUVRect({ 0.0f, 0.0f }, crackFrameSizePx_);
		t.crack->SetColor({ 1,1,1,0 });
		t.crack->Update();
	}

	state_ = State::Crack;
	stateTime_ = 0.0f;
	crackDone_ = false;
	dropDone_ = false;
}


void FadeManager::StartDrop()
{
	// まだ何もないなら無視
	if (tiles_.empty())
	{
		return;
	}

	// 画面中心から外へ飛ばしつつ、重力で落ちる
	const Vector2 center = { screenW_ * 0.5f, screenH_ * 0.5f };

	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }

		// 落下開始位置は「完成位置」に固定（覆い終わった状態から落とす）
		t.pos = t.targetCenter;
		t.scale = 1.0f;
		t.dropStarted = false;
		t.dropTime = 0.0f;

		// 外側への方向
		Vector2 dir = { t.targetCenter.x - center.x, t.targetCenter.y - center.y };
		float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
		if (len < 0.001f)
		{
			// 中心付近は適当に散らす
			dir = { RandRange(rng_, -1.0f, 1.0f), RandRange(rng_, -1.0f, 1.0f) };
			len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
			if (len < 0.001f) len = 1.0f;
		}
		dir.x /= len;
		dir.y /= len;

		// 初速（外へ + 下へ）
		float kick = dropKickOut_ + RandRange(rng_, -dropKickRand_, dropKickRand_);
		t.vel.x = dir.x * kick + RandRange(rng_, -80.0f, 80.0f);
		t.vel.y = RandRange(rng_, dropKickDownMin_, dropKickDownMax_);

		// 回転
		t.rotVel = RandRange(rng_, dropRotVelMin_, dropRotVelMax_);

		// ひびは見える状態にしておく（割れてる感を保ったまま落下）
		// ※既にCrackが終わっていればalpha=1になっているはずだが、念のため
		// stageは最後(9)に固定
		Vector2 lt = { crackFrameSizePx_.x * (float)(kCrackFrames_ - 1), 0.0f };
		t.crack->SetUVRect(lt, crackFrameSizePx_);
		t.crack->SetColor({ 1,1,1,1 });

		// 描画更新
		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->Update();
	}

	state_ = State::TileUncover; // Drop状態として使う
	stateTime_ = 0.0f;
	dropDone_ = false;
}

// ------------------------------
// 更新
// ------------------------------
void FadeManager::Update(float dt)
{
	// 何もしてないなら何もしない
	if (state_ == State::None)
	{
		return;
	}

	// 画面サイズの変化に追従（ウィンドウリサイズ対応）
	{
		auto* dx = DirectXCommon::GetInstance();
		int w = dx->GetClientWidth();
		int h = dx->GetClientHeight();
		if (w != screenW_ || h != screenH_)
		{
			screenW_ = w;
			screenH_ = h;
			RebuildTiles(screenW_, screenH_);

			// 変化直後はカバー中扱いにしておく
			state_ = State::TileCover;
			stateTime_ = 0.0f;
			crackDone_ = false;
		}
	}

	stateTime_ += dt;

	switch (state_)
	{
	case State::TileCover:
		UpdateCover(dt);
		break;

	case State::Hold:
		// 完全に覆われた状態
		// ここで SceneManager がシーンを切り替え、切り替え後に StartCrack() を呼ぶ想定
		break;

	case State::Crack:
		UpdateCrack(dt);
		break;

	case State::TileUncover:
		UpdateDrop(dt);
		break;

	default:
		break;
	}
}

// ------------------------------
// タイル設置（回転＋拡縮）
// ------------------------------
void FadeManager::UpdateCover(float dt)
{
	(void)dt;

	bool allPlaced = true;

	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }

		// delay後に設置開始
		float local = (stateTime_ - t.delayCover) / std::max(0.0001f, coverTileAnimTime_);
		float u = Clamp01(local);

		float e = EaseOutBack(u);

		t.pos = Lerp(t.startCenter, t.targetCenter, e);
		t.rot = Lerp(t.startRot, 0.0f, e);
		t.scale = Lerp(t.startScale, 1.0f, e);

		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		// crackは非表示のまま位置合わせ
		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->SetColor({ 1,1,1,0 });
		t.crack->Update();

		if (u >= 0.999f)
		{
			t.placed = true;
		}
		else
		{
			allPlaced = false;
		}
	}

	// 全タイル設置完了 → Holdへ
	if (allPlaced)
	{
		state_ = State::Hold;
		stateTime_ = 0.0f;
	}
}

// ------------------------------
// ひび割れアニメーション
// ------------------------------
void FadeManager::UpdateCrack(float dt)
{
	(void)dt;

	bool allDone = true;

	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }

		// delay後にヒビ開始
		float local = (stateTime_ - t.delayCrack) / std::max(0.0001f, crackTileAnimTime_);

		// まだ開始前：非表示のまま
		if (local < 0.0f)
		{
			allDone = false;
			continue;
		}

		float u = Clamp01(local);

		// stage 0..9
		int stage = (int)(u * (float)kCrackFrames_);
		if (stage < 0) stage = 0;
		if (stage > (kCrackFrames_ - 1)) stage = (kCrackFrames_ - 1);

		// alpha：最初だけ少しフェードインさせる
		float a = Clamp01(u * 1.25f); // すぐ見えるように
		if (u >= 1.0f) a = 1.0f;

		Vector2 lt = { crackFrameSizePx_.x * (float)stage, 0.0f };

		t.crack->SetUVRect(lt, crackFrameSizePx_);
		t.crack->SetColor({ 1,1,1,a });
		t.crack->Update();

		if (u < 0.999f)
		{
			allDone = false;
		}
	}

	crackDone_ = allDone;

	// 割れ終わったら、そのままドロップへ
	if (allDone)
	{
		StartDrop();
	}
}


// ------------------------------
// ドロップ更新（重力で落下）
// ------------------------------
void FadeManager::UpdateDrop(float dt)
{
	bool allDead = true;

	// 速度減衰をdtに合わせて（60fps基準）
	float damp = std::pow(dropDamping_, dt * 60.0f);

	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }

		// ドロップの開始待ち（全体の待ち + タイルごとの遅延）
		float local = stateTime_ - dropStartDelay_ - t.delayDrop;
		if (local < 0.0f)
		{
			// まだ落ちないタイルがあるので終わってない
			allDead = false;
			continue;
		}

		// ドロップ開始した瞬間に「アイテム化（縮む）」を開始
		if (!t.dropStarted)
		{
			t.dropStarted = true;
			t.dropTime = 0.0f;
		}

		t.dropTime += dt;

		// 1) スケールを 1.0 -> dropItemScale_ に素早く縮める（アイテム化）
		float shrinkT = Clamp01(t.dropTime / std::max(0.0001f, dropShrinkTime_));
		float se = EaseOutCubic(shrinkT);
		t.scale = Lerp(1.0f, dropItemScale_, se);

		// 2) ひび割れはドロップ開始と同時に消して「アイテムが落ちる」っぽくする
		float crackT = Clamp01(t.dropTime / std::max(0.0001f, dropCrackFadeTime_));
		float crackA = 1.0f - crackT;

		// 物理（y+が下想定）
		t.vel.y += dropGravity_ * dt;
		t.vel.x *= damp;
		t.vel.y *= damp;

		t.pos.x += t.vel.x * dt;
		t.pos.y += t.vel.y * dt;

		t.rot += t.rotVel * dt;

		// 画面外に出たら消す（中心基準なのでスケールを考慮）
		float halfH = (tileSize_.y * t.scale) * 0.5f;
		if (t.pos.y - halfH > (float)screenH_ + dropKillMargin_)
		{
			t.dead = true;

			// 念のため透明化
			t.base->SetColor({ 1,1,1,0 });
			t.base->Update();
			t.crack->SetColor({ 1,1,1,0 });
			t.crack->Update();
			continue;
		}

		// 更新（base）※縮んだ小さいアイテムとして描く
		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		// 更新（crack）※フェードで消す
		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->SetColor({ 1,1,1,crackA });
		t.crack->Update();

		allDead = false;
	}

	if (allDead)
	{
		dropDone_ = true;
		state_ = State::None;
		stateTime_ = 0.0f;
	}
}

// ------------------------------
// 描画
// ------------------------------
void FadeManager::Draw2DSprites()
{
	if (state_ == State::None)
	{
		return;
	}

	// base（ブロック）
	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }
		t.base->Draw();
	}

	// crack（Crack中または割れ完了後も描く。alpha=0のタイルは何も見えない）
	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }
		t.crack->Draw();
	}
}

// ------------------------------
// ImGui
// ------------------------------
void FadeManager::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("FadeManager - Tile Fade");

	const char* st =
		(state_ == State::None) ? "None" :
		(state_ == State::TileCover) ? "TileCover" :
		(state_ == State::Hold) ? "Hold" :
		(state_ == State::Crack) ? "Crack" :
		(state_ == State::TileUncover) ? "TileUncover" : "Unknown";

	ImGui::Text("State: %s", st);
	ImGui::Text("CrackDone: %s", crackDone_ ? "true" : "false");
	ImGui::Text("DropDone : %s", dropDone_ ? "true" : "false");

	ImGui::Separator();

	if (ImGui::Button("Start Cover (Place Tiles)"))
	{
		StartCover();
	}
	ImGui::SameLine();
	if (ImGui::Button("Start Crack Animation"))
	{
		StartCrack();
	}
	ImGui::SameLine();
	if (ImGui::Button("Start Drop"))
	{
		StartDrop();
	}

	ImGui::SeparatorText("Tile Settings");

	bool rebuild = false;
	rebuild |= ImGui::SliderFloat2("Tile Size", &tileSize_.x, 32.0f, 256.0f);

	ImGui::SeparatorText("Cover Params");
	ImGui::SliderFloat("Cover Tile Anim Time", &coverTileAnimTime_, 0.05f, 0.6f);
	ImGui::SliderFloat("Cover Stagger Total", &coverStaggerTotal_, 0.0f, 1.0f);
	ImGui::SliderFloat("Spawn Y Offset", &coverSpawnYOffset_, 0.0f, 1400.0f);

	ImGui::SeparatorText("Crack Params");
	ImGui::SliderFloat("Crack Tile Anim Time", &crackTileAnimTime_, 0.05f, 2.0f);
	ImGui::SliderFloat("Crack Stagger Total", &crackStaggerTotal_, 0.0f, 1.5f);

	ImGui::SeparatorText("Drop Params");
	ImGui::SliderFloat("Drop Start Delay", &dropStartDelay_, 0.0f, 1.0f);
	ImGui::SliderFloat("Drop Item Scale", &dropItemScale_, 0.02f, 0.60f);
	ImGui::SliderFloat("Drop Shrink Time", &dropShrinkTime_, 0.01f, 0.40f);
	ImGui::SliderFloat("Drop Crack Fade Time", &dropCrackFadeTime_, 0.01f, 0.40f);
	ImGui::SliderFloat("Drop Stagger Total", &dropStaggerTotal_, 0.0f, 1.5f);
	ImGui::SliderFloat("Drop Gravity", &dropGravity_, 0.0f, 8000.0f);
	ImGui::SliderFloat("Drop Damping", &dropDamping_, 0.90f, 0.999f);
	ImGui::SliderFloat("Drop Kick Out", &dropKickOut_, 0.0f, 2000.0f);
	ImGui::SliderFloat("Drop Kick Rand", &dropKickRand_, 0.0f, 800.0f);
	ImGui::SliderFloat("Drop Kick Down Min", &dropKickDownMin_, -2000.0f, 2000.0f);
	ImGui::SliderFloat("Drop Kick Down Max", &dropKickDownMax_, -2000.0f, 2000.0f);
	ImGui::SliderFloat("Drop RotVel Min", &dropRotVelMin_, -40.0f, 0.0f);
	ImGui::SliderFloat("Drop RotVel Max", &dropRotVelMax_, 0.0f, 40.0f);
	ImGui::SliderFloat("Drop Kill Margin", &dropKillMargin_, 0.0f, 1200.0f);

	ImGui::SeparatorText("Textures");

	ImGui::InputText("Tile Texture", tileTexBuf_, sizeof(tileTexBuf_));
	ImGui::InputText("Crack Atlas", crackTexBuf_, sizeof(crackTexBuf_));

	if (ImGui::Button("Apply Texture Paths"))
	{
		tileTexturePath_ = tileTexBuf_;
		crackAtlasPath_ = crackTexBuf_;
		rebuild = true;
	}

	rebuild |= ImGui::SliderFloat2("Crack Frame(px)", &crackFrameSizePx_.x, 8.0f, 512.0f);

	if (ImGui::Button("Rebuild Tiles Now"))
	{
		rebuild = true;
	}

	if (rebuild)
	{
		auto* dx = DirectXCommon::GetInstance();
		screenW_ = dx->GetClientWidth();
		screenH_ = dx->GetClientHeight();
		RebuildTiles(screenW_, screenH_);
	}

	ImGui::End();
#else
	// ImGui無効なら何もしない
#endif
}

// ------------------------------
// 終了
// ------------------------------
void FadeManager::Finalize()
{
	tiles_.clear();
	state_ = State::None;
	stateTime_ = 0.0f;
	crackDone_ = false;
	dropDone_ = false;
}
