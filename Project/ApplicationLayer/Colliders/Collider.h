#pragma once
#include <cstdint>
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "IDGenerator.h"

#include "OBB.h"
#include "Segment.h"
#include "Capsule.h"
#include "Sphere.h"


/// -------------------------------------------------------------
///                     当たり判定クラス
/// -------------------------------------------------------------
class Collider
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// コンストラクタ
	Collider() : serialNumber_(IDGenerator::Generate()) {} // シリアルナンバーを生成

	// 仮想デストラクタ
	virtual ~Collider() = default;

	// 衝突時に呼ばれる仮想関数
	virtual void OnCollision([[maybe_unused]] Collider* other) {}

public: /// ---------- OBBのメンバ関数 ---------- ///

	// 中心座標取得・設定
	virtual Vector3 GetCenterPosition() const { return colliderPosition_; }
	virtual void SetCenterPosition(const Vector3& pos) { colliderPosition_ = pos; }

	// 半サイズ取得・設定
	virtual Vector3 GetOBBHalfSize() const { return colliderHalfSize_; }
	virtual void SetOBBHalfSize(const Vector3& halfSize) { colliderHalfSize_ = halfSize; }

	// 回転（オイラー角）取得・設定
	virtual Vector3 GetOrientation() const { return orientation_; }
	virtual void SetOrientation(const Vector3& rot) { orientation_ = rot; }

	OBB GetOBB() const;

public: /// ---------- セグメントのメンバ関数 ---------- ///

	// セグメントを設定（衝突判定用）
	void SetSegment(const Segment& segment) { segment_ = segment; }

	// セグメントを取得
	virtual Segment GetSegment() const { return segment_; }

public: /// ---------- Sphere のメンバ関数 ---------- ///

	// Sphereを設定（衝突判定用）
	void SetSphere(Sphere& spere) { sphere_ = spere; useSphere_ = true; }

	// Sphereを取得
	virtual Sphere GetSphere() const { return sphere_; }

public: /// ---------- Capsule のメンバ関数 ---------- ///

	// Capsule を設定
	virtual void SetCapsule(const Capsule& capsule) { capsule_ = capsule; useCapsule_ = true; }

	// Capsule を取得
	virtual Capsule GetCapsule() const { return capsule_; }

	// 使用フラグ（テーブル判定用）
	bool HasCapsule() const { return useCapsule_; }

	// デバッグ可視化フラグの設定
	void SetCapsuleVisible(bool v) { drawCapsule_ = v; }

	// デバッグ可視化フラグの取得
	bool IsCapsuleVisible() const { return drawCapsule_; }

public: /// ---------- デバッグ用メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理（OBBの可視化）
	void Draw();

	// ImGui描画処理
	void DrawImGui();

public: /// ---------- 設定 ---------- ///

	// 識別IDを取得
	uint32_t GetTypeID() const { return typeID_; }

	// 識別IDを設定
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }
	// シリアルナンバーを取得
	uint32_t GetUniqueID() const { return serialNumber_; }

	// オーナーを設定・取得
	template<class T> void SetOwner(T* ptr) { owner_ = ptr; }
	template<class T> T* GetOwner() const { return static_cast<T*>(owner_); }

private: /// ---------- メンバ変数 ---------- ///

	// 識別ID
	uint32_t typeID_ = 0u;

	// オーナー（任意のオブジェクトを指せるようにvoidポインタで持つ）
	void* owner_ = nullptr;

private: /// ---------- OBBのメンバ変数 ---------- ///

	// OBBの半サイズ
	Vector3 colliderPosition_ = { 0.0f, 0.0f, 0.0f }; // 中心座標
	Vector3 colliderHalfSize_ = { 0.0f, 0.0f, 0.0f }; // 半サイズ
	Vector3 orientation_ = { 0.0f, 0.0f, 0.0f };	  // オイラー角（ラジアン）
	Vector4 debugColor_ = { 0.0f, 1.0f, 1.0f, 1.0f }; // デバッグ表示色

	// OBBを使用するかどうか
	bool useOBB_ = true;

private: /// ---------- セグメントのメンバ変数 ---------- ///

	// セグメント（衝突判定用）
	Segment segment_{};

	// セグメントを使用するかどうか
	bool useSegment_ = true;

private: /// ---------- Sphere のメンバ変数 ---------- ///

	Sphere sphere_{}; // Sphere（衝突判定用）
	bool useSphere_ = false; // Sphere を使用するかどうか

private: /// ---------- Capsule のメンバ変数 ---------- ///

	Capsule capsule_{};
	bool useCapsule_ = false; // Capsule を使用するかどうか
	bool drawCapsule_ = false;    // デバッグ可視化するか（★追加）

protected: /// ---------- シリアルナンバー ---------- ///

	// シリアルナンバー
	uint32_t serialNumber_ = 0;
};
