#include "include/Exporters/NifFalloutExportingFixture.h"
#include "include/Exporters/NifCollisionExporter.h"

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

    // Same wiring as NifDefaultExportingFixture's constructor - this class
    // has its own constructor (doesn't call the base one), so collisionExporter
    // needs to be wired here too, or collision data would silently never be
    // exported for the FNV ("falloutmaterial") path, which is what
    // NifTranslator.cpp actually uses for FNV exports.
    this->collisionExporter = new NifCollisionExporter(translatorOptions, translatorData, translatorUtils);
    this->nodeExporter->collisionExporter = this->collisionExporter;
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