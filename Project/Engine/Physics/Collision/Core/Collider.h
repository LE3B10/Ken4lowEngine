#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "IDGenerator.h"

#include "OBB.h"
#include "AABB.h"
#include "Segment.h"
#include "Capsule.h"
#include "Sphere.h"

namespace Ken4lowEngine
{
	/// Primitive Colliderが扱う単純形状の種別。見た目のMeshとは分離した衝突用形状として扱う。
	enum class ECollisionShapeType : uint8_t
	{
		None,
		Sphere,
		AABB,
		OBB,
		Capsule,
		Segment,
	};

	/// CollisionShapeInfo はCollider内の形状データを将来CollisionShapeへ分離するための中間置き場。
	struct CollisionShapeInfo
	{
		static constexpr float kDrawEpsilon = 0.001f;

		ECollisionShapeType shapeType = ECollisionShapeType::OBB;
		Vector3 colliderPosition = { 0.0f, 0.0f, 0.0f };
		Vector3 colliderHalfSize = { 0.0f, 0.0f, 0.0f };
		Vector3 orientation = { 0.0f, 0.0f, 0.0f };
		Vector4 debugColor = { 0.0f, 1.0f, 1.0f, 1.0f };
		bool useOBB = true;
		Vector3 obbBasis[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
		bool useOBBBasis = false;
		Segment segment{};
		bool useSegment = true;
		Sphere sphere{};
		bool useSphere = false;
		Capsule capsule{};
		bool useCapsule = false;
		bool drawCapsule = false;

		// AABBはcenter/halfSizeから派生生成し、既存OBB情報と重複保持しない。
		AABB BuildAABB() const
		{
			return AABB{ colliderPosition - colliderHalfSize, colliderPosition + colliderHalfSize };
		}

		// OBB生成をShapeInfo側へ寄せ、ShapeType dispatchからも同じPrimitiveを取り出せるようにする。
		OBB BuildOBB() const
		{
			OBB obb{};
			obb.center = colliderPosition;
			obb.size = colliderHalfSize;

			if (useOBBBasis)
			{
				obb.orientations[0] = obbBasis[0];
				obb.orientations[1] = obbBasis[1];
				obb.orientations[2] = obbBasis[2];
				return obb;
			}

			const Matrix4x4 rotMat = Matrix4x4::MakeRotateMatrix(orientation);
			obb.orientations[0] = Vector3::Normalize({ rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] });
			obb.orientations[1] = Vector3::Normalize({ rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] });
			obb.orientations[2] = Vector3::Normalize({ rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] });
			return obb;
		}

		// 形状ごとの描画可否判定をShapeInfo側へ寄せ、Collider本体から形状条件を少しずつ分離する。
		bool HasDrawableOBB() const
		{
			return colliderHalfSize.x > kDrawEpsilon || colliderHalfSize.y > kDrawEpsilon || colliderHalfSize.z > kDrawEpsilon;
		}

		bool HasDrawableSegment() const
		{
			return Vector3::Length(segment.diff) > kDrawEpsilon;
		}

		bool HasDrawableCapsule() const
		{
			return useCapsule && drawCapsule && capsule.radius > kDrawEpsilon;
		}
	};

	/// CollisionFilterData はTypeIDやPreset適用情報を将来CollisionComponentへ移すための中間置き場。
	struct CollisionFilterData
	{
		uint32_t typeID = 0u;
		uint32_t objectChannelId = 0u;
		bool queryEnabled = true;
		bool physicsEnabled = true;
		std::string presetName{};

		// 既存TypeIDとObjectChannel相当IDを同期し、段階移行中の判定互換性を保つ。
		void SetTypeId(uint32_t newTypeId)
		{
			typeID = newTypeId;
			objectChannelId = newTypeId;
		}
	};

	/// CollisionEventState はEnter/Stay/Exit用の接触履歴を将来イベント管理へ分離するための中間置き場。
	struct CollisionEventState
	{
		std::unordered_set<uint32_t> currentCollisions;
		std::unordered_set<uint32_t> prevCollisions;

		// Enter/Stay/Exitの履歴更新をEventState側へ寄せ、イベント管理分離の入口にする。
		void BeginFrame()
		{
			prevCollisions.swap(currentCollisions);
			currentCollisions.clear();
		}

		void AddContact(uint32_t otherUniqueId)
		{
			currentCollisions.insert(otherUniqueId);
		}
	};

	/// -------------------------------------------------------------
	///                     当たり判定クラス
	/// -------------------------------------------------------------
	class Collider
	{
	public: /// ---------- 仮想関数 ---------- ///

		// コンストラクタ
		Collider() : serialNumber_(IDGenerator::Generate()) {} // シリアルナンバーを生成

		// 仮想デストラクタ
		virtual ~Collider() = default;

		// 旧来の衝突通知（互換用）
		virtual void OnCollision([[maybe_unused]] Collider* other) {}

		/*
		Collision Event naming policy:
		- OnCollisionEnter/Stay/Exit は当面Block/Overlap兼用の互換イベントとして残す。
		- 将来のBlock専用イベントは OnCollisionEnter/Stay/Exit を物理的に遮る接触として扱う。
		- 将来のOverlap専用イベントは OnOverlapEnter/Stay/Exit をすり抜け接触通知として追加する。
		*/
		// Enter/Stay/Exit（デフォルトは互換のため OnCollision を呼ぶ）
		virtual void OnCollisionEnter(Collider* other) { OnCollision(other); }
		virtual void OnCollisionStay(Collider* other) { OnCollision(other); }
		virtual void OnCollisionExit([[maybe_unused]] Collider* other) {}

	public: /// ---------- 衝突状態管理（マネージャ側が使用） ---------- ///

		// フレーム開始時に呼ぶ：prev <- current, current をクリア
		void BeginCollisionFrame();

		// このフレームで接触した相手IDを登録
		void AddCollisionThisFrame(uint32_t otherUniqueId);

		const std::unordered_set<uint32_t>& GetCurrentCollisions() const { return eventState_.currentCollisions; }
		const std::unordered_set<uint32_t>& GetPrevCollisions() const { return eventState_.prevCollisions; }

	public: /// ---------- OBBのメンバ関数 ---------- ///

		// 中心座標取得・設定
		virtual Vector3 GetCenterPosition() const { return shapeInfo_.colliderPosition; }
		virtual void SetCenterPosition(const Vector3& pos) { shapeInfo_.colliderPosition = pos; }

		// 半サイズ取得・設定
		virtual Vector3 GetOBBHalfSize() const { return shapeInfo_.colliderHalfSize; }
		virtual void SetOBBHalfSize(const Vector3& halfSize) { shapeInfo_.colliderHalfSize = halfSize; shapeInfo_.shapeType = ECollisionShapeType::OBB; }

		// 回転（オイラー角）取得・設定
		virtual Vector3 GetOrientation() const { return shapeInfo_.orientation; }
		virtual void SetOrientation(const Vector3& rot) { shapeInfo_.orientation = rot; shapeInfo_.shapeType = ECollisionShapeType::OBB; }

		OBB GetOBB() const;
		AABB GetAABB() const { return shapeInfo_.BuildAABB(); }

		// AABBをcenter/halfSizeへ展開して保持し、既存OBB用データと共存させる。
		void SetAABB(const AABB& aabb)
		{
			shapeInfo_.colliderPosition = (aabb.min + aabb.max) * 0.5f;
			shapeInfo_.colliderHalfSize = (aabb.max - aabb.min) * 0.5f;
			shapeInfo_.shapeType = ECollisionShapeType::AABB;
		}

		void SetOBBBasis(const Vector3& axisX, const Vector3& axisY, const Vector3& axisZ);
		void ClearOBBBasis();
		bool HasOBBBasis() const { return shapeInfo_.useOBBBasis; }

	public: /// ---------- セグメントのメンバ関数 ---------- ///

		// セグメントを設定（衝突判定用）
		void SetSegment(const Segment& segment) { shapeInfo_.segment = segment; shapeInfo_.shapeType = ECollisionShapeType::Segment; }

		// セグメントを取得
		virtual Segment GetSegment() const { return shapeInfo_.segment; }

	public: /// ---------- Sphere のメンバ関数 ---------- ///

		// Sphereを設定（衝突判定用）
		void SetSphere(Sphere& spere) { shapeInfo_.sphere = spere; shapeInfo_.useSphere = true; shapeInfo_.shapeType = ECollisionShapeType::Sphere; }

		// Sphereを取得
		virtual Sphere GetSphere() const { return shapeInfo_.sphere; }

	public: /// ---------- Capsule のメンバ関数 ---------- ///

		// Capsule を設定
		virtual void SetCapsule(const Capsule& capsule) { shapeInfo_.capsule = capsule; shapeInfo_.useCapsule = true; shapeInfo_.shapeType = ECollisionShapeType::Capsule; }

		// Capsule を取得
		virtual Capsule GetCapsule() const { return shapeInfo_.capsule; }

		// 使用フラグ（テーブル判定用）
		bool HasCapsule() const { return shapeInfo_.useCapsule; }

		// デバッグ可視化フラグの設定
		void SetCapsuleVisible(bool v) { shapeInfo_.drawCapsule = v; }

		// デバッグ可視化フラグの取得
		bool IsCapsuleVisible() const { return shapeInfo_.drawCapsule; }

		// 現在の主Primitive形状種別を取得（既存判定には未使用）
		ECollisionShapeType GetShapeType() const { return shapeInfo_.shapeType; }

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
		uint32_t GetTypeID() const { return filterData_.typeID; }

		// 識別IDを設定
		void SetTypeID(uint32_t typeID) { filterData_.SetTypeId(typeID); }

		// ObjectChannel相当のIDを取得（現段階では既存TypeIDと同値）
		uint32_t GetObjectChannelId() const { return filterData_.objectChannelId; }

		// Preset適用名を記録（既存判定には使わず移行確認用）
		void SetCollisionPresetName(std::string_view presetName) { filterData_.presetName = std::string(presetName); }
		std::string_view GetCollisionPresetName() const { return filterData_.presetName; }

		// Query有効状態をDebug表示から確認するための読み取り入口。
		bool IsQueryEnabled() const { return filterData_.queryEnabled; }

		// Physics有効状態をDebug表示から確認するための読み取り入口。
		bool IsPhysicsEnabled() const { return filterData_.physicsEnabled; }

		// シリアルナンバーを取得
		uint32_t GetUniqueID() const { return serialNumber_; }

		// オーナーを設定・取得
		template<class T> void SetOwner(T* ptr) { owner_ = ptr; }
		template<class T> T* GetOwner() const { return static_cast<T*>(owner_); }

	private: /// ---------- メンバ変数 ---------- ///

		// 衝突設定データ（TypeID/Preset情報）
		CollisionFilterData filterData_{};

		// オーナー（任意のオブジェクトを指せるようにvoidポインタで持つ）
		void* owner_ = nullptr;

	private: /// ---------- 衝突履歴（Enter/Stay/Exit 用） ---------- ///

		CollisionEventState eventState_{};

	private: /// ---------- OBBのメンバ変数 ---------- ///

		CollisionShapeInfo shapeInfo_{};

	protected: /// ---------- シリアルナンバー ---------- ///

		// シリアルナンバー
		uint32_t serialNumber_ = 0;
	};

} // namespace Ken4lowEngine
