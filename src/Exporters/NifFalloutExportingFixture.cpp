#include "include/Exporters/NifFalloutExportingFixture.h"

NifFalloutExportingFixture::NifFalloutExportingFixture() {}

NifFalloutExportingFixture::NifFalloutExportingFixture(
    NifTranslatorDataRef translatorData,
    NifTranslatorOptionsRef translatorOptions,
    NifTranslatorUtilsRef translatorUtils)
{
    this->translatorOptions = translatorOptions;
    this->translatorData = translatorData;
    this->translatorUtils = translatorUtils;
    this->nodeExporter = new NifNodeExporter(translatorOptions, translatorData, translatorUtils);
    this->meshExporter = new NifMeshExporterSkyrim(this->nodeExporter, translatorOptions, translatorData, translatorUtils);
    this->meshExporter->morphExporter = new NifMorphExporter(translatorOptions, translatorData, translatorUtils); // NEW
    this->materialExporter = new NifMaterialExporterFallout(translatorOptions, translatorData, translatorUtils);
    this->animationExporter = new NifAnimationExporter(translatorOptions, translatorData, translatorUtils);
}

string NifFalloutExportingFixture::asString(bool verbose) const {
    stringstream out;
    out << NifSkyrimExportingFixture::asString(verbose) << endl;
    return out.str();
}

const Type& NifFalloutExportingFixture::getType() const {
    return TYPE;
}

const Type NifFalloutExportingFixture::TYPE("NifFalloutExportingFixture", &NifSkyrimExportingFixture::TYPE);