#pragma once
#include "AnimationModel.h"


// -------------------------------------------------------------
//			プレイヤーの歩行状態を表すクラス
// -------------------------------------------------------------
class PlayerWalkState
{
public: /// ---------- 純粋仮想関数のオーバーライド ---------- ///
	
	// ステートに入った瞬間の処理（アニメ再生、初期化など）
	void Initialize() ;
	
	//　毎フレーム呼ばれる更新処理（入力や遷移処理など）
	void Update();
	
	// 状態に応じた追加描画（HUD変更、特殊エフェクトなど）
	void Draw() ;

private:

	std::unique_ptr<AnimationModel> animationModel_; // アニメーションモデル

	const std::string modelFilePath_ = "PlayerStateModel/PlayerWalkAnimation.gltf"; // モデルファイルパス
};

