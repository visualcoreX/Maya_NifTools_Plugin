#include "include/Custom Nodes/BSLightningShader.h"


BSLightningShader::BSLightningShader() {

}

BSLightningShader::~BSLightningShader() {

}

MStatus BSLightningShader::compute(const MPlug& plug, MDataBlock& dataBlock) {
	return MStatus::kSuccess;
}

void* BSLightningShader::creator() {
	return new BSLightningShader();
}

MStatus BSLightningShader::initialize() {
	MFnTypedAttribute shader_flags2_attribute;
	MFnTypedAttribute shader_flags1_attribute;
	MFnTypedAttribute shader_type_attribute;
	MFnNumericAttribute lighting_effect1_attribute;
	MFnNumericAttribute lighting_effect2_attribute;
	MFnNumericAttribute environment_map_scale_attribute;
	MFnNumericAttribute skin_tint_color_attribute;
	MFnNumericAttribute hair_tint_color_attribute;

	MFnTypedAttribute texture_slot2_attribute;
	MFnTypedAttribute texture_slot3_attribute;
	MFnTypedAttribute texture_slot4_attribute;
	MFnTypedAttribute texture_slot5_attribute;
	MFnTypedAttribute texture_slot6_attribute;
	MFnTypedAttribute texture_slot7_attribute;
	MFnTypedAttribute texture_slot8_attribute;
	
	MFnMessageAttribute target_shader_attribute;
	MFnMessageAttribute target_shape_attribute;

	MStatus status;

	target_shader_attribute.setStorable(true);
	target_shader_attribute.setConnectable(true);
	target_shader_attribute.setWritable(true);

	target_shape_attribute.setStorable(true);
	target_shape_attribute.setConnectable(true);
	target_shape_attribute.setWritable(true);

	shader_flags2_attribute.setStorable(true);
	shader_flags2_attribute.setWritable(true);
	shader_flags2_attribute.setReadable(true);

	shader_flags1_attribute.setStorable(true);
	shader_flags1_attribute.setWritable(true);
	shader_flags1_attribute.setReadable(true);

	shader_type_attribute.setStorable(true);
	shader_type_attribute.setWritable(true);
	shader_type_attribute.setReadable(true);

	lighting_effect1_attribute.setStorable(true);
	lighting_effect1_attribute.setWritable(true);
	lighting_effect1_attribute.setReadable(true);

	lighting_effect2_attribute.setStorable(true);
	lighting_effect2_attribute.setWritable(true);
	lighting_effect2_attribute.setReadable(true);

	environment_map_scale_attribute.setStorable(true);
	environment_map_scale_attribute.setReadable(true);
	environment_map_scale_attribute.setWritable(true);

	skin_tint_color_attribute.setStorable(true);
	skin_tint_color_attribute.setWritable(true);
	skin_tint_color_attribute.setReadable(true);

	texture_slot2_attribute.setStorable(true);
	texture_slot2_attribute.setWritable(true);
	texture_slot2_attribute.setReadable(true);

	texture_slot3_attribute.setStorable(true);
	texture_slot3_attribute.setWritable(true);
	texture_slot3_attribute.setReadable(true);

	texture_slot4_attribute.setStorable(true);
	texture_slot4_attribute.setWritable(true);
	texture_slot4_attribute.setReadable(true);

	texture_slot4_attribute.setStorable(true);
	texture_slot4_attribute.setWritable(true);
	texture_slot4_attribute.setReadable(true);

	texture_slot5_attribute.setStorable(true);
	texture_slot5_attribute.setWritable(true);
	texture_slot5_attribute.setReadable(true);

	texture_slot6_attribute.setStorable(true);
	texture_slot6_attribute.setWritable(true);
	texture_slot6_attribute.setReadable(true);

	texture_slot7_attribute.setStorable(true);
	texture_slot7_attribute.setWritable(true);
	texture_slot7_attribute.setReadable(true);

	texture_slot8_attribute.setStorable(true);
	texture_slot8_attribute.setWritable(true);
	texture_slot8_attribute.setReadable(true);

	targetShader = target_shader_attribute.create("targetShader", "tS", &status);
	targetShape = target_shape_attribute.create("targetShape", "tSP", &status);
	MFnStringData fnStringData;
	MObject defaultType = fnStringData.create("DEFAULT");
	shaderType = shader_type_attribute.create("shaderType", "sT", MFnData::kString, defaultType, &status);
	shaderFlags1 = shader_flags1_attribute.create("shaderFlags1", "sF1", MFnData::kStringArray, &status);
	shaderFlags2 = shader_flags2_attribute.create("shaderFlags2", "sF2", MFnData::kStringArray, &status);
	lightingEffect1 = lighting_effect1_attribute.create("lightingEffect1", "lE1", MFnNumericData::kFloat, 1, &status);
	lightingEffect2 = lighting_effect2_attribute.create("lightingEffect2", "lE2", MFnNumericData::kFloat, 1, &status);
	environmentMapScale = environment_map_scale_attribute.create("environmentMapScale", "eMS", MFnNumericData::kFloat, 1, &status);
	skinTintColor = skin_tint_color_attribute.createColor("skinTintColor", "sTC", &status);
	hairTintColor = hair_tint_color_attribute.createColor("hairTintColor", "hTC",  &status);
	textureSlot2 = texture_slot2_attribute.create("textureSlot2", "tS2", MFnData::kString, &status);
	textureSlot3 = texture_slot2_attribute.create("textureSlot3", "tS3", MFnData::kString, &status);
	textureSlot4 = texture_slot2_attribute.create("textureSlot4", "tS4", MFnData::kString, &status);
	textureSlot5 = texture_slot2_attribute.create("textureSlot5", "tS5", MFnData::kString, &status);
	textureSlot6 = texture_slot2_attribute.create("textureSlot6", "tS6", MFnData::kString, &status);
	textureSlot7 = texture_slot2_attribute.create("textureSlot7", "tS7", MFnData::kString, &status);
	textureSlot8 = texture_slot2_attribute.create("textureSlot8", "tS8", MFnData::kString, &status);

	status = MPxNode::addAttribute(targetShader);
	status = MPxNode::addAttribute(targetShape);
	status = MPxNode::addAttribute(shaderType);
	status = MPxNode::addAttribute(shaderFlags1);
	status = MPxNode::addAttribute(shaderFlags2);
	status = MPxNode::addAttribute(lightingEffect1);
	status = MPxNode::addAttribute(lightingEffect2);
	status = MPxNode::addAttribute(environmentMapScale);
	status = MPxNode::addAttribute(skinTintColor);
	status = MPxNode::addAttribute(hairTintColor);
	status = MPxNode::addAttribute(textureSlot2);
	status = MPxNode::addAttribute(textureSlot3);
	status = MPxNode::addAttribute(textureSlot4);
	status = MPxNode::addAttribute(textureSlot5);
	status = MPxNode::addAttribute(textureSlot6);
	status = MPxNode::addAttribute(textureSlot7);
	status = MPxNode::addAttribute(textureSlot8);

	return MStatus::kSuccess;
}


SkyrimShaderPropertyFlags1 BSLightningShader::stringArrayToShaderFlags1(const MStringArray& string_array) {
	unsigned int shader_flags = 0;

	for (int i = 0; i < string_array.length(); i++) {
		MString flag = string_array[i];
		if (flag == "SLSF1_SPECULAR") {
			shader_flags = (shader_flags | 1);
		}
		else if (flag == "SLSF1_SKINNED") {
			shader_flags = (shader_flags | 2);
		}
		else if (flag == "SLSF1_TEMP_REFRACTION") {
			shader_flags = (shader_flags | 4);
		}
		else if (flag == "SLSF1_VERTEX_ALPHA") {
			shader_flags = (shader_flags | 8);
		}
		else if (flag == "SLSF1_GREYSCALE_TO_PALETTECOLOR") {
			shader_flags = (shader_flags | 16);
		}
		else if (flag == "SLSF1_GREYSCALE_TO_PALETTEALPHA") {
			shader_flags = (shader_flags | 32);
		}
		else if (flag == "SLSF1_USE_FALLOFF") {
			shader_flags = (shader_flags | 64);
		}
		else if (flag == "SLSF1_ENVIRONMENT_MAPPING") {
			shader_flags = (shader_flags | 128);
		}
		else if (flag == "SLSF1_RECIEVE_SHADOWS") {
			shader_flags = (shader_flags | 256);
		}
		else if (flag == "SLSF1_CAST_SHADOWS") {
			shader_flags = (shader_flags | 512);
		}
		else if (flag == "SLSF1_FACEGEN_DETAIL_MAP") {
			shader_flags = (shader_flags | 1024);
		}
		else if (flag == "SLSF1_PARALLAX") {
			shader_flags = (shader_flags | 2048);
		}
		else if (flag == "SLSF1_MODEL_SPACE_NORMALS") {
			shader_flags = (shader_flags | 4096);
		}
		else if (flag == "SLSF1_NON_PROJECTIVE_SHADOWS") {
			shader_flags = (shader_flags | 8192);
		}
		else if (flag == "SLSF1_LANDSCAPE") {
			shader_flags = (shader_flags | 16384);
		}
		else if (flag == "SLSF1_REFRACTION") {
			shader_flags = (shader_flags | 32768);
		}
		else if (flag == "SLSF1_FIRE_REFRACTION") {
			shader_flags = (shader_flags | 65536);
		}
		else if (flag == "SLSF1_EYE_ENVIRONMENT_MAPPING") {
			shader_flags = (shader_flags | 131072);
		}
		else if (flag == "SLSF1_HAIR_SOFT_LIGHTING") {
			shader_flags = (shader_flags | 262144);
		}
		else if (flag == "SLSF1_SCREENDOOR_ALPHA_FADE") {
			shader_flags = (shader_flags | 524288);
		}
		else if (flag == "SLSF1_LOCALMAP_HIDE_SECRET") {
			shader_flags = (shader_flags | 1048576);
		}
		else if (flag == "SLSF1_FACEGEN_RGB_TINT") {
			shader_flags = (shader_flags | 2097152);
		}
		else if (flag == "SLSF1_OWN_EMIT") {
			shader_flags = (shader_flags | 4194304);
		}
		else if (flag == "SLSF1_PROJECTED_UV") {
			shader_flags = (shader_flags | 8388608);
		}
		else if (flag == "SLSF1_MULTIPLE_TEXTURES") {
			shader_flags = (shader_flags | 16777216);
		}
		else if (flag == "SLSF1_REMAPPABLE_TEXTURES") {
			shader_flags = (shader_flags | 33554432);
		}
		else if (flag == "SLSF1_DECAL") {
			shader_flags = (shader_flags | 67108864);
		}
		else if (flag == "SLSF1_DYNAMIC_DECAL") {
			shader_flags = (shader_flags | 134217728);
		}
		else if (flag == "SLSF1_PARALLAX_OCCLUSION") {
			shader_flags = (shader_flags | 268435456);
		}
		else if (flag == "SLSF1_EXTERNAL_EMITTANCE") {
			shader_flags = (shader_flags | 536870912);
		}
		else if (flag == "SLSF1_SOFT_EFFECT") {
			shader_flags = (shader_flags | 1073741824);
		}
		else if (flag == "SLSF1_ZBUFFER_TEST") {
			shader_flags = (shader_flags | 2147483648);
		}
	}

	return (SkyrimShaderPropertyFlags1) shader_flags;
}

MStringArray BSLightningShader::shaderFlags1ToStringArray(SkyrimShaderPropertyFlags1 shader_flags1) {
	MStringArray ret;
	bool has_flags = true;
	unsigned int shader_flags = (unsigned int) shader_flags1;

	while (has_flags == true) {
		has_flags = false;

		if ((1 & shader_flags) == 1) {
			ret.append("SLSF1_SPECULAR");
			shader_flags = (shader_flags & ~1);
			has_flags = true;
		}
		else if ((2 & shader_flags) == 2) {
			ret.append("SLSF1_SKINNED");
			shader_flags = (shader_flags & ~2);
			has_flags = true;
		}
		else if ((4 & shader_flags) == 4) {
			ret.append("SLSF1_TEMP_REFRACTION");
			shader_flags = (shader_flags & ~4);
			has_flags = true;
		}
		else if ((8 & shader_flags) == 8) {
			ret.append("SLSF1_VERTEX_ALPHA");
			shader_flags = (shader_flags & ~8);
			has_flags = true;
		}
		else if ((16 & shader_flags) == 16) {
			ret.append("SLSF1_GREYSCALE_TO_PALETTECOLOR");
			shader_flags = (shader_flags & ~16);
			has_flags = true;
		}
		else if ((32 & shader_flags) == 32) {
			ret.append("SLSF1_GREYSCALE_TO_PALETTEALPHA");
			shader_flags = (shader_flags & ~32);
			has_flags = true;
		}
		else if ((64 & shader_flags) == 64) {
			ret.append("SLSF1_USE_FALLOFF");
			shader_flags = (shader_flags & ~64);
			has_flags = true;
		}
		else if ((128 & shader_flags) == 128) {
			ret.append("SLSF1_ENVIRONMENT_MAPPING");
			shader_flags = (shader_flags & ~128);
			has_flags = true;
		}
		else if ((256 & shader_flags) == 256) {
			ret.append("SLSF1_RECIEVE_SHADOWS");
			shader_flags = (shader_flags & ~256);
			has_flags = true;
		}
		else if ((512 & shader_flags) == 512) {
			ret.append("SLSF1_CAST_SHADOWS");
			shader_flags = (shader_flags & ~512);
			has_flags = true;
		}
		else if ((1024 & shader_flags) == 1024) {
			ret.append("SLSF1_FACEGEN_DETAIL_MAP");
			shader_flags = (shader_flags & ~1024);
			has_flags = true;
		}
		else if ((2048 & shader_flags) == 2048) {
			ret.append("SLSF1_PARALLAX");
			shader_flags = (shader_flags & ~2048);
			has_flags = true;
		}
		else if ((4096 & shader_flags) == 4096) {
			ret.append("SLSF1_MODEL_SPACE_NORMALS");
			shader_flags = (shader_flags & ~4096);
			has_flags = true;
		}
		else if ((8192 & shader_flags) == 8192) {
			ret.append("SLSF1_NON_PROJECTIVE_SHADOWS");
			shader_flags = (shader_flags & ~8192);
			has_flags = true;
		}
		else if ((16384 & shader_flags) == 16384) {
			ret.append("SLSF1_LANDSCAPE");
			shader_flags = (shader_flags & ~16384);
			has_flags = true;
		}
		else if ((32768 & shader_flags) == 32768) {
			ret.append("SLSF1_REFRACTION");
			shader_flags = (shader_flags & ~32768);
			has_flags = true;
		}
		else if ((65536 & shader_flags) == 65536) {
			ret.append("SLSF1_FIRE_REFRACTION");
			shader_flags = (shader_flags & ~65536);
			has_flags = true;
		}
		else if ((131072 & shader_flags) == 131072) {
			ret.append("SLSF1_EYE_ENVIRONMENT_MAPPING");
			shader_flags = (shader_flags & ~131072);
			has_flags = true;
		}
		else if ((262144 & shader_flags) == 262144) {
			ret.append("SLSF1_HAIR_SOFT_LIGHTING");
			shader_flags = (shader_flags & ~262144);
			has_flags = true;
		}
		else if ((524288 & shader_flags) == 524288) {
			ret.append("SLSF1_SCREENDOOR_ALPHA_FADE");
			shader_flags = (shader_flags & ~524288);
			has_flags = true;
		}
		else if ((1048576 & shader_flags) == 1048576) {
			ret.append("SLSF1_LOCALMAP_HIDE_SECRET");
			shader_flags = (shader_flags & ~1048576);
			has_flags = true;
		}
		else if ((2097152 & shader_flags) == 2097152) {
			ret.append("SLSF1_FACEGEN_RGB_TINT");
			shader_flags = (shader_flags & ~2097152);
			has_flags = true;
		}
		else if ((4194304 & shader_flags) == 4194304) {
			ret.append("SLSF1_OWN_EMIT");
			shader_flags = (shader_flags & ~4194304);
			has_flags = true;
		}
		else if ((8388608 & shader_flags) == 8388608) {
			ret.append("SLSF1_PROJECTED_UV");
			shader_flags = (shader_flags & ~8388608);
			has_flags = true;
		}
		else if ((16777216 & shader_flags) == 16777216) {
			ret.append("SLSF1_MULTIPLE_TEXTURES");
			shader_flags = (shader_flags & ~16777216);
			has_flags = true;
		}
		else if ((33554432 & shader_flags) == 33554432) {
			ret.append("SLSF1_REMAPPABLE_TEXTURES");
			shader_flags = (shader_flags & ~33554432);
			has_flags = true;
		}
		else if ((67108864 & shader_flags) == 67108864) {
			ret.append("SLSF1_DECAL");
			shader_flags = (shader_flags & ~67108864);
			has_flags = true;
		}
		else if ((134217728 & shader_flags) == 134217728) {
			ret.append("SLSF1_DYNAMIC_DECAL");
			shader_flags = (shader_flags & ~134217728);
			has_flags = true;
		}
		else if ((268435456 & shader_flags) == 268435456) {
			ret.append("SLSF1_PARALLAX_OCCLUSION");
			shader_flags = (shader_flags & ~268435456);
			has_flags = true;
		}
		else if ((536870912 & shader_flags) == 536870912) {
			ret.append("SLSF1_EXTERNAL_EMITTANCE");
			shader_flags = (shader_flags & ~536870912);
			has_flags = true;
		}
		else if ((1073741824 & shader_flags) == 1073741824) {
			ret.append("SLSF1_SOFT_EFFECT");
			shader_flags = (shader_flags & ~1073741824);
		}
		else if ((2147483648 & shader_flags) == 2147483648) {
			ret.append("SLSF1_ZBUFFER_TEST");
			shader_flags = (shader_flags & ~2147483648);
			has_flags = true;
		}
	}

	return ret;
}

MStringArray BSLightningShader::shaderFlags2ToStringArray(SkyrimShaderPropertyFlags2 shader_flags2) {
	MStringArray ret;
	unsigned int shader_flags = (unsigned int) shader_flags2;

	bool has_flags = true;
	while (has_flags == true) {
		has_flags = false;

		if ((1 & shader_flags) == 1) {
			ret.append("SLSF2_ZBUFFER_WRITE");
			shader_flags = (shader_flags & ~1);
			has_flags = true;
		}
		else if ((2 & shader_flags) == 2) {
			ret.append("SLSF2_LOD_LANDSCAPE");
			shader_flags = (shader_flags & ~2);
			has_flags = true;
		}
		else if ((4 & shader_flags) == 4) {
			ret.append("SLSF2_LOD_Objects");
			shader_flags = (shader_flags & ~4);
			has_flags = true;
		}
		else if ((8 & shader_flags) == 8) {
			ret.append("SLSF2_NO_FADE");
			shader_flags = (shader_flags & ~8);
			has_flags = true;
		}
		else if ((16 & shader_flags) == 16) {
			ret.append("SLSF2_DOUBLE_SIDED");
			shader_flags = (shader_flags & ~16);
			has_flags = true;
		}
		else if ((32 & shader_flags) == 32) {
			ret.append("SLSF2_VERTEX_COLORS");
			shader_flags = (shader_flags & ~32);
			has_flags = true;
		}
		else if ((64 & shader_flags) == 64) {
			ret.append("SLSF2_GLOW_MAP");
			shader_flags = (shader_flags & ~64);
			has_flags = true;
		}
		else if ((128 & shader_flags) == 128) {
			ret.append("SLSF2_ASSUME_SHADOWMASK");
			shader_flags = (shader_flags & ~128);
			has_flags = true;
		}
		else if ((256 & shader_flags) == 256) {
			ret.append("SLSF2_PACKED_TANGENT");
			shader_flags = (shader_flags & ~256);
			has_flags = true;
		}
		else if ((512 & shader_flags) == 512) {
			ret.append("SLSF2_MULTI_INDEX_SNOW");
			shader_flags = (shader_flags & ~512);
			has_flags = true;
		}
		else if ((1024 & shader_flags) == 1024) {
			ret.append("SLSF2_VERTEX_LIGHTING");
			shader_flags = (shader_flags & ~1024);
			has_flags = true;
		}
		else if ((2048 & shader_flags) == 2048) {
			ret.append("SLSF2_UNIFORM_SCALE");
			shader_flags = (shader_flags & ~2048);
			has_flags = true;
		}
		else if ((4096 & shader_flags) == 4096) {
			ret.append("SLSF2_FIT_SLOPE");
			shader_flags = (shader_flags & ~4096);
			has_flags = true;
		}
		else if ((8192 & shader_flags) == 8192) {
			ret.append("SLSF2_BILLBOARD");
			shader_flags = (shader_flags & ~8192);
			has_flags = true;
		}
		else if ((16384 & shader_flags) == 16384) {
			ret.append("SLSF2_NO_LOD_LAND_BLEND");
			shader_flags = (shader_flags & ~16384);
			has_flags = true;
		}
		else if ((32768 & shader_flags) == 32768) {
			ret.append("SLSF2_ENVMAP_LIGHT_FADE");
			shader_flags = (shader_flags & ~32768);
			has_flags = true;
		}
		else if ((65536 & shader_flags) == 65536) {
			ret.append("SLSF2_WIREFRAME");
			shader_flags = (shader_flags & ~65536);
			has_flags = true;
		}
		else if ((131072 & shader_flags) == 131072) {
			ret.append("SLSF2_WEAPON_BLOOD");
			shader_flags = (shader_flags & ~131072);
			has_flags = true;
		}
		else if ((262144 & shader_flags) == 262144) {
			ret.append("SLSF2_HIDE_ON_LOCAL_MAP");
			shader_flags = (shader_flags & ~262144);
			has_flags = true;
		}
		else if ((524288 & shader_flags) == 524288) {
			ret.append("SLSF2_PREMULT_ALPHA");
			shader_flags = (shader_flags & ~524288);
			has_flags = true;
		}
		else if ((1048576 & shader_flags) == 1048576) {
			ret.append("SLSF2_CLOUD_LOD");
			shader_flags = (shader_flags & ~1048576);
			has_flags = true;
		}
		else if ((2097152 & shader_flags) == 2097152) {
			ret.append("SLSF2_ANISOTROPIC_LIGHTING");
			shader_flags = (shader_flags & ~2097152);
			has_flags = true;
		}
		else if ((4194304 & shader_flags) == 4194304) {
			ret.append("SLSF2_NO_TRANSPARENCY_MULTISAMPLING");
			shader_flags = (shader_flags & ~4194304);
			has_flags = true;
		}
		else if ((8388608 & shader_flags) == 8388608) {
			ret.append("SLSF2_UNUSED01");
			shader_flags = (shader_flags & ~8388608);
			has_flags = true;
		}
		else if ((16777216 & shader_flags) == 16777216) {
			ret.append("SLSF2_MULTI_LAYER_PARALLAX");
			shader_flags = (shader_flags & ~16777216);
			has_flags = true;
		}
		else if ((33554432 & shader_flags) == 33554432) {
			ret.append("SLSF2_SOFT_LIGHTING");
			shader_flags = (shader_flags & ~33554432);
			has_flags = true;
		}
		else if ((67108864 & shader_flags) == 67108864) {
			ret.append("SLSF2_RIM_LIGHTING");
			shader_flags = (shader_flags & ~67108864);
			has_flags = true;
		}
		else if ((134217728 & shader_flags) == 134217728) {
			ret.append("SLSF2_BACK_LIGHTING");
			shader_flags = (shader_flags & ~134217728);
			has_flags = true;
		}
		else if ((268435456 & shader_flags) == 268435456) {
			ret.append("SLSF2_UNUSED02");
			shader_flags = (shader_flags & ~268435456);
			has_flags = true;
		}
		else if ((536870912 & shader_flags) == 536870912) {
			ret.append("SLSF2_TREE_ANIM");
			shader_flags = (shader_flags & ~536870912);
			has_flags = true;
		}
		else if ((1073741824 & shader_flags) == 1073741824) {
			ret.append("SLSF2_EFFECT_LIGHTING");
			shader_flags = (shader_flags & ~1073741824);
		}
		else if ((2147483648 & shader_flags) == 2147483648) {
			ret.append("SLSF2_HD_LOD_OBJECTS");
			shader_flags = (shader_flags & ~2147483648);
			has_flags = true;
		}
	}

	return ret;
}

SkyrimShaderPropertyFlags2 BSLightningShader::stringArrayToShaderFlags2(const MStringArray& string_array) {
	unsigned int shader_flags = 0;

	for (int i = 0; i < string_array.length(); i++) {
		MString flag = string_array[i];
		if (flag == "SLSF2_ZBUFFER_WRITE") {
			shader_flags = (shader_flags | 1);
		}
		else if (flag == "SLSF2_LOD_LANDSCAPE") {
			shader_flags = (shader_flags | 2);
		}
		else if (flag == "SLSF2_LOD_OBJECTS") {
			shader_flags = (shader_flags | 4);
		}
		else if (flag == "SLSF2_NO_FADE") {
			shader_flags = (shader_flags | 8);
		}
		else if (flag == "SLSF2_DOUBLE_SIDED") {
			shader_flags = (shader_flags | 16);
		}
		else if (flag == "SLSF2_VERTEX_COLORS") {
			shader_flags = (shader_flags | 32);
		}
		else if (flag == "SLSF2_GLOW_MAP") {
			shader_flags = (shader_flags | 64);
		}
		else if (flag == "SLSF2_ASSUME_SHADOWMASK") {
			shader_flags = (shader_flags | 128);
		}
		else if (flag == "SLSF2_PACKED_TANGENT") {
			shader_flags = (shader_flags | 256);
		}
		else if (flag == "SLSF2_MULTI_INDEX_SNOW") {
			shader_flags = (shader_flags | 512);
		}
		else if (flag == "SLSF2_VERTEX_LIGHTING") {
			shader_flags = (shader_flags | 1024);
		}
		else if (flag == "SLSF2_UNIFORM_SCALE") {
			shader_flags = (shader_flags | 2048);
		}
		else if (flag == "SLSF2_FIT_SLOPE") {
			shader_flags = (shader_flags | 4096);
		}
		else if (flag == "SLSF2_BILLBOARD") {
			shader_flags = (shader_flags | 8192);
		}
		else if (flag == "SLSF2_NO_LOD_LAND_BLEND") {
			shader_flags = (shader_flags | 16384);
		}
		else if (flag == "SLSF2_ENVMAP_LIGHT_FADE") {
			shader_flags = (shader_flags | 32768);
		}
		else if (flag == "SLSF2_WIREFRAME") {
			shader_flags = (shader_flags | 65536);
		}
		else if (flag == "SLSF2_WEAPON_BLOOD") {
			shader_flags = (shader_flags | 131072);
		}
		else if (flag == "SLSF2_HIDE_ON_LOCAL_MAP") {
			shader_flags = (shader_flags | 262144);
		}
		else if (flag == "SLSF2_PREMULT_ALPHA") {
			shader_flags = (shader_flags | 524288);
		}
		else if (flag == "SLSF2_CLOUD_LOD") {
			shader_flags = (shader_flags | 1048576);
		}
		else if (flag == "SLSF2_ANISOTROPIC_LIGHTING") {
			shader_flags = (shader_flags | 2097152);
		}
		else if (flag == "SLSF2_NO_TRANSPARENCY_MULTISAMPLING") {
			shader_flags = (shader_flags | 4194304);
		}
		else if (flag == "SLSF2_UNUSED01") {
			shader_flags = (shader_flags | 8388608);
		}
		else if (flag == "SLSF2_MULTI_LAYER_PARALLAX") {
			shader_flags = (shader_flags | 16777216);
		}
		else if (flag == "SLSF2_SOFT_LIGHTING") {
			shader_flags = (shader_flags | 33554432);
		}
		else if (flag == "SLSF2_RIM_LIGHTING") {
			shader_flags = (shader_flags | 67108864);
		}
		else if (flag == "SLSF2_BACK_LIGHTING") {
			shader_flags = (shader_flags | 134217728);
		}
		else if (flag == "SLSF2_UNUSED02") {
			shader_flags = (shader_flags | 268435456);
		}
		else if (flag == "SLSF2_TREE_ANIM") {
			shader_flags = (shader_flags | 536870912);
		}
		else if (flag == "SLSF2_EFFECT_LIGHTING") {
			shader_flags = (shader_flags | 1073741824);
		}
		else if (flag == "SLSF2_HD_LOD_OBJECTS") {
			shader_flags = (shader_flags | 2147483648);
		}
	}

	return (SkyrimShaderPropertyFlags2) shader_flags;
}

BSLightingShaderPropertyShaderType BSLightningShader::stringToSkyrimShaderType(MString shader_type) {

	if (shader_type == "DEFAULT") {
		return BSLightingShaderPropertyShaderType::DEFAULT;
	}

	if (shader_type == "ENVIRONMENT_MAP") {
		return BSLightingShaderPropertyShaderType::ENVIRONMENT_MAP;
	}

	if (shader_type == "GLOW_SHADER") {
		return BSLightingShaderPropertyShaderType::GLOW_SHADER;
	}

	if (shader_type == "HEIGHTMAP") {
		return BSLightingShaderPropertyShaderType::HEIGHTMAP;
	}

	if (shader_type == "FACE_TINT") {
		return BSLightingShaderPropertyShaderType::FACE_TINT;
	}

	if (shader_type == "SKIN_TINT") {
		return BSLightingShaderPropertyShaderType::SKIN_TINT;
	}

	if (shader_type == "HAIR_TINT") {
		return BSLightingShaderPropertyShaderType::HAIR_TINT;
	}

	if (shader_type == "PARALLAX_OCC_MATERIAL") {
		return BSLightingShaderPropertyShaderType::PARALLAX_OCC_MATERIAL;
	}

	if (shader_type == "WORLD_MULTITEXTURE") {
		return BSLightingShaderPropertyShaderType::WORLD_MULTITEXTURE;
	}

	if (shader_type == "WORLDMAP1") {
		return BSLightingShaderPropertyShaderType::WORLDMAP1;
	}

	if (shader_type == "UNKNOWN_10") {
		return BSLightingShaderPropertyShaderType::UNKNOWN_10;
	}

	if (shader_type == "MULTILAYER_PARALLAX") {
		return BSLightingShaderPropertyShaderType::MULTILAYER_PARALLAX;
	}

	if (shader_type == "UNKNOWN_12") {
		return BSLightingShaderPropertyShaderType::UNKNOWN_12;
	}

	if (shader_type == "WORLDMAP2") {
		return BSLightingShaderPropertyShaderType::WORLDMAP2;
	}

	if (shader_type == "SPARKLE_SNOW") {
		return BSLightingShaderPropertyShaderType::SPARKLE_SNOW;
	}

	if (shader_type == "WORLDMAP3") {
		return BSLightingShaderPropertyShaderType::WORLDMAP3;
	}

	if (shader_type == "EYE_ENVMAP") {
		return BSLightingShaderPropertyShaderType::EYE_ENVMAP;
	}

	if (shader_type == "UNKNOWN_17") {
		return BSLightingShaderPropertyShaderType::UNKNOWN_17;
	}

	if (shader_type == "WORLDMAP4") {
		return BSLightingShaderPropertyShaderType::WORLDMAP4;
	}

	if (shader_type == "WORLD_LOD_MULTITEXTURE") {
		return BSLightingShaderPropertyShaderType::WORLD_LOD_MULTITEXTURE;
	}

	return BSLightingShaderPropertyShaderType::DEFAULT;
}

MString BSLightningShader::skyrimShaderTypeToString(BSLightingShaderPropertyShaderType shader_type) {

	if (shader_type == BSLightingShaderPropertyShaderType::DEFAULT) {
		return "DEFAULT";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::ENVIRONMENT_MAP) {
		return "ENVIRONMENT_MAP";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::EYE_ENVMAP) {
		return "EYE_ENVMAP";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::FACE_TINT) {
		return "FACE_TINT";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::GLOW_SHADER) {
		return "GLOW_SHADER";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::HAIR_TINT) {
		return "HAIR_TINT";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::HEIGHTMAP) {
		return "HEIGHTMAP";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::MULTILAYER_PARALLAX) {
		return "MULTILAYER_PARALLAX";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::PARALLAX_OCC_MATERIAL) {
		return "PARALLAX_OCC_MATERIAL";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::SKIN_TINT) {
		return "SKIN_TINT";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::SPARKLE_SNOW) {
		return "SPARKLE_SNOW";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::UNKNOWN_10) {
		return "UNKNOWN_10";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::UNKNOWN_12) {
		return "UNKNOWN_12";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::UNKNOWN_17) {
		return "UNKNOWN_17";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::WORLDMAP1) {
		return "WORLDMAP1";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::WORLDMAP2) {
		return "WORLDMAP2";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::WORLDMAP3) {
		return "WORLDMAP3";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::WORLDMAP4) {
		return "WORLDMAP4";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::WORLD_LOD_MULTITEXTURE) {
		return "WORLD_LOD_MULTITEXTURE";
	}

	if (shader_type == BSLightingShaderPropertyShaderType::WORLD_MULTITEXTURE) {
		return "WORLD_MULTITEXTURE";
	}

	return "DEFAULT";
}



MObject BSLightningShader::targetShader;

MObject BSLightningShader::targetShape;

MObject BSLightningShader::shaderType;

MObject BSLightningShader::shaderFlags1;

MObject BSLightningShader::shaderFlags2;

MObject BSLightningShader::lightingEffect1;

MObject BSLightningShader::lightingEffect2;

MObject BSLightningShader::environmentMapScale;

MObject BSLightningShader::skinTintColor;

MObject BSLightningShader::hairTintColor;

MObject BSLightningShader::textureSlot2;

MObject BSLightningShader::textureSlot3;

MObject BSLightningShader::textureSlot4;

MObject BSLightningShader::textureSlot5;

MObject BSLightningShader::textureSlot6;

MObject BSLightningShader::textureSlot7;

MObject BSLightningShader::textureSlot8;

MTypeId BSLightningShader::id(0x7ff15);
