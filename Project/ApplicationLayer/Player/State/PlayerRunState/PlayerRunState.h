#pragma once
#include "IPlayerState.h"

/// -------------------------------------------------------------
///			プレイヤーの走行状態を表すクラス
/// -------------------------------------------------------------
class PlayerRunState : public IPlayerState
{
public: /// ---------- 純粋仮想関数のオーバーライド ---------- ///

	// ステートに入った瞬間の処理（アニメ再生、初期化など）
	void Initialize(Player* player) override;
	
	//　毎フレーム呼ばれる更新処理（入力や遷移処理など）
	void Update(Player* player) override;
	
	// ステートを抜けるときの後処理
	void Finalize(Player* player) override;
	
	// 状態に応じた追加描画（HUD変更、特殊エフェクトなど）
	void Draw(Player* player) override;

private: /// ---------- メンバ変数 ---------- ///

	const std::string modelFilePath_ = "PlayerStateModel/PlayerRunState.gltf"; // モデルファイルパス
};

