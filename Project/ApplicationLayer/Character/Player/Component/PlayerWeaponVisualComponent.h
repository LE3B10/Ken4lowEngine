#pragma once
#include "Object3D.h"
#include "PlayerWeaponComponent.h"
#include "WorldTransformEx.h"

#include <memory>

namespace K4E = Ken4lowEngine;

/// ------------------------------------------------------------------
///				プレイヤーの武器のビジュアルコンポーネント
/// ------------------------------------------------------------------
class PlayerWeaponVisualComponent
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化
	void Initialize();

	// 武器ロジックと右手のワールド変換をバインド
	void BindWeaponLogic(PlayerWeaponComponent* weaponLogic) { weaponLogic_ = weaponLogic; }

	// 右手のワールド変換をバインド
	void BindRightHandTransform(K4E::WorldTransformEx* handTransform) { rightHandTransform_ = handTransform; }

	// 更新
	void Update(float deltaTime, bool isADS);

	// 描画
	void Draw();

	// シャドウ描画
	void DrawShadow();

	// ImGui から見た目を調整
	void DrawImGui();

	// 現在の見た目を強制再構築したいときに使う
	void ForceRefresh();

	// 銃口のワールド座標を取得する
	bool TryGetMuzzleWorldPosition(K4E::Vector3& outPosition) const;

	// 銃口の向きを取得する。取得できない場合は false を返す
	bool TryGetMuzzleForward(K4E::Vector3& outForward) const;

private: /// ---------- メンバ関数 ---------- ///

	// 武器の切り替えを検知してビジュアルを再構築
	void RebuildIfWeaponChanged();

	// 武器のワールド変換を右手に同期させる
	void SyncToHand(bool isADS);

	// モデルをロードする共通処理
	bool LoadWeaponModel(const std::string& modelPath);

	// ローカル座標を現在の武器ワールド行列でワールド座標に変換する
	K4E::Vector3 TransformWeaponLocalPoint(const K4E::Vector3& localPoint) const;

private: /// ---------- メンバ変数 ---------- ///

	// 武器ロジックへのポインタ（装備中の武器の状態を参照するため）
	PlayerWeaponComponent* weaponLogic_ = nullptr;

	// 右手のワールド変換へのポインタ（右手の位置や回転を取得するため）
	K4E::WorldTransformEx* rightHandTransform_ = nullptr;

	std::unique_ptr<K4E::Object3D> weaponObject_;
	int32_t appliedWeaponId_ = 0;
	bool visible_ = true;

	// 武器のローカルオフセットと回転（右手に対する位置と向きの調整）
	K4E::Vector3 hipLocalOffset_{ 0.0f, -2.0f, 0.25f };
	K4E::Vector3 hipLocalRotate_{ 0.0f, 0.0f, -1.57f };

	// ADS（Aim Down Sights）時のローカルオフセットと回転（サイトを覗くときの位置と向きの調整）
	K4E::Vector3 adsLocalOffset_{ 0.0f, -2.0f, 0.25f };
	K4E::Vector3 adsLocalRotate_{ 0.0f, 0.0f, -1.57f };

	// 手に持つときのローカルオフセットと回転（右手に持ったときの位置と向きの調整）
	K4E::Vector3 handSocketLocalOffset_{ 0.05f, -0.08f, 0.12f };
	K4E::Vector3 handSocketLocalRotate_{ 0.0f, -1.57f, 0.0f };

	// 銃口のローカル位置。モデルによってずれる場合はここを調整する
	K4E::Vector3 muzzleLocalOffset_{ 0.0f, -0.02f, 0.85f };

	// 武器のスケール（全体の大きさの調整）
	K4E::Vector3 modelScale_{ 1.0f, 1.0f, 1.0f };

	// 一人称武器だけ少し小さくしたい時の倍率。最終的には modelScale_ に掛ける。
	float viewModelScaleMultiplier_ = 1.0f;

	K4E::Matrix4x4 weaponWorldMatrix_ = K4E::Matrix4x4::MakeIdentity();
	bool hasWeaponWorldMatrix_ = false;

	bool refreshRequested_ = false;
	std::string appliedModelPath_;
};

