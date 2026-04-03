#pragma once
#include <vector>
#include <cfloat>
#include <cmath>
#include "Vector3.h"
#include "AABB.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　		ワールド衝突結果構造体
/// -------------------------------------------------------------
struct WorldCollisionResult
{
	Vector3 fixedCenter{};
	bool grounded = false;   // Player用（Boss/Enemyは無視でOK）
};

/// -------------------------------------------------------------
///				　		ワールド衝突設定構造体
/// -------------------------------------------------------------
struct WorldCollisionSettings
{
	Vector3 half{ 0.8f, 2.0f, 0.8f };
	Vector3 centerOffset{ 0.0f, 0.0f, 0.0f }; // 見た目座標→物理中心の差
	float eps = 0.002f;
};

/// -------------------------------------------------------------
///				　		ワールド衝突解決クラス
/// -------------------------------------------------------------
class WorldCollisionResolver
{
public: /// ---------- 静的メンバ関数 ---------- ///

	/// <summary>
	/// ワールド内のAABB群と移動前後の座標をもとに衝突を検出・解決する静的関数。プレイヤーの着地判定やジャンプ速度の更新に対応する。
	/// </summary>
	/// <param name="worldAABBs">ワールド内の AABB（軸平行境界ボックス）の配列。衝突判定の対象となる環境データ。</param>
	/// <param name="s">WorldCollisionSettings 型の設定。衝突解決に用いるパラメータ（閾値やマージンなど）。</param>
	/// <param name="oldTranslate">描画座標での移動前の位置。（コメント通り、従来通り oldPos を渡す）</param>
	/// <param name="newTranslate">描画座標での移動後の位置。（現在の body_.transform.translate_ を渡す）</param>
	/// <param name="useGrounded">着地判定を考慮するかどうか。プレイヤーでは true を指定することが想定される。</param>
	/// <param name="inoutJumpVelocity">（オプション）プレイヤー用の入出力ジャンプ速度へのポインタ。衝突解決によりジャンプ速度を修正する場合に値が書き換えられる。Boss/Enemy 等では nullptr を渡す。デフォルトは nullptr。</param>
	/// <returns>WorldCollisionResult 型。衝突の有無や補正後の位置・法線、着地状態など、衝突解決の結果を含む。</returns>
	static WorldCollisionResult Resolve(
		const std::vector<AABB>& worldAABBs,
		const WorldCollisionSettings& s,
		const Vector3& oldTranslate,      // 描画座標（今まで通り oldPos を渡す）
		const Vector3& newTranslate,      // 描画座標（今の body_.transform.translate_）
		bool useGrounded,                 // Playerだけ true
		float* inoutJumpVelocity = nullptr // Playerだけ渡す（Boss/Enemyはnullptr）
	);
};


} // namespace Ken4lowEngine
