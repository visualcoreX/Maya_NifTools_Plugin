#include "include/Exporters/NifSkyrimExportingFixture.h"

NifSkyrimExportingFixture::NifSkyrimExportingFixture() {

}

NifSkyrimExportingFixture::NifSkyrimExportingFixture(NifTranslatorDataRef translatorData, NifTranslatorOptionsRef translatorOptions, NifTranslatorUtilsRef translatorUtils) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
	this->nodeExporter = new NifNodeExporter(translatorOptions, translatorData, translatorUtils);
	this->meshExporter = new NifMeshExporterSkyrim(this->nodeExporter, translatorOptions, translatorData, translatorUtils);
	this->materialExporter = new NifMaterialExporterSkyrim(translatorOptions, translatorData, translatorUtils);
	this->animationExporter = new NifAnimationExporter(translatorOptions, translatorData, translatorUtils);
}


string NifSkyrimExportingFixture::asString(bool verbose /*= false */) const {
	stringstream out;

	out << NifDefaultExportingFixture::asString(verbose) << endl;
	out << this->nodeExporter->asString(verbose) << endl;
	out << this->meshExporter->asString(verbose) << endl;
	out << this->materialExporter->asString(verbose) << endl;

	return out.str();
}

const Type& NifSkyrimExportingFixture::getType() const {
	return TYPE;
}


const Type NifSkyrimExportingFixture::TYPE("NifSkyrimExportingFixture", &NifDefaultExportingFixture::TYPE);
