#pragma once
#include <string>
#include <memory>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class Player;
class Input;

/// -------------------------------------------------------------
///			プレイヤーの状態を表すインターフェースクラス
/// -------------------------------------------------------------
class IPlayerState
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// デストラクタ
	virtual ~IPlayerState() = default;

	// ステートに入った瞬間の処理（アニメ再生、初期化など）
	virtual void Initialize(Player* player) = 0;

	//　毎フレーム呼ばれる更新処理（入力や遷移処理など）
	virtual void Update(Player* player) = 0;

	// ステートを抜けるときの後処理
	virtual void Finalize(Player* player) = 0;

	// 状態に応じた追加描画（HUD変更、特殊エフェクトなど）
	virtual void Draw(Player* player) = 0;

protected: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr;

};
