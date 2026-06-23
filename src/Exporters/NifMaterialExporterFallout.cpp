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
                    alpha_property->SetFlags(237); // 237 = 0xED: alpha blending enabled, src=SRC_ALPHA, dst=INV_SRC_ALPHA, threshold=128
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
            // FNV shader flags
            unsigned int flags = SF_SPECULAR | SF_REMAPPABLE_TEXTURES | SF_ZBUFFER_TEST;
            MPlug flags_plug = shader_node.findPlug("fnvShaderFlags");
            if (!flags_plug.isNull())
                flags = this->stringToFNVShaderFlags(flags_plug.asString());
            shader_property->SetShaderFlags((BSShaderFlags)flags);

            // Read shader type from extra attribute, fall back to SHADER_DEFAULT
            BSShaderType shader_type = SHADER_DEFAULT;
            MPlug type_plug = shader_node.findPlug("fnvShaderType");
            if (!type_plug.isNull())
                shader_type = this->stringToFNVShaderType(type_plug.asString());
            shader_property->SetShaderType(shader_type);

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

unsigned int NifMaterialExporterFallout::stringToFNVShaderFlags(MString flags_string) {
    unsigned int flags = 0;
    MStringArray parts;
    flags_string.split('|', parts);
    struct Entry { const char* name; unsigned int bit; };
    Entry table[] = {
        { "SF_SPECULAR",                   1 },
        { "SF_SKINNED",                    2 },
        { "SF_LOWDETAIL",                  4 },
        { "SF_VERTEX_ALPHA",               8 },
        { "SF_UNKNOWN_1",                  16 },
        { "SF_SINGLE_PASS",                32 },
        { "SF_EMPTY",                      64 },
        { "SF_ENVIRONMENT_MAPPING",        128 },
        { "SF_ALPHA_TEXTURE",              256 },
        { "SF_UNKNOWN_2",                  512 },
        { "SF_FACEGEN",                    1024 },
        { "SF_PARALLAX_SHADER_INDEX_15",   2048 },
        { "SF_UNKNOWN_3",                  4096 },
        { "SF_NON_PROJECTIVE_SHADOWS",     8192 },
        { "SF_UNKNOWN_4",                  16384 },
        { "SF_REFRACTION",                 32768 },
        { "SF_FIRE_REFRACTION",            65536 },
        { "SF_EYE_ENVIRONMENT_MAPPING",    131072 },
        { "SF_HAIR",                       262144 },
        { "SF_DYNAMIC_ALPHA",              524288 },
        { "SF_LOCALMAP_HIDE_SECRET",       1048576 },
        { "SF_WINDOW_ENVIRONMENT_MAPPING", 2097152 },
        { "SF_TREE_BILLBOARD",             4194304 },
        { "SF_SHADOW_FRUSTUM",             8388608 },
        { "SF_MULTIPLE_TEXTURES",          16777216 },
        { "SF_REMAPPABLE_TEXTURES",        33554432 },
        { "SF_DECAL_SINGLE_PASS",          67108864 },
        { "SF_DYNAMIC_DECAL_SINGLE_PASS",  134217728 },
        { "SF_PARALLAX_OCCULSION",         268435456 },
        { "SF_EXTERNAL_EMITTANCE",         536870912 },
        { "SF_SHADOW_MAP",                 1073741824 },
        { "SF_ZBUFFER_TEST",               2147483648U },
    };
    for (unsigned int i = 0; i < parts.length(); i++) {
        for (auto& e : table) {
            if (parts[i] == e.name) {
                flags |= e.bit;
                break;
            }
        }
    }
    return flags;
}

// Convert fnvShaderType string attribute to BSShaderType enum value
BSShaderType NifMaterialExporterFallout::stringToFNVShaderType(MString type_string) {
    if (type_string == "SHADER_TALL_GRASS")  return SHADER_TALL_GRASS;
    if (type_string == "SHADER_SKY")         return SHADER_SKY;
    if (type_string == "SHADER_SKIN")        return SHADER_SKIN;
    if (type_string == "SHADER_WATER")       return SHADER_WATER;
    if (type_string == "SHADER_LIGHTING30")  return SHADER_LIGHTING30;
    if (type_string == "SHADER_TILE")        return SHADER_TILE;
    if (type_string == "SHADER_NOLIGHTING")  return SHADER_NOLIGHTING;
    // Default fallback
    return SHADER_DEFAULT;
}

const Type& NifMaterialExporterFallout::GetType() const {
    return TYPE;
}

const Type NifMaterialExporterFallout::TYPE("NifMaterialExporterFallout", &NifMaterialExporterSkyrim::TYPE);