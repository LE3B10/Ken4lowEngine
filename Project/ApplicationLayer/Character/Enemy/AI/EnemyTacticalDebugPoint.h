#pragma once
#include <Vector3.h>

// 戦闘中に敵AIが検討する移動候補点のデバッグ情報
struct EnemyTacticalDebugPoint
{
	Ken4lowEngine::Vector3 position; // 候補点の位置
	float score; // 候補点のスコア（高いほど良い）
	bool valid; // 候補点が有効かどうか（例えば、障害物に埋まっているなどで無効な場合がある）
	bool selected; // 現在のAIの選択点かどうか
};