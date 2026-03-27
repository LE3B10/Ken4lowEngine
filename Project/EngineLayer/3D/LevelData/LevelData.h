#pragma once
#include <string>
#include <vector>
#include <Vector3.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				　	ボックスコライダーデータ構造体
	/// -------------------------------------------------------------
	struct ObjectColliderData
	{
		std::string type;		// コライダーの名前
		Vector3 center;			// 中心位置
		Vector3 size;			// サイズ
		bool enabled = false;	// 有効フラグ
	};

	/// -------------------------------------------------------------
	///				　	スポーンポイントデータ構造体
	/// -------------------------------------------------------------
	struct SpawnProps
	{
		int wave = 0;   // ウェーブ数
		int group = 0;  // グループ数
		int count = 1;  // スポーンする敵の数

		std::string archetype;
		bool hasArchetype = false;
	};

	struct IntroCameraProps
	{
		int order = 0;			// カメラの切り替わる順番
		float duration = 1.5f;	// カメラの切り替わる時間（秒）
		float fov = 60.0f;		// カメラのFOV（度）
		std::string targetName; // カメラの注視点となるオブジェクトの名前

		std::string interpMode = "Linear"; // 補間モード
		std::string aimMode = "Target";    // 注視点のモード
	};

	/// -------------------------------------------------------------
	///				　		オブジェクトデータ構造体
	/// -------------------------------------------------------------
	struct ObjectData
	{
		std::string name;      // オブジェクトの名前
		std::string type;      // オブジェクトのタイプ
		std::string modelName; // モデル名

		Vector3 position;                 // 位置
		Vector3 rotation;                 // 回転
		Vector3 scale{ 1.0f,1.0f,1.0f };  // スケール
		ObjectColliderData collider;      // コライダーデータ

		// SpawnPoint用
		SpawnProps spawnProps;      // スポーンプロパティ
		bool hasSpawnProps = false; // スポーンプロパティが有効かどうか

		// IntroCamera用
		IntroCameraProps introCameraProps;      // イントロカメラプロパティ
		bool hasIntroCameraProps = false;       // イントロカメラプロパティが有効かどうか
	};

	/// -------------------------------------------------------------
	///				　		レベルデータ構造体
	/// -------------------------------------------------------------
	struct LevelData
	{
		std::vector<ObjectData> objects; // レベル内のオブジェクトデータ
	};

} // namespace Ken4lowEngine