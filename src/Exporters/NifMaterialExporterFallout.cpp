#include "include/Exporters/NifMaterialExporterFallout.h"

NifMaterialExporterFallout::NifMaterialExporterFallout(
    NifTranslatorOptionsRef translator_options,
    NifTranslatorDataRef translator_data,
    NifTranslatorUtilsRef translator_utils)
{
    this->translatorOptions = translator_options;
    this->translatorData = translator_data;
    this->translatorUtils = translator_utils;
}

void NifMaterialExporterFallout::ExportMaterials() {
    MFnDependencyNode shader_node;
    MItDependencyNodes it_nodes(MFn::kLambert);

    while (!it_nodes.isDone()) {
        if (it_nodes.thisNode().hasFn(MFn::kLambert)) {
            shader_node.setObject(it_nodes.thisNode());

            NiAlphaPropertyRef alpha_property = NULL;
            BSShaderTextureSetRef shader_textures = new BSShaderTextureSet();
            BSShaderPPLightingPropertyRef shader_property = new BSShaderPPLightingProperty();
            NiMaterialPropertyRef mat_property = new NiMaterialProperty();

            for (int i = 0; i < 6; i++) {
                shader_textures->SetTexture(i, "");
            }

            MColor color;
            MPlugArray connections;
            MObject current_texture;

            // Diffuse map (slot 0)
            this->GetColor(shader_node.object(), "color", color, current_texture);
            if (!current_texture.isNull()) {
                string file_name = this->ExportTexture(current_texture);
                shader_textures->SetTexture(0, file_name);
                MFnDependencyNode texture_node(current_texture);
                texture_node.findPlug("outAlpha").connectedTo(connections, false, true);
                if (connections.length() > 0 && connections[0].node() == shader_node.object()) {
                    alpha_property = new NiAlphaProperty();
                    alpha_property->SetFlags(237);
                }
            }

            // Normal map (slot 1)
            connections.clear();
            MPlug in_normal = shader_node.findPlug("normalCamera");
            in_normal.connectedTo(connections, true, false);
            if (connections.length() > 0) {
                MPlug bump_plug = connections[0];
                MFnDependencyNode bump_node(bump_plug.node());
                MPlug bump_value_plug = bump_node.findPlug("bumpValue");
                connections.clear();
                bump_value_plug.connectedTo(connections, true, false);
                if (connections.length() > 0) {
                    current_texture = connections[0].node();
                    string file_name = this->ExportTexture(current_texture);
                    shader_textures->SetTexture(1, file_name);
                }
            }

            // Glow map (slot 2) - read from incandescence/glowIntensity graph
            connections.clear();
            MPlug in_incandescence = shader_node.findPlug("incandescence");
            in_incandescence.connectedTo(connections, true, false);
            if (connections.length() > 0) {
                current_texture = connections[0].node();
                string file_name = this->ExportTexture(current_texture);
                shader_textures->SetTexture(2, file_name);
            }

            // Environment cube map (slot 4) - read from reflectedColor graph
            if (shader_node.object().hasFn(MFn::kPhong)) {
                connections.clear();
                MPlug in_reflected_color = shader_node.findPlug("reflectedColor");
                in_reflected_color.connectedTo(connections, true, false);
                if (connections.length() > 0) {
                    current_texture = connections[0].node();
                    string file_name = this->ExportTexture(current_texture);
                    shader_textures->SetTexture(4, file_name);
                }

                // Environment mask (slot 5) - read from reflectivity graph
                connections.clear();
                MPlug in_reflectivity = shader_node.findPlug("reflectivity");
                in_reflectivity.connectedTo(connections, true, false);
                if (connections.length() > 0) {
                    // reflectivity is connected to outAlpha of the env mask file node
                    current_texture = connections[0].node();
                    string file_name = this->ExportTexture(current_texture);
                    shader_textures->SetTexture(5, file_name);
                }
            }

            shader_property->SetTextureSet(shader_textures);
            // FNV-standard shader flags: Specular, Remappable Textures, ZBuffer Test
            shader_property->SetShaderFlags((BSShaderFlags)(SF_SPECULAR | SF_REMAPPABLE_TEXTURES | SF_ZBUFFER_TEST));

            // NiMaterialProperty
            if (shader_node.object().hasFn(MFn::kPhong)) {
                MFnPhongShader phong_node(shader_node.object());
                MColor spec = phong_node.specularColor();

                // FNV-standard material values (Maya's raw values made mesh invisible/wrong-shiny)
                mat_property->SetSpecularColor(Color3(0.0f, 0.0f, 0.0f));
                mat_property->SetGlossiness(80.0f);
                mat_property->SetTransparency(1.0f); // Alpha = 1.0, fully visible

                MColor emit = phong_node.incandescence();
                mat_property->SetEmissiveColor(Color3(emit.r, emit.g, emit.b));
            }
            else {
                MGlobal::displayWarning("Warning! Shader types besides phong aren't supported too well");
            }

            vector<NiPropertyRef> properties;
            properties.push_back(DynamicCast<NiProperty>(shader_property));
            properties.push_back(DynamicCast<NiProperty>(mat_property));
            if (alpha_property != NULL) {
                properties.push_back(DynamicCast<NiProperty>(alpha_property));
            }

            this->translatorData->shaders[shader_node.name().asChar()] = properties;
        }
        it_nodes.next();
    }
}

string NifMaterialExporterFallout::asString(bool verbose) const {
    stringstream out;
    out << NifMaterialExporterSkyrim::asString(verbose) << endl;
    out << "NifMaterialExporterFallout" << endl;
    return out.str();
}

const Type& NifMaterialExporterFallout::GetType() const {
    return TYPE;
}

const Type NifMaterialExporterFallout::TYPE("NifMaterialExporterFallout", &NifMaterialExporterSkyrim::TYPE);