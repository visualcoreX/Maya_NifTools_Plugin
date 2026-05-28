#pragma once

#include <maya/MFileObject.h>
#include <string>

// Resave an exported NIF using nifly to keep the export backend consistent.
// Returns true on success, false and fills outError on failure.
bool ResaveWithNifly(const MFileObject& file, std::string& outError);


