#ifndef _NIFMATERIALEXPORTERFALLOUT_H
#define _NIFMATERIALEXPORTERFALLOUT_H
#include "NifMaterialExporterSkyrim.h"
#include <obj/BSShaderPPLightingProperty.h>

class NifMaterialExporterFallout : public NifMaterialExporterSkyrim {
public:
    NifMaterialExporterFallout() {}
    NifMaterialExporterFallout(NifTranslatorOptionsRef translator_options,
        NifTranslatorDataRef translator_data,
        NifTranslatorUtilsRef translator_utils);
    virtual void ExportMaterials();
    virtual string asString(bool verbose = false) const;
    virtual const Type& GetType() const;
    const static Type TYPE;

    unsigned int stringToFNVShaderFlags(MString flags_string);

    BSShaderType stringToFNVShaderType(MString type_string);
};
#endif