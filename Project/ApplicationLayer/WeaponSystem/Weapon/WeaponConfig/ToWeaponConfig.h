#pragma once
#include "WeaponConfig.h"
#include "WeaponData.h"

// --- WeaponData -> WeaponConfig 変換（最小） ---
static WeaponConfig ToWeaponConfig(const WeaponData& E)
{
	WeaponConfig w{};
	w.name = E.name;
	w.clazz = E.clazz;
	w.muzzleSpeed = E.muzzleSpeed;
	w.maxDistance = E.maxDistance;
	w.rpm = E.rpm;
	w.magCapacity = static_cast<uint32_t>(E.magCapacity);
	w.startingReserve = static_cast<uint32_t>(E.startingReserve);
	w.reloadTime = E.reloadTime;
	w.bulletsPerShot = static_cast<uint32_t>(E.bulletsPerShot);

	w.autoReload = E.autoReload;
	w.requestedMaxSegments = E.requestedMaxSegments;

	w.spreadDeg = E.spreadDeg;
	w.pelletTracerMode = E.pelletTracerMode;
	w.pelletTracerCount = E.pelletTracerCount;

	// tracer
	w.tracer.enabled = E.tracer.enabled;
	w.tracer.tracerLength = E.tracer.tracerLength;
	w.tracer.tracerWidth = E.tracer.tracerWidth;
	w.tracer.minSegLength = E.tracer.minSegLength;
	w.tracer.startOffsetForward = E.tracer.startOffsetForward;
	w.tracer.tracerColor = E.tracer.color;

	// muzzle
	w.muzzle.enabled = E.muzzle.enabled;
	w.muzzle.life = E.muzzle.life;
	w.muzzle.startLength = E.muzzle.startLength;
	w.muzzle.endLength = E.muzzle.endLength;
	w.muzzle.startWidth = E.muzzle.startWidth;
	w.muzzle.endWidth = E.muzzle.endWidth;
	w.muzzle.randomYawDeg = E.muzzle.randomYawDeg;
	w.muzzle.color = E.muzzle.color;

	w.muzzle.offsetForward = E.muzzle.offsetForward;
	w.muzzle.sparksEnabled = E.muzzle.sparksEnabled;
	w.muzzle.sparkCount = E.muzzle.sparkCount;
	w.muzzle.sparkLifeMin = E.muzzle.sparkLifeMin;
	w.muzzle.sparkLifeMax = E.muzzle.sparkLifeMax;
	w.muzzle.sparkSpeedMin = E.muzzle.sparkSpeedMin;
	w.muzzle.sparkSpeedMax = E.muzzle.sparkSpeedMax;
	w.muzzle.sparkConeDeg = E.muzzle.sparkConeDeg;
	w.muzzle.sparkGravityY = E.muzzle.sparkGravityY;
	w.muzzle.sparkWidth = E.muzzle.sparkWidth;
	w.muzzle.sparkOffsetForward = E.muzzle.sparkOffsetForward;
	w.muzzle.sparkColorStart = E.muzzle.sparkColorStart;
	w.muzzle.sparkColorEnd = E.muzzle.sparkColorEnd;

	// casing
	w.casing.enabled = E.casing.enabled;
	w.casing.offsetRight = E.casing.offsetRight;
	w.casing.offsetUp = E.casing.offsetUp;
	w.casing.offsetBack = E.casing.offsetBack;
	w.casing.speedMin = E.casing.speedMin;
	w.casing.speedMax = E.casing.speedMax;
	w.casing.coneDeg = E.casing.coneDeg;
	w.casing.gravityY = E.casing.gravityY;
	w.casing.life = E.casing.life;
	w.casing.drag = E.casing.drag;
	w.casing.upKick = E.casing.upKick;
	w.casing.upBias = E.casing.upBias;
	w.casing.spinMin = E.casing.spinMin;
	w.casing.spinMax = E.casing.spinMax;
	w.casing.color = E.casing.color;
	w.casing.scale = E.casing.scale;
	return w;
}

// --- ピストル武器設定プリセット作成 ---
static std::string MakeUniqueName(const std::unordered_map<std::string, WeaponData>& table, std::string name)
{
	if (!table.count(name)) return name;
	int i = 1;
	while (table.count(name + "_" + std::to_string(i))) ++i;
	return name + "_" + std::to_string(i);
}