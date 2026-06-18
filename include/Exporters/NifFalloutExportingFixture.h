#ifndef _NIFFALLOUTEXPORTINGFIXTURE_H
#define _NIFFALLOUTEXPORTINGFIXTURE_H
#include "NifSkyrimExportingfixture.h"
#include "NifMaterialExporterFallout.h"

class NifFalloutExportingFixture : public NifSkyrimExportingFixture {
public:
    NifFalloutExportingFixture();
    NifFalloutExportingFixture(NifTranslatorDataRef translatorData,
        NifTranslatorOptionsRef translatorOptions,
        NifTranslatorUtilsRef translatorUtils);
    virtual string asString(bool verbose = false) const;
    virtual const Type& getType() const;
    const static Type TYPE;
};
#endif