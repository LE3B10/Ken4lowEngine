#pragma once
#include "Object3D.h"
#include <WorldTransformEx.h>
#include "WeaponConfig.h"

#include "Collider.h"

#include <memory>
#include <vector>

namespace K4E = ::Ken4lowEngine;

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
		K4E::Object3D* object; // 3Dオブジェクト
		K4E::Vector3 p0; // 始点
		K4E::Vector3 p1; // 終点
		float age;        // 経過時間
		float life;       // 残り寿命
		float width;      // 幅
		K4E::Vector4 color;      // 色
		bool alive;       // 生存フラグ
		bool attached = false; // 銃口に追従するか
		uint32_t ownerId = 0;   // 所有者ID（追従用）
	};

	// 弾の情報（将来拡張用）
	struct Bullet
	{
		K4E::Vector3 position; // 座標
		K4E::Vector3 velocity; // 速度
		bool alive;       // 生存フラグ
		float traveled;    // 移動距離
		uint32_t userShotCount; // 発射からのフレーム数（トレーサ間引き用）
	};

	// 衝突判定用の弾情報
	// K4E::Collider は CollisionManager へ登録されるため、破棄時に RemoveCollider してから delete する。
	// （定義は BallisticEffect.cpp 側で行う：CollisionManager の完全型が必要なため）
	struct ColliderDeleter
	{
		CollisionManager* mgr = nullptr;
		void operator()(K4E::Collider* p) const noexcept;
	};
	using ColliderPtr = std::unique_ptr<K4E::Collider, ColliderDeleter>;

	struct ColliderBullet
	{
		K4E::Vector3 position{};
		K4E::Vector3 prev{};
		K4E::Vector3 velocity{};
		float   traveled = 0.0f;
		bool    alive = false;
		uint32_t userShotCount = 0;
		ColliderPtr collider{}; // 衝突専用（RemoveCollider→delete を自動化）
	};

	// マズルフラッシュ
	struct MuzzleFlash
	{
		K4E::Object3D* object = nullptr;  // プールから借りる
		K4E::Vector3   pos{};
		K4E::Vector3   dir{ 0,0,1 };
		float     age = 0.0f;
		float     life = 0.06f;
		float     startLen = 0.2f, endLen = 0.05f;
		float     startWid = 0.10f, endWid = 0.03f;
		K4E::Vector4   color{ 1,1,1,1 };
		bool      alive = false;
	};

	// 火花
	struct Spark
	{
		K4E::Object3D* object = nullptr;
		K4E::Vector3   pos{};
		K4E::Vector3   vel{};
		float     age = 0.0f;
		float     life = 0.1f;
		float     width = 0.018f;
		K4E::Vector4   col0{ 1,1,1,1 };  // 開始色
		K4E::Vector4   col1{ 1,0,0,0 };  // 終了色（α0）
		bool      alive = false;
	};

	// 薬莢
	struct Casing
	{
		K4E::Object3D* object = nullptr;
		K4E::Vector3 pos{};
		K4E::Vector3 vel{};
		K4E::Vector3 ang{};     // 回転角
		K4E::Vector3 angVel{};  // 角速度
		float   age = 0.0f;
		float   life = 0.125f;
		bool    alive = false;
		K4E::Vector4 color{ 1,1,1,1 };
		K4E::Vector3 scale{ 0.04f,0.04f,0.12f };
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
	void Start(const K4E::Vector3& position, const K4E::Vector3& velocity, const WeaponConfig& weapon);

public: /// ---------- セッター・ゲッター ---------- ///

	// プレイヤーのボディを基準にするオフセットを設定
	void SetPlayerBodyTransform(const K4E::WorldTransformEx& bodyTransform, const K4E::Vector3& offset) {
		parentTransform_ = &bodyTransform;
		offset_ = offset;
		transform_.parent_ = const_cast<K4E::WorldTransformEx*>(&bodyTransform);
	}

	// 親Transformを設定
	void SetParentTransform(const K4E::WorldTransformEx* parent) {
		parentTransform_ = parent;
		transform_.parent_ = const_cast<K4E::WorldTransformEx*>(parent);
	}

	void SetWorldTransform(const K4E::WorldTransformEx& transform) { transform_ = transform; }
	K4E::WorldTransformEx& GetWorldTransform() { return transform_; }

	// 銃口のワールド座標（親＋offset_）を返す
	K4E::Vector3 GetMuzzleWorld() const;

	// 当たり判定管理を渡す
	void SetCollisionManager(CollisionManager* mgr) {
		collisionMgr_ = mgr;
		// 既に生成済みのコライダーがあれば、デリータに mgr を差し替える（後から Set しても安全）
		for (auto& b : colliderBullets_) {
			if (b.collider) {
				b.collider.get_deleter().mgr = mgr;
			}
		}
	}

	// 当たり判定を登録
	void RegisterColliders(CollisionManager* mgr);

private: /// ---------- メンバ関数 ---------- ///

	// セグメントを1本追加（前pos→今pos）
	void PushTrail(const K4E::Vector3& p0, const K4E::Vector3& p1, float speed, const WeaponConfig& weapon);

	// マズルフラッシュを追加
	void SpawnMuzzleFlash(const K4E::Vector3& position, const K4E::Vector3& forward, const WeaponConfig& weapon);

	// 生成関数
	void SpawnMuzzleSparks(const K4E::Vector3& pos, const K4E::Vector3& forward, const WeaponConfig& weapon);

	// 薬莢の生成
	void SpawnCasing(const K4E::Vector3& basePos, const K4E::Vector3& forward, const WeaponConfig& weapon);

private: /// ---------- メンバ変数 ---------- ///

	// 当たり判定管理
	CollisionManager* collisionMgr_ = nullptr;

	// ワールド変換
	K4E::WorldTransformEx transform_; // 自身のTransform
	const K4E::WorldTransformEx* parentTransform_ = nullptr;
	K4E::Vector3 offset_ = { 0.0f, 0.325f, 2.5f };

	K4E::WorldTransformEx playerBodyTransform_; // プレイヤーボディTransform（オフセット基準用）

	WeaponConfig currentWeapon_; // 現在の武器設定
	uint32_t shotCounter_ = 0;    // 発射カウンタ（トレーサ間引き用）

	std::vector<std::unique_ptr<K4E::Object3D>> objectPool_; // 実体の保持（破棄は自動）
	std::vector<K4E::Object3D*> freeList_;                   // 空きポインタ

	std::vector<TrailSegment> trails_; // 軌跡セグメントの配列
	std::vector<Bullet> bullets_; // 弾の配列（将来拡張用）
	std::vector<ColliderBullet> colliderBullets_;

	// 物理&見た目パラメータ
	float gravityY_ = -9.8f;   // m/s^2
	float drag_ = 0.05f;   // 空気抵抗係数（0で無効）
	float maxLife_ = 0.25f;   // セグメント寿命
	float baseWidth_ = 0.02f;   // 基本太さ
	K4E::Vector4 tracerColor_ = { 0.8f,1.0f,0.6f,1.0f };

	uint32_t maxSegments_ = 512; // 最大セグメント数
	float minSegLength_ = 0.02f; // セグメント最小長さ

	float bulletMaxDistance_ = 200.0f; // 最大飛距離[m]（好みで調整）

	std::vector<MuzzleFlash> flashes_;

	// マズルフラッシュ用プール
	std::vector<std::unique_ptr<K4E::Object3D>> flashPool_;
	std::vector<K4E::Object3D*> flashFree_;
	uint32_t maxFlashes_ = 64; // マズルフラッシュの最大数

	std::vector<Spark> sparks_;

	// スパーク用プール
	std::vector<std::unique_ptr<K4E::Object3D>> sparkPool_;
	std::vector<K4E::Object3D*> sparkFree_;
	uint32_t maxSparks_ = 256;

	// 薬莢用プール（将来用）
	std::vector<Casing> casings_;

	std::vector<std::unique_ptr<K4E::Object3D>> casingPool_;
	std::vector<K4E::Object3D*> casingFree_;
	uint32_t maxCasings_ = 256;
};

