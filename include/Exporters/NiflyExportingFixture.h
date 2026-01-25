#pragma once

#include <maya/MFileObject.h>
#include <maya/MStatus.h>

#include <string>
#include <vector>

struct NiflyExportOptions {
	std::vector<std::string> exportedShapes;
	unsigned int exportUserVersion2 = 0;
};

// Native nifly-based exporter for Skyrim/Fallout geometry.
class NiflyExportingFixture {
public:
	explicit NiflyExportingFixture(const NiflyExportOptions& options);

	MStatus WriteNodes(const MFileObject& file);

private:
	NiflyExportOptions exportOptions;

	bool ShouldExportNode(const MString& name) const;
};

