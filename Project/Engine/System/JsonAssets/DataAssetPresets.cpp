#include "DataAssetPresets.h"

namespace Ken4lowEngine
{
	namespace
	{
		nlohmann::json ToJ(const Vector2& v) { return { v.x, v.y }; }
		nlohmann::json ToJ(const Vector3& v) { return { v.x, v.y, v.z }; }
		nlohmann::json ToJ(const Vector4& v) { return { v.x, v.y, v.z, v.w }; }
		void FromJ(const nlohmann::json& j, Vector2& v) { if (j.is_array() && j.size() >= 2) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); } }
		void FromJ(const nlohmann::json& j, Vector3& v) { if (j.is_array() && j.size() >= 3) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); } }
		void FromJ(const nlohmann::json& j, Vector4& v) { if (j.is_array() && j.size() >= 4) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); v.w = j[3].get<float>(); } }
	}
#define READ_NUM(name) if(inJson.contains(#name)){ name = inJson[#name].get<decltype(name)>(); }
	void LightPreset::ToJson(nlohmann::json& outJson) const { outJson = {{"directionalDirection",ToJ(directionalDirection)},{"color",ToJ(color)},{"intensity",intensity},{"enableShadow",enableShadow},{"shadowBias",shadowBias},{"normalBias",normalBias},{"shadowStrength",shadowStrength},{"shadowMapSize",shadowMapSize},{"shadowWidth",shadowWidth},{"shadowHeight",shadowHeight},{"shadowNearZ",shadowNearZ},{"shadowFarZ",shadowFarZ},{"shadowFocusMode",shadowFocusMode}}; }
	void LightPreset::FromJson(const nlohmann::json& inJson) { FromJ(inJson.value("directionalDirection", nlohmann::json::array()), directionalDirection); FromJ(inJson.value("color", nlohmann::json::array()), color); READ_NUM(intensity); READ_NUM(enableShadow); READ_NUM(shadowBias); READ_NUM(normalBias); READ_NUM(shadowStrength); READ_NUM(shadowMapSize); READ_NUM(shadowWidth); READ_NUM(shadowHeight); READ_NUM(shadowNearZ); READ_NUM(shadowFarZ); READ_NUM(shadowFocusMode); }
	void PostEffectPreset::ToJson(nlohmann::json& outJson) const { outJson = {{"enabled",enabled},{"activeEffect",activeEffect},{"bloomIntensity",bloomIntensity},{"vignetteIntensity",vignetteIntensity},{"grayscale",grayscale},{"sepia",sepia},{"fade",fade}}; }
	void PostEffectPreset::FromJson(const nlohmann::json& inJson) { READ_NUM(enabled); if(inJson.contains("activeEffect")) activeEffect=inJson["activeEffect"].get<std::string>(); READ_NUM(bloomIntensity); READ_NUM(vignetteIntensity); READ_NUM(grayscale); READ_NUM(sepia); READ_NUM(fade); }
	void Object3DPreset::ToJson(nlohmann::json& outJson) const { outJson={{"modelPath",modelPath},{"texturePath",texturePath},{"position",ToJ(position)},{"rotation",ToJ(rotation)},{"scale",ToJ(scale)},{"visible",visible},{"castShadow",castShadow},{"receiveShadow",receiveShadow}}; }
	void Object3DPreset::FromJson(const nlohmann::json& inJson) { if(inJson.contains("modelPath"))modelPath=inJson["modelPath"].get<std::string>(); if(inJson.contains("texturePath"))texturePath=inJson["texturePath"].get<std::string>(); FromJ(inJson.value("position",nlohmann::json::array()),position); FromJ(inJson.value("rotation",nlohmann::json::array()),rotation); FromJ(inJson.value("scale",nlohmann::json::array()),scale); READ_NUM(visible); READ_NUM(castShadow); READ_NUM(receiveShadow); }
	void SpritePreset::ToJson(nlohmann::json& outJson) const { outJson={{"texturePath",texturePath},{"position",ToJ(position)},{"size",ToJ(size)},{"color",ToJ(color)},{"anchor",ToJ(anchor)},{"visible",visible}}; }
	void SpritePreset::FromJson(const nlohmann::json& inJson) { if(inJson.contains("texturePath"))texturePath=inJson["texturePath"].get<std::string>(); FromJ(inJson.value("position",nlohmann::json::array()),position); FromJ(inJson.value("size",nlohmann::json::array()),size); FromJ(inJson.value("color",nlohmann::json::array()),color); FromJ(inJson.value("anchor",nlohmann::json::array()),anchor); READ_NUM(visible); }
	void ParticlePreset::ToJson(nlohmann::json& outJson) const { outJson={{"emitterType",emitterType},{"position",ToJ(position)},{"spawnRate",spawnRate},{"lifetime",lifetime},{"speed",speed},{"color",ToJ(color)},{"size",size},{"loop",loop}}; }
	void ParticlePreset::FromJson(const nlohmann::json& inJson) { if(inJson.contains("emitterType"))emitterType=inJson["emitterType"].get<std::string>(); FromJ(inJson.value("position",nlohmann::json::array()),position); READ_NUM(spawnRate); READ_NUM(lifetime); READ_NUM(speed); FromJ(inJson.value("color",nlohmann::json::array()),color); READ_NUM(size); READ_NUM(loop); }
}
