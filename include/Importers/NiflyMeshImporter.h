#pragma once

#include <maya/MStatus.h>
#include <maya/MFileObject.h>

#include "Common/NifTranslatorOptions.h"
#include "Common/NifTranslatorUtils.h"

class NiflyMeshImporter
{
public:
	static MStatus ImportFile(const MFileObject& file,
		const NifTranslatorOptionsRef& options,
		const NifTranslatorUtilsRef& utils);
};

