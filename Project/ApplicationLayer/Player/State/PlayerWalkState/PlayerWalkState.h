#pragma once
#include "IPlayerState.h"

// -------------------------------------------------------------
//			プレイヤーの歩行状態を表すクラス
// -------------------------------------------------------------
class PlayerWalkState : public IPlayerState
{
public: /// ---------- 純粋仮想関数のオーバーライド ---------- ///
	
	// ステートに入った瞬間の処理（アニメ再生、初期化など）
	void Initialize(Player* player) ;
	
	//　毎フレーム呼ばれる更新処理（入力や遷移処理など）
	void Update(Player* player);
	
	// ステートを抜けるときの後処理
	void Finalize(Player* player) override;

	// 状態に応じた追加描画（HUD変更、特殊エフェクトなど）
	void Draw(Player* player) ;

private:

	const std::string modelFilePath_ = "PlayerStateModel/humanWalking.gltf"; // モデルファイルパス
};

