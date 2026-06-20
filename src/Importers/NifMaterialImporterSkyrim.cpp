#include "include/Importers/NifMaterialImporterSkyrim.h"


NifMaterialImporterSkyrim::NifMaterialImporterSkyrim() {

}

NifMaterialImporterSkyrim::NifMaterialImporterSkyrim( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
}

void NifMaterialImporterSkyrim::ImportMaterialsAndTextures( NiAVObjectRef & root ) {
	this->GatherMaterialsAndTextures(root);
}

void NifMaterialImporterSkyrim::GatherMaterialsAndTextures(NiAVObjectRef& root) {
	if (root->GetType().IsDerivedType(NiGeometry::TYPE) ||
		root->GetType().IsDerivedType(BSTriShape::TYPE)) {
		NiAlphaPropertyRef alpha_property = NULL;
		BSLightingShaderPropertyRef shader_property = NULL;
		BSShaderPPLightingPropertyRef fnv_shader_property = NULL;
		Niflib::array<2, Ref<NiProperty>> properties;

		if (root->GetType().IsDerivedType(NiGeometry::TYPE)) {
			NiGeometryRef geometry = DynamicCast<NiGeometry>(root);
			properties = geometry->GetBSProperties();
		} else {
			BSTriShapeRef bs_shape = DynamicCast<BSTriShape>(root);
			properties = bs_shape->GetBSProperties();
		}

		for (int i = 0; i < 2; i++) {
			NiPropertyRef current_property = properties[i];

			if (current_property != NULL && current_property->GetType().IsSameType(BSLightingShaderProperty::TYPE)) {
				shader_property = DynamicCast<BSLightingShaderProperty>(current_property);
			}
			if (current_property != NULL && current_property->GetType().IsSameType(BSShaderPPLightingProperty::TYPE)) {
				fnv_shader_property = DynamicCast<BSShaderPPLightingProperty>(current_property);
			}
			if (current_property != NULL && current_property->GetType().IsSameType(NiAlphaProperty::TYPE)) {
				alpha_property = DynamicCast<NiAlphaProperty>(current_property);
			}
		}

		if (root->GetType().IsDerivedType(NiGeometry::TYPE)) {
			NiGeometryRef geometry = DynamicCast<NiGeometry>(root);
			vector<NiPropertyRef> ni_properties = geometry->GetProperties();
			for (int i = 0; i < ni_properties.size(); i++) {
				NiPropertyRef prop = ni_properties[i];
				if (prop != NULL && prop->GetType().IsSameType(BSShaderPPLightingProperty::TYPE)) {
					fnv_shader_property = DynamicCast<BSShaderPPLightingProperty>(prop);
				}
				if (prop != NULL && prop->GetType().IsSameType(NiAlphaProperty::TYPE)) {
					alpha_property = DynamicCast<NiAlphaProperty>(prop);
				}
			}
		}
		
		int valid_properties = 0;

		if(alpha_property != NULL) {
			valid_properties++;
		}
		if (shader_property != NULL) {
			valid_properties++;
		}
		if (fnv_shader_property != NULL) {
			valid_properties++;
		}

		if (valid_properties == 0) {
			return;
		}

		MObject found_material;

		for (int i = 0; i < this->property_groups.size(); i++) {
			vector<NiPropertyRef> property_group = this->property_groups[i];
			int similarities = 0;

			for (int j = 0; j < property_group.size(); j++) {
				if (alpha_property != NULL && property_group[j]->GetType().IsSameType(NiAlphaProperty::TYPE)) {
					NiAlphaPropertyRef current_alpha = DynamicCast<NiAlphaProperty>(property_group[j]);
					if (current_alpha == alpha_property) {
						similarities++;
					}
				}
				if (shader_property != NULL && property_group[j]->GetType().IsSameType(BSLightingShaderProperty::TYPE)) {
					BSLightingShaderPropertyRef current_shader = DynamicCast<BSLightingShaderProperty>(property_group[j]);
					if (current_shader == shader_property) {
						similarities++;
					}
				}
				if (fnv_shader_property != NULL && property_group[j]->GetType().IsSameType(BSShaderPPLightingProperty::TYPE)) {
					BSShaderPPLightingPropertyRef current_fnv = DynamicCast<BSShaderPPLightingProperty>(property_group[j]);
					if (current_fnv == fnv_shader_property) {
						similarities++;
					}
				}
			}

			if (valid_properties != 0 && similarities == valid_properties) {
				found_material = this->imported_materials[i];
				break;
			}
		}
		if(found_material.isNull()) {
			MFnPhongShader new_shader;
			found_material = new_shader.create();

			if(shader_property != NULL) {
				MColor reflective_color(shader_property->GetSpecularColor().r, shader_property->GetSpecularColor().g, shader_property->GetSpecularColor().b);
				MColor incadescence_color(shader_property->GetEmissiveColor().r, shader_property->GetEmissiveColor().g, shader_property->GetEmissiveColor().b);
				float glow_intensity = shader_property->GetEmissiveMultiple() / 1000;
				float reflective_strength = shader_property->GetSpecularStrength();
				float cosine_power = shader_property->GetGlossiness();
				float transparency = 1.0f - shader_property->GetAlpha();

				new_shader.setReflectedColor(reflective_color);
				new_shader.findPlug("reflectivity").setFloat(reflective_strength);
				new_shader.setIncandescence(incadescence_color);
				new_shader.setGlowIntensity(glow_intensity);
				new_shader.setCosPower(cosine_power);
				new_shader.setTransparency(transparency);

				BSShaderTextureSetRef texture_set = shader_property->GetTextureSet();
				if(texture_set != NULL) {
					MDGModifier dg_modifier;
					string color_texture = texture_set->GetTexture(0);
					string normal_map = texture_set->GetTexture(1);

					if(color_texture.length() > 0) {
						MString color_texture_file = this->GetTextureFilePath(color_texture);
						MFnDependencyNode color_texture_node;
						color_texture_node.create(MString("file"), MString(color_texture.c_str()));
						color_texture_node.findPlug("ftn").setValue(color_texture_file);
						MItDependencyNodes node_it(MFn::kTextureList);
						MFnDependencyNode texture_list(node_it.item());
						MPlug textures = texture_list.findPlug(MString("textures"));

						MPlug current_texture;
						int next = 0;
						while(true) {
							current_texture = textures.elementByLogicalIndex(next);

							if(current_texture.isConnected() == false) {
								break;
							}
							next++;
						}
						MPlug texture_message = color_texture_node.findPlug(MString("message"));
						dg_modifier.connect(texture_message, current_texture);

						MPlug color_texture_outcolor = color_texture_node.findPlug("outColor");
						MPlug new_shader_color = new_shader.findPlug("color");

						dg_modifier.connect(color_texture_outcolor, new_shader_color);

						if(alpha_property != NULL) {
							MPlug color_texture_outalpha = color_texture_node.findPlug("outTransparency");
							MPlug new_shader_transparency = new_shader.findPlug("transparency");
							dg_modifier.connect(color_texture_outalpha, new_shader_transparency);
						}

						MFnDependencyNode color_texture_placement;
						color_texture_placement.create("place2dTexture", "place2dTexture");

						dg_modifier.connect( color_texture_placement.findPlug("coverage"), color_texture_node.findPlug("coverage") );
						dg_modifier.connect( color_texture_placement.findPlug("mirrorU"), color_texture_node.findPlug("mirrorU") );
						dg_modifier.connect( color_texture_placement.findPlug("mirrorV"), color_texture_node.findPlug("mirrorV") );
						dg_modifier.connect( color_texture_placement.findPlug("noiseUV"), color_texture_node.findPlug("noiseUV") );
						dg_modifier.connect( color_texture_placement.findPlug("offset"), color_texture_node.findPlug("offset") );
						dg_modifier.connect( color_texture_placement.findPlug("outUV"), color_texture_node.findPlug("uvCoord") );
						dg_modifier.connect( color_texture_placement.findPlug("outUvFilterSize"), color_texture_node.findPlug("uvFilterSize") );
						dg_modifier.connect( color_texture_placement.findPlug("repeatUV"), color_texture_node.findPlug("repeatUV") );
						dg_modifier.connect( color_texture_placement.findPlug("rotateFrame"), color_texture_node.findPlug("rotateFrame") );
						dg_modifier.connect( color_texture_placement.findPlug("rotateUV"), color_texture_node.findPlug("rotateUV") );
						dg_modifier.connect( color_texture_placement.findPlug("stagger"), color_texture_node.findPlug("stagger") );
						dg_modifier.connect( color_texture_placement.findPlug("translateFrame"), color_texture_node.findPlug("translateFrame") );
						dg_modifier.connect( color_texture_placement.findPlug("vertexCameraOne"), color_texture_node.findPlug("vertexCameraOne") );
						dg_modifier.connect( color_texture_placement.findPlug("vertexUvOne"), color_texture_node.findPlug("vertexUvOne") );
						dg_modifier.connect( color_texture_placement.findPlug("vertexUvTwo"), color_texture_node.findPlug("vertexUvTwo") );
						dg_modifier.connect( color_texture_placement.findPlug("vertexUvThree"), color_texture_node.findPlug("vertexUvThree") );
						dg_modifier.connect( color_texture_placement.findPlug("wrapU"), color_texture_node.findPlug("wrapU") );
						dg_modifier.connect( color_texture_placement.findPlug("wrapV"), color_texture_node.findPlug("wrapV") );

					}

					if(normal_map.length() > 0) {
						MString normal_map_file = this->GetTextureFilePath(normal_map);
						MFnDependencyNode normal_map_node;
						normal_map_node.create(MString("file"), MString(normal_map.c_str()));
						normal_map_node.findPlug("ftn").setValue(normal_map_file);
						MItDependencyNodes node_it(MFn::kTextureList);
						MFnDependencyNode texture_list(node_it.item());
						MPlug textures = texture_list.findPlug(MString("textures"));

						MPlug current_texture;
						int next = 0;
						while(true) {
							current_texture = textures.elementByLogicalIndex(next);

							if(current_texture.isConnected() == false) {
								break;
							}
							next++;
						}

						MPlug texture_message = normal_map_node.findPlug("message");
						dg_modifier.connect(texture_message, current_texture);

						MFnDependencyNode bump2d_node;
						bump2d_node.create(MString("bump2d"),MString("bump2d"));
						MPlug bump_interp = bump2d_node.findPlug("bumpInterp");
						bump_interp.setInt(1);
						MString str = bump_interp.info();

						MPlug normal_map_outAlpha = normal_map_node.findPlug("outAlpha");
						MPlug bump2d_bumpvalue = bump2d_node.findPlug("bumpValue");
						dg_modifier.connect(normal_map_outAlpha, bump2d_bumpvalue);

						MPlug bump2d_outnormal = bump2d_node.findPlug("outNormal");
						MPlug new_shader_normalcamera = new_shader.findPlug("normalCamera");
						dg_modifier.connect(bump2d_outnormal, new_shader_normalcamera);

						MFnDependencyNode normal_map_placement;
						normal_map_placement.create("place2dTexture", "place2dTexture");

						dg_modifier.connect( normal_map_placement.findPlug("coverage"), normal_map_node.findPlug("coverage") );
						dg_modifier.connect( normal_map_placement.findPlug("mirrorU"), normal_map_node.findPlug("mirrorU") );
						dg_modifier.connect( normal_map_placement.findPlug("mirrorV"), normal_map_node.findPlug("mirrorV") );
						dg_modifier.connect( normal_map_placement.findPlug("noiseUV"), normal_map_node.findPlug("noiseUV") );
						dg_modifier.connect( normal_map_placement.findPlug("offset"), normal_map_node.findPlug("offset") );
						dg_modifier.connect( normal_map_placement.findPlug("outUV"), normal_map_node.findPlug("uvCoord") );
						dg_modifier.connect( normal_map_placement.findPlug("outUvFilterSize"), normal_map_node.findPlug("uvFilterSize") );
						dg_modifier.connect( normal_map_placement.findPlug("repeatUV"), normal_map_node.findPlug("repeatUV") );
						dg_modifier.connect( normal_map_placement.findPlug("rotateFrame"), normal_map_node.findPlug("rotateFrame") );
						dg_modifier.connect( normal_map_placement.findPlug("rotateUV"), normal_map_node.findPlug("rotateUV") );
						dg_modifier.connect( normal_map_placement.findPlug("stagger"), normal_map_node.findPlug("stagger") );
						dg_modifier.connect( normal_map_placement.findPlug("translateFrame"), normal_map_node.findPlug("translateFrame") );
						dg_modifier.connect( normal_map_placement.findPlug("vertexCameraOne"), normal_map_node.findPlug("vertexCameraOne") );
						dg_modifier.connect( normal_map_placement.findPlug("vertexUvOne"), normal_map_node.findPlug("vertexUvOne") );
						dg_modifier.connect( normal_map_placement.findPlug("vertexUvTwo"), normal_map_node.findPlug("vertexUvTwo") );
						dg_modifier.connect( normal_map_placement.findPlug("vertexUvThree"), normal_map_node.findPlug("vertexUvThree") );
						dg_modifier.connect( normal_map_placement.findPlug("wrapU"), normal_map_node.findPlug("wrapU") );
						dg_modifier.connect( normal_map_placement.findPlug("wrapV"), normal_map_node.findPlug("wrapV") );
					}

					dg_modifier.doIt();
				}

				MString shader_Type = this->skyrimShaderTypeToString(shader_property->GetSkyrimShaderType());
				MString shader_flags1 = this->skyrimShaderFlags1ToString(shader_property->GetShaderFlags1());
				MString shader_flags2 = this->skyrimShaderFlags2ToString(shader_property->GetShaderFlags2());

				MString mel_command = "addAttr -dt \"string\" -shortName skyrimShaderFlags1 ";
				MGlobal::executeCommand(mel_command + new_shader.name());
				mel_command = "addAttr -dt \"string\" -shortName skyrimShaderFlags2 ";
				MGlobal::executeCommand(mel_command + new_shader.name());
				mel_command = "addAttr -dt \"string\" -shortName skyrimShaderType ";
				MGlobal::executeCommand(mel_command + new_shader.name());

				mel_command = "setAttr -type \"string\" ";
				MGlobal::executeCommand(mel_command + new_shader.name() + "\.skyrimShaderFlags1 \"" + shader_flags1 + "\"");
				MGlobal::executeCommand(mel_command + new_shader.name() + "\.skyrimShaderFlags2 \"" + shader_flags2 + "\"");
				MGlobal::executeCommand(mel_command + new_shader.name() + "\.skyrimShaderType \"" + shader_Type + "\"");

				unsigned int shader_type2 = shader_property->GetSkyrimShaderType();
				if(texture_set != NULL) {
					if(shader_type2 == 1) {
						mel_command = "addAttr -dt \"string\" -shortName cubeMapTexture ";
						MGlobal::executeCommand(mel_command + new_shader.name());
						mel_command = "addAttr -dt \"string\" -shortName evironmentMaskTexture ";
						MGlobal::executeCommand(mel_command + new_shader.name());

						mel_command = "setAttr -type \"string\" ";
						MGlobal::executeCommand(mel_command + new_shader.name() + "\.cubeMapTexture \"" + this->GetTextureFilePath(texture_set->GetTexture(4)) + "\"");
						MGlobal::executeCommand(mel_command + new_shader.name() + "\.evironmentMaskTexture \"" + this->GetTextureFilePath(texture_set->GetTexture(5)) + "\"");
					} else if(shader_type2 == 2) {
						mel_command = "addAttr -dt \"string\" -shortName glowMapTexture ";
						MGlobal::executeCommand(mel_command + new_shader.name());
						
						mel_command = "setAttr -type \"string\" ";
						MGlobal::executeCommand(mel_command + new_shader.name() + "\.glowMapTexture \"" + this->GetTextureFilePath(texture_set->GetTexture(2)) + "\"");
					} else if(shader_type2 == 5) {
						mel_command = "addAttr -dt \"string\" -shortName skinTexture ";
						MGlobal::executeCommand(mel_command + new_shader.name());

						mel_command = "setAttr -type \"string\" ";
						MGlobal::executeCommand(mel_command + new_shader.name() + "\.skinTexture \"" + this->GetTextureFilePath(texture_set->GetTexture(2)) + "\"");

					}
				}

				MDGModifier dg_modifier;
				MFnDependencyNode bs_lightning_node;
				bs_lightning_node.create("bsLightningShader");

				MPlug input_message_shader = bs_lightning_node.findPlug("targetShader");
				MPlug new_shader_out = new_shader.findPlug("message");
				dg_modifier.connect(new_shader_out, input_message_shader);
				dg_modifier.doIt();

				MStringArray shader_flags1_strings = BSLightningShader::shaderFlags1ToStringArray(shader_property->GetShaderFlags1());
				MStringArray shader_flags2_strings = BSLightningShader::shaderFlags2ToStringArray(shader_property->GetShaderFlags2());

				mel_command = "setAttr ";
				mel_command += (bs_lightning_node.name() + ".shaderFlags1");
				mel_command += " -type \"stringArray\" ";
				mel_command += shader_flags1_strings.length();
				for (int z = 0; z < shader_flags1_strings.length(); z++) {
					mel_command += (" \"" + shader_flags1_strings[z] + "\"");
				}
				MGlobal::executeCommand(mel_command);

				mel_command = "setAttr ";
				mel_command += (bs_lightning_node.name() + ".shaderFlags2");
				mel_command += " -type \"stringArray\" ";
				mel_command += shader_flags2_strings.length();
				for (int z = 0; z < shader_flags2_strings.length(); z++) {
					mel_command += (" \"" + shader_flags2_strings[z] + "\"");
				}
				MGlobal::executeCommand(mel_command);

				MPlug texture_slot_plug;

				for (int y = 2; y <= 8; y++) {
					mel_command = "textureSlot";
					mel_command += y;

					texture_slot_plug = bs_lightning_node.findPlug(mel_command);

					texture_slot_plug.setString(texture_set->GetTexture(y).c_str());
				}

				MPlug shader_type_plug = bs_lightning_node.findPlug("shaderType");
				shader_type_plug.setString(BSLightningShader::skyrimShaderTypeToString(shader_property->GetSkyrimShaderType()));

				MPlug others_plug = bs_lightning_node.findPlug("lightingEffect1");
				others_plug.setFloat(shader_property->GetLightingEffect1());

				others_plug = bs_lightning_node.findPlug("lightingEffect2");
				others_plug.setFloat(shader_property->GetLightingEffect2());

				others_plug = bs_lightning_node.findPlug("environmentMapScale");
				others_plug.setFloat(shader_property->GetEnvironmentMapScale());

				others_plug = bs_lightning_node.findPlug("skinTintColor");
				others_plug.child(0).setValue(shader_property->GetSkinTintColor().r);
				others_plug.child(1).setValue(shader_property->GetSkinTintColor().g);
				others_plug.child(2).setValue(shader_property->GetSkinTintColor().b);

				others_plug = bs_lightning_node.findPlug("hairTintColor");
				others_plug.child(0).setValue(shader_property->GetHairTintColor().r);
				others_plug.child(1).setValue(shader_property->GetHairTintColor().g);
				others_plug.child(2).setValue(shader_property->GetHairTintColor().b);
			}
			else if (fnv_shader_property != NULL) {
				BSShaderTextureSetRef texture_set = fnv_shader_property->GetTextureSet();
				if (texture_set != NULL) {
					MDGModifier dg_modifier;
					string color_texture = texture_set->GetTexture(0);
					string normal_map = texture_set->GetTexture(1);

					if (color_texture.length() > 0) {
						MString color_texture_file = this->GetTextureFilePath(color_texture);
						MFnDependencyNode color_texture_node;
						color_texture_node.create(MString("file"), MString(color_texture.c_str()));
						color_texture_node.findPlug("ftn").setValue(color_texture_file);

						MItDependencyNodes node_it(MFn::kTextureList);
						MFnDependencyNode texture_list(node_it.item());
						MPlug textures = texture_list.findPlug(MString("textures"));
						MPlug current_texture;
						int next = 0;
						while (true) {
							current_texture = textures.elementByLogicalIndex(next);
							if (!current_texture.isConnected()) break;
							next++;
						}
						dg_modifier.connect(color_texture_node.findPlug("message"), current_texture);
						dg_modifier.connect(color_texture_node.findPlug("outColor"), new_shader.findPlug("color"));

						if (alpha_property != NULL) {
							dg_modifier.connect(color_texture_node.findPlug("outTransparency"), new_shader.findPlug("transparency"));
						}

						MFnDependencyNode placement;
						placement.create("place2dTexture", "place2dTexture");
						dg_modifier.connect(placement.findPlug("outUV"), color_texture_node.findPlug("uvCoord"));
						dg_modifier.connect(placement.findPlug("outUvFilterSize"), color_texture_node.findPlug("uvFilterSize"));
						dg_modifier.connect(placement.findPlug("repeatUV"), color_texture_node.findPlug("repeatUV"));
						dg_modifier.connect(placement.findPlug("wrapU"), color_texture_node.findPlug("wrapU"));
						dg_modifier.connect(placement.findPlug("wrapV"), color_texture_node.findPlug("wrapV"));
					}

					if (normal_map.length() > 0) {
						MString normal_map_file = this->GetTextureFilePath(normal_map);
						MFnDependencyNode normal_map_node;
						normal_map_node.create(MString("file"), MString(normal_map.c_str()));
						normal_map_node.findPlug("ftn").setValue(normal_map_file);

						MFnDependencyNode bump2d_node;
						bump2d_node.create(MString("bump2d"), MString("bump2d"));
						bump2d_node.findPlug("bumpInterp").setInt(1);

						dg_modifier.connect(normal_map_node.findPlug("outAlpha"), bump2d_node.findPlug("bumpValue"));
						dg_modifier.connect(bump2d_node.findPlug("outNormal"), new_shader.findPlug("normalCamera"));

						// Alpha = specular mask
						dg_modifier.connect(normal_map_node.findPlug("outAlpha"), new_shader.findPlug("specularColorR"));
						dg_modifier.connect(normal_map_node.findPlug("outAlpha"), new_shader.findPlug("specularColorG"));
						dg_modifier.connect(normal_map_node.findPlug("outAlpha"), new_shader.findPlug("specularColorB"));
					}

					// Use the full texture vector so we can safely check how many slots
					// this particular NIF actually stores before indexing into it.
					vector<string> all_textures = texture_set->GetTextures();

					// Slot 2 - Glow / emissive mask (Name_g.dds)
					if (all_textures.size() > 2 && all_textures[2].length() > 0) {
						string glow_map = all_textures[2];
						MString glow_map_file = this->GetTextureFilePath(glow_map);
						MFnDependencyNode glow_map_node;
						glow_map_node.create(MString("file"), MString(glow_map.c_str()));
						glow_map_node.findPlug("ftn").setValue(glow_map_file);
						glow_map_node.findPlug("alphaGain").setValue(0.25);
						glow_map_node.findPlug("defaultColorR").setValue(0.0);
						glow_map_node.findPlug("defaultColorG").setValue(0.0);
						glow_map_node.findPlug("defaultColorB").setValue(0.0);

						// Mask intensity drives the glow strength, color drives the glow tint
						dg_modifier.connect(glow_map_node.findPlug("outAlpha"), new_shader.findPlug("glowIntensity"));
						dg_modifier.connect(glow_map_node.findPlug("outColor"), new_shader.findPlug("incandescence"));
					}

					// Slot 3 - Parallax / height map (Name_p.dds)
					if (all_textures.size() > 3 && all_textures[3].length() > 0) {
						string parallax_map = all_textures[3];
						MString parallax_file = this->GetTextureFilePath(parallax_map);
						MFnDependencyNode parallax_node;
						parallax_node.create(MString("file"), MString(parallax_map.c_str()));
						parallax_node.findPlug("ftn").setValue(parallax_file);
						parallax_node.findPlug("alphaIsLuminance").setValue(true);

						MFnDependencyNode parallax_bump_node;
						parallax_bump_node.create(MString("bump2d"), MString("parallax_bump"));
						// 0 = standard bump (height-based), not tangent-space normals like the normal map uses
						parallax_bump_node.findPlug("bumpInterp").setInt(0);

						dg_modifier.connect(parallax_node.findPlug("outAlpha"), parallax_bump_node.findPlug("bumpValue"));
						// MFnPhongShader has no dedicated displacement input; bump2d's outNormal/outDisplacement
						// can be wired into a displacementShader node downstream if true displacement is needed.
					}

					// Slot 4 - Environment cube map (Name_e.dds)
					if (all_textures.size() > 4 && all_textures[4].length() > 0) {
						string env_map = all_textures[4];
						MString env_map_file = this->GetTextureFilePath(env_map);
						MFnDependencyNode env_map_node;
						env_map_node.create(MString("file"), MString(env_map.c_str()));
						env_map_node.findPlug("ftn").setValue(env_map_file);

						dg_modifier.connect(env_map_node.findPlug("outColor"), new_shader.findPlug("reflectedColor"));

						// Keep the reflectedColor connection in the graph for reference, but exclude it
						// from actual rendering so the env map doesn't show up as a visible reflection
						// in viewport/software renders - it's only meant to be re-exported back to the NIF.
						MString mel_command = "shadingConnection -e -cs off \"";
						mel_command += new_shader.name() + ".rc\"";
						MGlobal::executeCommand(mel_command);
					}

					// Slot 5 - Environment mask (Name_em.dds / Name_m.dds)
					if (all_textures.size() > 5 && all_textures[5].length() > 0) {
						string env_mask = all_textures[5];
						MString env_mask_file = this->GetTextureFilePath(env_mask);
						MFnDependencyNode env_mask_node;
						env_mask_node.create(MString("file"), MString(env_mask.c_str()));
						env_mask_node.findPlug("ftn").setValue(env_mask_file);

						// Mask alpha modulates reflection strength
						dg_modifier.connect(env_mask_node.findPlug("outAlpha"), new_shader.findPlug("reflectivity"));
					}

					dg_modifier.doIt();
				}
			}
		}

		vector<NiPropertyRef> property_group;

		if (shader_property != NULL) {
			property_group.push_back(DynamicCast<NiProperty>(shader_property));
		}
		if (fnv_shader_property != NULL) {
			property_group.push_back(DynamicCast<NiProperty>(fnv_shader_property));
			if (root->GetType().IsDerivedType(NiGeometry::TYPE)) {
				NiGeometryRef geometry = DynamicCast<NiGeometry>(root);
				vector<NiPropertyRef> ni_props = geometry->GetProperties();
				for (int i = 0; i < ni_props.size(); i++) {
					if (ni_props[i] != NULL && ni_props[i]->GetType().IsSameType(NiMaterialProperty::TYPE)) {
						property_group.push_back(ni_props[i]);
					}
				}
			}
		}
		if (alpha_property != NULL) {
			property_group.push_back(DynamicCast<NiProperty>(alpha_property));
		}

		// NiStencilProperty (and any other unhandled NiProperty types) must also be
		// included so this list matches ComplexShape::Merge's property group exactly,
		// otherwise shader-to-mesh matching in NifMeshImporterSkyrim fails to connect.
		if (root->GetType().IsDerivedType(NiGeometry::TYPE)) {
			NiGeometryRef geometry = DynamicCast<NiGeometry>(root);
			vector<NiPropertyRef> ni_props = geometry->GetProperties();
			for (int i = 0; i < ni_props.size(); i++) {
				if (ni_props[i] != NULL && ni_props[i]->GetType().IsSameType(NiStencilProperty::TYPE)) {
					property_group.push_back(ni_props[i]);
				}
			}
		}

		this->imported_materials.push_back(found_material);
		this->property_groups.push_back(property_group);
		this->translatorData->importedMaterials.push_back(pair<vector<NiPropertyRef>, MObject>(property_group, found_material));

	}

	if (root->GetType().IsDerivedType(NiNode::TYPE)) {
		NiNodeRef node = DynamicCast<NiNode>(root);
		vector<NiAVObjectRef> children = node->GetChildren();
		for (int i = 0; i < children.size(); i++) {
			this->GatherMaterialsAndTextures(children[i]);
		}
	}
}

MString NifMaterialImporterSkyrim::skyrimShaderFlags1ToString( unsigned int shader_flags ) {
	MString ret;

	bool has_flags = true;
	while(has_flags == true) {
		has_flags = false;

		if ((1 & shader_flags) == 1) {
			ret += "|SLSF1_Specular";
			shader_flags = (shader_flags & ~1);
			has_flags = true;
		} else if ((2 & shader_flags) == 2) {
			ret += "|SLSF1_Skinned";
			shader_flags = (shader_flags & ~2);
			has_flags = true;
		} else if ((4 & shader_flags) == 4) {
			ret += "|SLSF1_Temp_Refraction";
			shader_flags = (shader_flags & ~4);
			has_flags = true;
		} else if ((8 & shader_flags) == 8) {
			ret += "|SLSF1_Vertex_Alpha";
			shader_flags = (shader_flags & ~8);
			has_flags = true;
		} else if ((16 & shader_flags) == 16) {
			ret += "|SLSF1_Greyscale_To_PaletteColor";
			shader_flags = (shader_flags & ~16);
			has_flags = true;
		} else if ((32 & shader_flags) == 32) {
			ret += "|SLSF1_Greyscale_To_PaletteAlpha";
			shader_flags = (shader_flags & ~32);
			has_flags = true;
		} else if ((64 & shader_flags) == 64) {
			ret += "|SLSF1_Use_Falloff";
			shader_flags = (shader_flags & ~64);
			has_flags = true;
		} else if ((128 & shader_flags) == 128) {
			ret += "|SLSF1_Environment_Mapping";
			shader_flags = (shader_flags & ~128);
			has_flags = true;
		} else if ((256 & shader_flags) == 256) {
			ret += "|SLSF1_Recieve_Shadows";
			shader_flags = (shader_flags & ~256);
			has_flags = true;
		} else if ((512 & shader_flags) == 512) {
			ret += "|SLSF1_Cast_Shadows";
			shader_flags = (shader_flags & ~512);
			has_flags = true;
		} else if ((1024 & shader_flags) == 1024) {
			ret += "|SLSF1_Facegen_Detail_Map";
			shader_flags = (shader_flags & ~1024);
			has_flags = true;
		} else if ((2048 & shader_flags) == 2048) {
			ret += "|SLSF1_Parallax";
			shader_flags = (shader_flags & ~2048);
			has_flags = true;
		} else if ((4096 & shader_flags) == 4096) {
			ret += "|SLSF1_Model_Space_Normals";
			shader_flags = (shader_flags & ~4096);
			has_flags = true;
		} else if ((8192 & shader_flags) == 8192) {
			ret += "|SLSF1_Non-Projective_Shadows";
			shader_flags = (shader_flags & ~8192);
			has_flags = true;
		} else if ((16384 & shader_flags) == 16384) {
			ret += "|SLSF1_Landscape";
			shader_flags = (shader_flags & ~16384);
			has_flags = true;
		} else if ((32768 & shader_flags) == 32768) {
			ret += "|SLSF1_Refraction";
			shader_flags = (shader_flags & ~32768);
			has_flags = true;
		} else if ((65536 & shader_flags) == 65536) {
			ret += "|SLSF1_Fire_Refraction";
			shader_flags = (shader_flags & ~65536);
			has_flags = true;
		} else if ((131072 & shader_flags) == 131072) {
			ret += "|SLSF1_Eye_Environment_Mapping";
			shader_flags = (shader_flags & ~131072);
			has_flags = true;
		} else if ((262144 & shader_flags) == 262144) {
			ret += "|SLSF1_Hair_Soft_Lighting";
			shader_flags = (shader_flags & ~262144);
			has_flags = true;
		} else if ((524288 & shader_flags) == 524288) {
			ret += "|SLSF1_Screendoor_Alpha_Fade";
			shader_flags = (shader_flags & ~524288);
			has_flags = true;
		} else if ((1048576 & shader_flags) == 1048576) {
			ret += "|SLSF1_Localmap_Hide_Secret";
			shader_flags = (shader_flags & ~1048576);
			has_flags = true;
		} else if ((2097152 & shader_flags) == 2097152) {
			ret += "|SLSF1_FaceGen_RGB_Tint";
			shader_flags = (shader_flags & ~2097152);
			has_flags = true;
		} else if ((4194304 & shader_flags) == 4194304) {
			ret += "|SLSF1_Own_Emit";
			shader_flags = (shader_flags & ~4194304);
			has_flags = true;
		} else if ((8388608 & shader_flags) == 8388608) {
			ret += "|SLSF1_Projected_UV";
			shader_flags = (shader_flags & ~8388608);
			has_flags = true;
		} else if ((16777216 & shader_flags) == 16777216) {
			ret += "|SLSF1_Multiple_Textures";
			shader_flags = (shader_flags & ~16777216);
			has_flags = true;
		} else if ((33554432 & shader_flags) == 33554432) {
			ret += "|SLSF1_Remappable_Textures";
			shader_flags = (shader_flags & ~33554432);
			has_flags = true;
		} else if ((67108864 & shader_flags) == 67108864) {
			ret += "|SLSF1_Decal";
			shader_flags = (shader_flags & ~67108864);
			has_flags = true;
		} else if ((134217728 & shader_flags) == 134217728) {
			ret += "|SLSF1_Dynamic_Decal";
			shader_flags = (shader_flags & ~134217728);
			has_flags = true;
		} else if ((268435456 & shader_flags) == 268435456) {
			ret += "|SLSF1_Parallax_Occlusion";
			shader_flags = (shader_flags & ~268435456);
			has_flags = true;
		} else if ((536870912 & shader_flags) == 536870912) {
			ret += "|SLSF1_External_Emittance";
			shader_flags = (shader_flags & ~536870912);
			has_flags = true;
		} else if ((1073741824 & shader_flags) == 1073741824) {
			ret += "|SLSF1_Soft_Effect";
			shader_flags = (shader_flags & ~1073741824);
		} else if ((2147483648 & shader_flags) == 2147483648) {
			ret += "|SLSF1_ZBuffer_Test";
			shader_flags = (shader_flags & ~2147483648);
			has_flags = true;
		}
	}

	if(ret.length() > 1) {
			ret = ret.substring(1, ret.length());
	}

	return ret;
}

MString NifMaterialImporterSkyrim::skyrimShaderFlags2ToString( unsigned int shader_flags ) {
	MString ret;

	bool has_flags = true;
	while(has_flags == true) {
		has_flags = false;

		if ((1 & shader_flags) == 1) {
			ret += "|SLSF2_ZBuffer_Write";
			shader_flags = (shader_flags & ~1);
			has_flags = true;
		} else if ((2 & shader_flags) == 2) {
			ret += "|SLSF2_LOD_Landscape";
			shader_flags = (shader_flags & ~2);
			has_flags = true;
		} else if ((4 & shader_flags) == 4) {
			ret += "|SLSF2_LOD_Objects";
			shader_flags = (shader_flags & ~4);
			has_flags = true;
		} else if ((8 & shader_flags) == 8) {
			ret += "|SLSF2_No_Fade";
			shader_flags = (shader_flags & ~8);
			has_flags = true;
		} else if ((16 & shader_flags) == 16) {
			ret += "|SLSF2_Double_Sided";
			shader_flags = (shader_flags & ~16);
			has_flags = true;
		} else if ((32 & shader_flags) == 32) {
			ret += "|SLSF2_Vertex_Colors";
			shader_flags = (shader_flags & ~32);
			has_flags = true;
		} else if ((64 & shader_flags) == 64) {
			ret += "|SLSF2_Glow_Map";
			shader_flags = (shader_flags & ~64);
			has_flags = true;
		} else if ((128 & shader_flags) == 128) {
			ret += "|SLSF2_Assume_Shadowmask";
			shader_flags = (shader_flags & ~128);
			has_flags = true;
		} else if ((256 & shader_flags) == 256) {
			ret += "|SLSF2_Packed_Tangent";
			shader_flags = (shader_flags & ~256);
			has_flags = true;
		} else if ((512 & shader_flags) == 512) {
			ret += "|SLSF2_Multi_Index_Snow";
			shader_flags = (shader_flags & ~512);
			has_flags = true;
		} else if ((1024 & shader_flags) == 1024) {
			ret += "|SLSF2_Vertex_Lighting";
			shader_flags = (shader_flags & ~1024);
			has_flags = true;
		} else if ((2048 & shader_flags) == 2048) {
			ret += "|SLSF2_Uniform_Scale";
			shader_flags = (shader_flags & ~2048);
			has_flags = true;
		} else if ((4096 & shader_flags) == 4096) {
			ret += "|SLSF2_Fit_Slope";
			shader_flags = (shader_flags & ~4096);
			has_flags = true;
		} else if ((8192 & shader_flags) == 8192) {
			ret += "|SLSF2_Billboard";
			shader_flags = (shader_flags & ~8192);
			has_flags = true;
		} else if ((16384 & shader_flags) == 16384) {
			ret += "|SLSF2_No_LOD_Land_Blend";
			shader_flags = (shader_flags & ~16384);
			has_flags = true;
		} else if ((32768 & shader_flags) == 32768) {
			ret += "|SLSF2_EnvMap_Light_Fade";
			shader_flags = (shader_flags & ~32768);
			has_flags = true;
		} else if ((65536 & shader_flags) == 65536) {
			ret += "|SLSF2_Wireframe";
			shader_flags = (shader_flags & ~65536);
			has_flags = true;
		} else if ((131072 & shader_flags) == 131072) {
			ret += "|SLSF2_Weapon_Blood";
			shader_flags = (shader_flags & ~131072);
			has_flags = true;
		} else if ((262144 & shader_flags) == 262144) {
			ret += "|SLSF2_Hide_On_Local_Map";
			shader_flags = (shader_flags & ~262144);
			has_flags = true;
		} else if ((524288 & shader_flags) == 524288) {
			ret += "|SLSF2_Premult_Alpha";
			shader_flags = (shader_flags & ~524288);
			has_flags = true;
		} else if ((1048576 & shader_flags) == 1048576) {
			ret += "|SLSF2_Cloud_LOD";
			shader_flags = (shader_flags & ~1048576);
			has_flags = true;
		} else if ((2097152 & shader_flags) == 2097152) {
			ret += "|SLSF2_Anisotropic_Lighting";
			shader_flags = (shader_flags & ~2097152);
			has_flags = true;
		} else if ((4194304 & shader_flags) == 4194304) {
			ret += "|SLSF2_No_Transparency_Multisampling";
			shader_flags = (shader_flags & ~4194304);
			has_flags = true;
		} else if ((8388608 & shader_flags) == 8388608) {
			ret += "|SLSF2_Unused01";
			shader_flags = (shader_flags & ~8388608);
			has_flags = true;
		} else if ((16777216 & shader_flags) == 16777216) {
			ret += "|SLSF2_Multi_Layer_Parallax";
			shader_flags = (shader_flags & ~16777216);
			has_flags = true;
		} else if ((33554432 & shader_flags) == 33554432) {
			ret += "|SLSF2_Soft_Lighting";
			shader_flags = (shader_flags & ~33554432);
			has_flags = true;
		} else if ((67108864 & shader_flags) == 67108864) {
			ret += "|SLSF2_Rim_Lighting";
			shader_flags = (shader_flags & ~67108864);
			has_flags = true;
		} else if ((134217728 & shader_flags) == 134217728) {
			ret += "|SLSF2_Back_Lighting";
			shader_flags = (shader_flags & ~134217728);
			has_flags = true;
		} else if ((268435456 & shader_flags) == 268435456) {
			ret += "|SLSF2_Unused02";
			shader_flags = (shader_flags & ~268435456);
			has_flags = true;
		} else if ((536870912 & shader_flags) == 536870912) {
			ret += "|SLSF2_Tree_Anim";
			shader_flags = (shader_flags & ~536870912);
			has_flags = true;
		} else if ((1073741824 & shader_flags) == 1073741824) {
			ret += "|SLSF2_Effect_Lighting";
			shader_flags = (shader_flags & ~1073741824);
		} else if ((2147483648 & shader_flags) == 2147483648) {
			ret += "|SLSF2_HD_LOD_Objects";
			shader_flags = (shader_flags & ~2147483648);
			has_flags = true;
		}
	}

	if(ret.length() > 1) {
		ret = ret.substring(1, ret.length());
	}

	return ret;
}

MString NifMaterialImporterSkyrim::skyrimShaderTypeToString( unsigned int shader_type ) {
	MString ret;

	if(shader_type == 0) {
		ret = "Default";
	} else if(shader_type == 1) {
		ret = "EnvMap";
	} else if(shader_type == 5) {
		ret = "Skin";
	} else if(shader_type == 2) {
		ret = "Glow";
	} else if(shader_type == 6) {
		ret = "Hair";
	} else if(shader_type == 11) {
		ret = "Ice/Parallax";
	} else if(shader_type == 15) {
		ret = "Eye";
	}

	return ret;
}

string NifMaterialImporterSkyrim::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifMaterialImporter::asString(verbose)<<endl;
	out<<"NifMaterialExporterSkyrim"<<endl;

	return out.str();
}

const Type& NifMaterialImporterSkyrim::GetType() const {
	return TYPE;
}

const Type NifMaterialImporterSkyrim::TYPE("NifMaterialImporterSkyrim",&NifMaterialImporter::TYPE);


