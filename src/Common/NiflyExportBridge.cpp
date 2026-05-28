#include "include/Common/NiflyExportBridge.h"

#include "NifFile.hpp"

#include <filesystem>

bool ResaveWithNifly(const MFileObject& file, std::string& outError) {
	try {
		const std::filesystem::path filePath(file.resolvedFullName().asChar());
		nifly::NifFile nif;
		nifly::NifLoadOptions loadOptions;
		int loadResult = nif.Load(filePath, loadOptions);
		if (loadResult != 0) {
			outError = "Nifly resave failed to load exported NIF (error " + std::to_string(loadResult) + ")";
			return false;
		}

		nifly::NifSaveOptions saveOptions;
		int saveResult = nif.Save(filePath, saveOptions);
		if (saveResult != 0) {
			outError = "Nifly resave failed to save exported NIF (error " + std::to_string(saveResult) + ")";
			return false;
		}

		return true;
	} catch (const std::exception& e) {
		outError = std::string("Nifly resave exception: ") + e.what();
		return false;
	} catch (...) {
		outError = "Nifly resave exception: unknown error";
		return false;
	}
}


