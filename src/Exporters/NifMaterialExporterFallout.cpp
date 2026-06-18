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

            // Диффузная текстура (слот 0)
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

            // Нормал-мап (слот 1)
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

            shader_property->SetTextureSet(shader_textures);

            // NiMaterialProperty
            if (shader_node.object().hasFn(MFn::kPhong)) {
                MFnPhongShader phong_node(shader_node.object());
                MColor spec = phong_node.specularColor();
                mat_property->SetSpecularColor(Color3(spec.r, spec.g, spec.b));
                mat_property->SetGlossiness(phong_node.findPlug("cosinePower").asFloat());
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