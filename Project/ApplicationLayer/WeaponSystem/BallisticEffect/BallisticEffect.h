#pragma once
#include "Object3D.h"
#include <WorldTransformEx.h>
#include "WeaponConfig.h"

#include "Collider.h"

#include <memory>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class CollisionManager;

/// -------------------------------------------------------------
///				　		　弾道エフェクト
/// -------------------------------------------------------------
class BallisticEffect
{
private: /// ---------- 構造体 ---------- ///

	// 軌跡のセグメント
	struct TrailSegment
	{
		Object3D* object; // 3Dオブジェクト
		Vector3 p0; // 始点
		Vector3 p1; // 終点
		float age;        // 経過時間
		float life;       // 残り寿命
		float width;      // 幅
		Vector4 color;      // 色
		bool alive;       // 生存フラグ
		bool attached = false; // 銃口に追従するか
		uint32_t ownerId = 0;   // 所有者ID（追従用）
	};

	// 弾の情報（将来拡張用）
	struct Bullet
	{
		Vector3 position; // 座標
		Vector3 velocity; // 速度
		bool alive;       // 生存フラグ
		float traveled;    // 移動距離
		uint32_t userShotCount; // 発射からのフレーム数（トレーサ間引き用）

		Collider* collider = nullptr; // 衝突判定用コライダー 
	};

	// マズルフラッシュ
	struct MuzzleFlash
	{
		Object3D* object = nullptr;  // プールから借りる
		Vector3   pos{};
		Vector3   dir{ 0,0,1 };
		float     age = 0.0f;
		float     life = 0.06f;
		float     startLen = 0.2f, endLen = 0.05f;
		float     startWid = 0.10f, endWid = 0.03f;
		Vector4   color{ 1,1,1,1 };
		bool      alive = false;
	};

	// 火花
	struct Spark
	{
		Object3D* object = nullptr;
		Vector3   pos{};
		Vector3   vel{};
		float     age = 0.0f;
		float     life = 0.1f;
		float     width = 0.018f;
		Vector4   col0{ 1,1,1,1 };  // 開始色
		Vector4   col1{ 1,0,0,0 };  // 終了色（α0）
		bool      alive = false;
	};

	// 薬莢
	struct Casing
	{
		Object3D* object = nullptr;
		Vector3 pos{};
		Vector3 vel{};
		Vector3 ang{};     // 回転角
		Vector3 angVel{};  // 角速度
		float   age = 0.0f;
		float   life = 0.125f;
		bool    alive = false;
		Vector4 color{ 1,1,1,1 };
		Vector3 scale{ 0.04f,0.04f,0.12f };
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

	// 弾道開始
	void Start(const Vector3& position, const Vector3& velocity, const WeaponConfig& weapon);

public: /// ---------- セッター・ゲッター ---------- ///

	// 親Transformを設定
	void SetParentTransform(const WorldTransformEx* parent) {
		parentTransform_ = parent;
		transform_.parent_ = const_cast<WorldTransformEx*>(parent);
	}

	void SetWorldTransform(const WorldTransformEx& transform) { transform_ = transform; }
	WorldTransformEx& GetWorldTransform() { return transform_; }

	// 銃口のワールド座標（親＋offset_）を返す
	Vector3 GetMuzzleWorld() const;

	// 当たり判定管理を渡す
	void SetCollisionManager(CollisionManager* mgr) { collisionMgr_ = mgr; }

	// 当たり判定を登録
	void RegisterColliders(CollisionManager* mgr);

private: /// ---------- メンバ関数 ---------- ///

	// セグメントを1本追加（前pos→今pos）
	void PushTrail(const Vector3& p0, const Vector3& p1, float speed, const WeaponConfig& weapon);

	// マズルフラッシュを追加
	void SpawnMuzzleFlash(const Vector3& position, const Vector3& forward, const WeaponConfig& weapon);

	// 生成関数
	void SpawnMuzzleSparks(const Vector3& pos, const Vector3& forward, const WeaponConfig& weapon);

	// 薬莢の生成
	void SpawnCasing(const Vector3& basePos, const Vector3& forward, const WeaponConfig& weapon);

private: /// ---------- メンバ変数 ---------- ///

	// 当たり判定管理
	CollisionManager* collisionMgr_ = nullptr;

	// ワールド変換
	WorldTransformEx transform_;
	const WorldTransformEx* parentTransform_ = nullptr;
	Vector3 offset_ = { 0.0f, 0.325f, 2.5f };

	WeaponConfig currentWeapon_; // 現在の武器設定
	uint32_t shotCounter_ = 0;    // 発射カウンタ（トレーサ間引き用）

	std::vector<std::unique_ptr<Object3D>> objectPool_; // 実体の保持（破棄は自動）
	std::vector<Object3D*> freeList_;                   // 空きポインタ

	std::vector<TrailSegment> trails_; // 軌跡セグメントの配列
	std::vector<Bullet> bullets_; // 弾の配列（将来拡張用）

	// 物理&見た目パラメータ
	float gravityY_ = -9.8f;   // m/s^2
	float drag_ = 0.05f;   // 空気抵抗係数（0で無効）
	float maxLife_ = 0.25f;   // セグメント寿命
	float baseWidth_ = 0.02f;   // 基本太さ
	Vector4 tracerColor_ = { 0.8f,1.0f,0.6f,1.0f };

	uint32_t maxSegments_ = 512; // 最大セグメント数
	float minSegLength_ = 0.02f; // セグメント最小長さ

	float bulletMaxDistance_ = 200.0f; // 最大飛距離[m]（好みで調整）

	std::vector<MuzzleFlash> flashes_;

	// マズルフラッシュ用プール
	std::vector<std::unique_ptr<Object3D>> flashPool_;
	std::vector<Object3D*> flashFree_;
	uint32_t maxFlashes_ = 64; // マズルフラッシュの最大数

	std::vector<Spark> sparks_;

	// スパーク用プール
	std::vector<std::unique_ptr<Object3D>> sparkPool_;
	std::vector<Object3D*> sparkFree_;
	uint32_t maxSparks_ = 256;

	// 薬莢用プール（将来用）
	std::vector<Casing> casings_;

	std::vector<std::unique_ptr<Object3D>> casingPool_;
	std::vector<Object3D*> casingFree_;
	uint32_t maxCasings_ = 256;
};

