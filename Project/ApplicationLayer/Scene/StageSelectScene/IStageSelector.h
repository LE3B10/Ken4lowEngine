#pragma once
#include <functional>
#include <vector>
#include <string>
#include <Vector4.h>

/// ---------- シーン側から渡す共有コンテキスト ---------- ///
struct StageInfo
{
	uint32_t id = {};			  // ステージID
	std::string name;			  // ステージ名
	std::string thumbPath;		  // サムネイル画像パス
	bool locked = false;		  // ロックされているかどうか
	uint32_t startsCompleted = 0; // クリア済みスター数
	Vector4 color = { 1,1,1,1 };  // ステージカラー（背景用）
};

/// ---------- コンテキスト ---------- ///
struct SelectorContext
{
	float screenWidth = 1280.0f;  // 画面幅
	float screenHeight = 720.0f;  // 画面高さ
	class Input* input = nullptr; // 入力インターフェース
	class FadeController* fadeController = nullptr; // フェードコントローラー
	const std::vector<StageInfo>* stages = nullptr; // ステージ情報リスト（外部管理）

	// コールバック
	std::function<void(uint32_t)> onRequestPlay; // プレイ要求コールバック
	std::function<void(uint32_t)> onRequestMap;  // マップ要求コールバック
	std::function<void()> onRequestBack;         // 戻る要求コールバック
};

/// -------------------------------------------------------------
///				　ステージセレクターインターフェース
/// -------------------------------------------------------------
class IStageSelector
{
public: /// ---------- メンバ関数 ---------- ///

	virtual ~IStageSelector() = default;

	// 初期化処理 : 依存注入：コンテキストを受け取る
	virtual void Initialize(const SelectorContext& context) = 0;

	// 更新処理 : ⊿タイム付き
	virtual void Update(float deltaTime) = 0;

	// 3Dオブジェクトの描画
	virtual void Draw3DObjects() = 0;

	// 2Dオブジェクトの描画
	virtual void Draw2DSprites() = 0;

	// モード切替のフック
	virtual void OnEnter() = 0;

	// モード切替のフック
	virtual void OnExit() = 0;

	// 外部からの指示用
	virtual void FocusToIndex(int index, bool tween = true) = 0; // 指定インデックスへフォーカス
};

