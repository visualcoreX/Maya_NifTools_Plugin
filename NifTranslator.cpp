#include "NifTranslator.h"
#include "include/Importers/NiflyImporter.h"
#include "include/Common/NiflyExportBridge.h"
#include "include/Exporters/NiflyExportingFixture.h"
#include <fstream>
#include <ctime>
#include <iomanip>
#include <filesystem>

//#define _DEBUG
ofstream out;

//--Function Definitions--//

//--NifTranslator::identifyFile--//

// This routine must use passed data to determine if the file is of a type supported by this translator

// Code adapted from animImportExport example that comes with Maya 6.5

MPxFileTranslator::MFileKind NifTranslator::identifyFile (const MFileObject& fileName, const char* buffer, short size) const {
	MString fName = fileName.name();
	if(fName.toUpperCase() != "NIF" && fName.toUpperCase() != "KF")
		return kNotMyFileType;

	return kIsMyFileType;
}

//--Plug-in Load/Unload--//

//--initializePlugin--//

// Code adapted from lepTranslator example that comes with Maya 6.5
MStatus initializePlugin( MObject obj ) {
#ifdef _DEBUG
	out.open( "C:\\Maya NIF Plug-in Log.txt", ofstream::binary );
#endif

	//out << "Initializing Plugin..." << endl;
	MStatus   status;
	MFnPlugin plugin( obj, "NifTools", PLUGIN_VERSION );

	// Register the translator with the system
	status =  plugin.registerFileTranslator( TRANSLATOR_NAME,  //File Translator Name
		"nifTranslator.rgb", //Icon
		NifTranslator::creator, //Factory Function
		"nifTranslatorOpts", //MEL Script for options dialog
		NULL, //Default Options
		false ); //Requires MEL support

	if (!status) {
		status.perror("registerFileTranslator");
		return status;
	}

	status = plugin.registerNode("nifDismemberPartition", NifDismemberPartition::id, NifDismemberPartition::creator, NifDismemberPartition::initialize, MPxNode::kDependNode);
	if (!status) {
		status.perror("registerNifDismemberPartition");
		return status;
	}


	status = plugin.registerNode("bsLightningShader", BSLightningShader::id, BSLightningShader::creator, BSLightningShader::initialize, MPxNode::kDependNode);
	if (!status) {
		status.perror("registerBSLightningShader");
		return status;
	}

	//Execute the command to create the NifTools Menu
	MGlobal::executeCommand("nifTranslatorMenuCreate");


	//out << "Done Initializing." << endl;
	return status;
}

//--uninitializePlugin--//

// Code adapted from lepTranslator example that comes with Maya 6.5
MStatus uninitializePlugin( MObject obj )
{
	MStatus   status;
	MFnPlugin plugin( obj );

	status =  plugin.deregisterFileTranslator( TRANSLATOR_NAME );
	if (!status) {
		status.perror("deregisterFileTranslator");
		return status;
	}

	status = plugin.deregisterNode(NifDismemberPartition::id);
	if (!status) {
		status.perror("deregisterFileTranslator");
		return status;
	}

	//Execute the command to delete the NifTools Menu
	MGlobal::executeCommand("nifTranslatorMenuRemove");
	if (!status) {
		status.perror("deregisterNifDismemberPartition");
		return status;
	}

	return status;
}

//--NifTranslator::reader--//

//This routine is called by Maya when it is necessary to load a file of a type supported by this translator.
//Responsible for reading the contents of the given file, and creating Maya objects via API or MEL calls to reflect the data in the file.
MStatus NifTranslator::reader	 (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	NifTranslatorDataRef translator_data(new NifTranslatorData());
	NifTranslatorOptionsRef translator_options(new NifTranslatorOptions());
	NifTranslatorUtilsRef translator_utils(new NifTranslatorUtils(translator_data,translator_options));

	translator_options->ParseOptionsString(optionsString);

	const char* logPath = "C:\\Users\\rober\\Documents\\maya\\2025\\scripts\\nifTranslator_debug.log";
	std::string filename = file.fullName().asChar();
	
	// Log the import attempt
	{
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			std::time_t now = std::time(nullptr);
			log << "[NifTranslator::reader] time=" << std::put_time(std::localtime(&now), "%F %T") << std::endl;
			log << "[NifTranslator::reader] file=" << filename << std::endl;
		}
	}

	// First, try to use nifly-based importer for modern NIF files (Skyrim SE, FO4, etc.)
	// This handles BSTriShape and other modern formats correctly
	if (NiflyImporter::ShouldUseNifly(filename)) {
		{
			std::ofstream log(logPath, std::ios::out | std::ios::app);
			if (log.is_open()) {
				log << "[NifTranslator::reader] Using NiflyImporter for modern NIF" << std::endl;
			}
		}
		
		// Create import options from translator options
		NiflyImportOptions niflyOptions;
		niflyOptions.importScale = translator_options->importScale;
		niflyOptions.texturePath = translator_options->texturePath;
		niflyOptions.importFileDir = file.rawPath().asChar();
		niflyOptions.useNameMangling = translator_options->useNameMangling;
		
		NiflyImporter niflyImporter;
		niflyImporter.SetOptions(niflyOptions);
		MStatus status = niflyImporter.Import(file);
		
		if (status == MStatus::kSuccess) {
			return status;
		}
		
		// Log the error but don't fall back - nifly should handle these files
		std::string error = niflyImporter.GetLastError();
		{
			std::ofstream log(logPath, std::ios::out | std::ios::app);
			if (log.is_open()) {
				log << "[NifTranslator::reader] NiflyImporter failed: " << error << std::endl;
			}
		}
		MGlobal::displayError(MString("NIF import failed: ") + error.c_str());
		return MStatus::kFailure;
	}

	// Fall back to niflib for older NIF formats
	{
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			log << "[NifTranslator::reader] Using niflib for legacy NIF" << std::endl;
		}
	}

	NifImportingFixtureRef importer;
	ImportType import_type = ImportType::Default;
	
	try {
		Header file_header = ReadHeader(filename.c_str());

		vector<string> block_types = file_header.getBlockTypes();
		vector<unsigned short> block_types_index = file_header.getBlockTypeIndex();

		if(block_types[block_types_index[0]] == NiControllerSequence::TYPE.GetTypeName()) {
			import_type = ImportType::AnimationKF;
		} else if(block_types[block_types_index[0]] == BSFadeNode::TYPE.GetTypeName() ||
			 file_header.getUserVersion() == 12 || (file_header.getUserVersion() == 11 && file_header.getUserVersion2() == 57) ) {
			import_type = ImportType::SkyrimFallout;
		} else {
			for(size_t i = 0; i < block_types.size(); i++) {
				if(block_types[i] == BSDismemberSkinInstance::TYPE.GetTypeName() || block_types[i] == BSShaderTextureSet::TYPE.GetTypeName()) {
					import_type = ImportType::SkyrimFallout;
				}
			}
		}

		{
			std::ofstream log(logPath, std::ios::out | std::ios::app);
			if (log.is_open()) {
				log << "[NifTranslator::reader] importType=" << static_cast<int>(import_type) << std::endl;
			}
		}

		if(import_type == ImportType::AnimationKF) {
			importer = new NifKFImportingFixture(translator_options, translator_data, translator_utils);
		} else if (import_type == ImportType::SkyrimFallout) {
			importer = new NifSkyrimImportingFixture(translator_options, translator_data, translator_utils);
		} else if (import_type == ImportType::Default) {
			importer = new NifDefaultImportingFixture(translator_data, translator_options, translator_utils);
		}

		return importer->ReadNodes(file);
		
	} catch (const std::exception& e) {
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			log << "[NifTranslator::reader] exception: " << e.what() << std::endl;
		}
		MGlobal::displayError(MString("NIF import failed: ") + e.what());
		return MStatus::kFailure;
	} catch (...) {
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			log << "[NifTranslator::reader] unknown exception." << std::endl;
		}
		MGlobal::displayError("NIF import failed: unknown exception.");
		return MStatus::kFailure;
	}
}

//--NifTranslator::writer--//

//This routine is called by Maya when it is necessary to save a file of a type supported by this translator.
//Responsible for traversing all objects in the current Maya scene, and writing a representation to the given
//file in the supported format.
MStatus NifTranslator::writer (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	NifTranslatorOptionsRef translator_options(new NifTranslatorOptions());
	NifTranslatorDataRef translator_data(new NifTranslatorData());
	NifTranslatorUtilsRef translator_utils(new NifTranslatorUtils(translator_data, translator_options));

	translator_options->ParseOptionsString(optionsString);

	NifExportingFixtureRef exporting_fixture;

	string export_type = "geometry";
	if(translator_options->exportType.length() > 1) {
		export_type = translator_options->exportType;
	}

	currentExportType = export_type;

	auto resolvedName = file.resolvedName();
	auto resolvedFileName = file.resolvedFullName();
	auto rawName = file.rawName();
	auto rawFullName = file.rawFullName();

	MGlobal::displayInfo(resolvedName);
	MGlobal::displayInfo(resolvedFileName);
	MGlobal::displayInfo(rawName);
	MGlobal::displayInfo(rawFullName);
	
	if(export_type == "geometry") {
		if(translator_options->exportMaterialType == "standardmaterial") {
			exporting_fixture = new NifDefaultExportingFixture(translator_data, translator_options, translator_utils);
		}
		if(translator_options->exportMaterialType == "skyrimmaterial") {
			// Use native nifly export path for Skyrim/Fallout
			NiflyExportOptions niflyOptions;
			niflyOptions.exportedShapes = translator_options->exportedShapes;
			niflyOptions.exportUserVersion2 = translator_options->exportUserVersion2;
			NiflyExportingFixture niflyExporter(niflyOptions);
			return niflyExporter.WriteNodes(file);
		}
	}

	if(export_type == "animation") {
		exporting_fixture = new NifKFExportingFixture(translator_options, translator_data, translator_utils);
	}
	
	if(exporting_fixture != NULL) {
		MStatus status = exporting_fixture->WriteNodes(file);
		if (status != MStatus::kSuccess) {
			return status;
		}

		// For Skyrim/Fallout exports, resave using nifly to keep backend consistent
		if (export_type == "geometry" && translator_options->exportMaterialType == "skyrimmaterial") {
			std::string resaveError;
			if (!ResaveWithNifly(file, resaveError)) {
				MGlobal::displayWarning(MString(resaveError.c_str()));
			} else {
				MGlobal::displayInfo("Nifly resave completed.");
			}
		}

		return status;
	}
	
	return MStatus::kFailure;
}

//MMatrix MatrixN2M( const Matrix44 & n ) {
//	//Copy Niflib matrix to Maya matrix
//
//	float myMat[4][4] = { 
//		n[0][0], n[0][1], n[0][2], n[0][3],
//		n[1][0], n[1][1], n[1][2], n[1][3],
//		n[2][0], n[2][1], n[2][2], n[2][3],
//		n[3][0], n[3][1], n[3][2], n[3][3]
//	};
//
//	return MMatrix(myMat);
//}
//
//Matrix44 MatrixM2N( const MMatrix & n ) {
//	//Copy Maya matrix to Niflib matrix
//	return Matrix44( 
//		(float)n[0][0], (float)n[0][1], (float)n[0][2], (float)n[0][3],
//		(float)n[1][0], (float)n[1][1], (float)n[1][2], (float)n[1][3],
//		(float)n[2][0], (float)n[2][1], (float)n[2][2], (float)n[2][3],
//		(float)n[3][0], (float)n[3][1], (float)n[3][2], (float)n[3][3]
//	);
//}
