#include "NifTranslator.h"
#include "include/Importers/NiflyImporter.h"
#include "include/Common/NiflyExportBridge.h"
#include "include/Exporters/NiflyExportingFixture.h"
#include "include/Custom Nodes/NifBhkRigidBody.h"
#include "include/Custom Nodes/NifBhkShape.h"
#include <fstream>
#include <ctime>
#include <iomanip>
#include <filesystem>

//#define _DEBUG
ofstream out;

//--Function Definitions--//

//--NifTranslator::identifyFile--//

MPxFileTranslator::MFileKind NifTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const {
	MString fName = fileName.name();
	if (fName.toUpperCase() != "NIF" && fName.toUpperCase() != "KF")
		return kNotMyFileType;

	return kIsMyFileType;
}

//--Plug-in Load/Unload--//

//--initializePlugin--//

MStatus initializePlugin(MObject obj) {
#ifdef _DEBUG
	out.open("C:\\Maya NIF Plug-in Log.txt", ofstream::binary);
#endif

	MStatus   status;
	MFnPlugin plugin(obj, "NifTools", PLUGIN_VERSION);

	status = plugin.registerFileTranslator(TRANSLATOR_NAME,
		"nifTranslator.rgb",
		NifTranslator::creator,
		"nifTranslatorOpts",
		NULL,
		false);

	if (!status) {
		status.perror("registerFileTranslator");
		return status;
	}

	status = plugin.registerNode("nifDismemberPartition", NifDismemberPartition::id, NifDismemberPartition::creator, NifDismemberPartition::initialize, MPxNode::kDependNode);
	if (!status) {
		status.perror("registerNifDismemberPartition");
		return status;
	}

	status = plugin.registerNode("nifBhkRigidBody", NifBhkRigidBody::id,
		NifBhkRigidBody::creator, NifBhkRigidBody::initialize);
	if (!status) {
		status.perror("registerNode nifBhkRigidBody failed");
		MGlobal::displayError(MString("registerNode nifBhkRigidBody failed: ") + status.errorString());
	}
	else {
		MGlobal::displayInfo("registerNode nifBhkRigidBody succeeded.");
	}

	// AETemplate for nifBhkRigidBody, defined inline as a MEL string instead
	// of a separate scripts/AE*.mel file - keeps the whole plugin self
	// contained, with nothing extra to lose during distribution/install.
	// This only customizes the "Bhk Flags" section (2-column checkbox grid
	// instead of Maya's default one-per-row stack); everything else on the
	// node falls through to -addExtraControls, which reproduces Maya's
	// normal automatic layout unchanged.
	{
		MString aeTemplateScript =
			"global proc AEnifBhkRigidBody_bhkFlagsNew(string $a0, string $a1, string $a2, string $a3, string $a4, string $a5, string $a6, string $a7, string $a8, string $a9, string $a10, string $a11) {\n"
			"    string $names[12];\n"
			"    $names[0]=\"Active\"; $names[1]=\"Reset Trans\"; $names[2]=\"Notify\"; $names[3]=\"Set Local\";\n"
			"    $names[4]=\"Dbg Display\"; $names[5]=\"Use Vel\"; $names[6]=\"Reset\"; $names[7]=\"Sync On Update\";\n"
			"    $names[8]=\"Blend Pos\"; $names[9]=\"Always Blend\"; $names[10]=\"Anim Targeted\"; $names[11]=\"Dismembered Limb\";\n"
			"    string $attrs[12];\n"
			"    $attrs[0]=$a0; $attrs[1]=$a1; $attrs[2]=$a2; $attrs[3]=$a3; $attrs[4]=$a4; $attrs[5]=$a5;\n"
			"    $attrs[6]=$a6; $attrs[7]=$a7; $attrs[8]=$a8; $attrs[9]=$a9; $attrs[10]=$a10; $attrs[11]=$a11;\n"
			"    columnLayout -adjustableColumn 1;\n"
			"    int $i;\n"
			"    for ($i = 0; $i < 6; $i++) {\n"
			"        rowLayout -numberOfColumns 4 -columnWidth4 150 20 150 20 -columnAttach4 \"right\" \"left\" \"right\" \"left\";\n"
			"            text -label ($names[$i] + \"  \");\n"
			"            checkBox -label \"\" (\"bhkFlagBox\" + $i);\n"
			"            text -label ($names[$i+6] + \"  \");\n"
			"            checkBox -label \"\" (\"bhkFlagBox\" + ($i+6));\n"
			"        setParent ..;\n"
			"    }\n"
			"    setParent ..;\n"
			"    AEnifBhkRigidBody_bhkFlagsReplace($a0,$a1,$a2,$a3,$a4,$a5,$a6,$a7,$a8,$a9,$a10,$a11);\n"
			"}\n"
			"global proc AEnifBhkRigidBody_bhkFlagsReplace(string $a0, string $a1, string $a2, string $a3, string $a4, string $a5, string $a6, string $a7, string $a8, string $a9, string $a10, string $a11) {\n"
			"    string $attrs[12];\n"
			"    $attrs[0]=$a0; $attrs[1]=$a1; $attrs[2]=$a2; $attrs[3]=$a3; $attrs[4]=$a4; $attrs[5]=$a5;\n"
			"    $attrs[6]=$a6; $attrs[7]=$a7; $attrs[8]=$a8; $attrs[9]=$a9; $attrs[10]=$a10; $attrs[11]=$a11;\n"
			"    int $i;\n"
			"    for ($i = 0; $i < 12; $i++) {\n"
			"        connectControl (\"bhkFlagBox\" + $i) $attrs[$i];\n"
			"    }\n"
			"}\n"
			"global proc AEnifBhkRigidBodyTemplate(string $nodeName) {\n"
			"    editorTemplate -suppress \"caching\";\n"
			"    editorTemplate -suppress \"frozen\";\n"
			"    editorTemplate -suppress \"nodeState\";\n"
			"    editorTemplate -beginScrollLayout;\n"
			"        editorTemplate -addControl \"targetShape\";\n"
			"        editorTemplate -beginLayout \"Bhk Flags\" -collapse 0;\n"
			"            editorTemplate -callCustom \"AEnifBhkRigidBody_bhkFlagsNew\" \"AEnifBhkRigidBody_bhkFlagsReplace\" \"bhkFlagActive\" \"bhkFlagResetTrans\" \"bhkFlagNotify\" \"bhkFlagSetLocal\" \"bhkFlagDbgDisplay\" \"bhkFlagUseVel\" \"bhkFlagReset\" \"bhkFlagSyncOnUpdate\" \"bhkFlagBlendPos\" \"bhkFlagAlwaysBlend\" \"bhkFlagAnimTargeted\" \"bhkFlagDismemberedLimb\";\n"
			"        editorTemplate -endLayout;\n"
			"        editorTemplate -addControl \"isHavokT\";\n"
			"        editorTemplate -addControl \"layer\";\n"
			"        editorTemplate -addControl \"havokFilterFlags\";\n"
			"        editorTemplate -addControl \"motionSystem\";\n"
			"        editorTemplate -addControl \"qualityType\";\n"
			"        editorTemplate -addControl \"deactivatorType\";\n"
			"        editorTemplate -addControl \"solverDeactivation\";\n"
			"        editorTemplate -addControl \"collisionResponse\";\n"
			"        editorTemplate -addControl \"mass\";\n"
			"        editorTemplate -addControl \"friction\";\n"
			"        editorTemplate -addControl \"restitution\";\n"
			"        editorTemplate -addControl \"linearDamping\";\n"
			"        editorTemplate -addControl \"angularDamping\";\n"
			"        editorTemplate -addControl \"maxLinearVelocity\";\n"
			"        editorTemplate -addControl \"maxAngularVelocity\";\n"
			"        editorTemplate -addControl \"penetrationDepth\";\n"
			"        editorTemplate -addControl \"havokTranslation\";\n"
			"        editorTemplate -addControl \"havokRotation\";\n"
			"        editorTemplate -addControl \"havokRotationW\";\n"
			"        editorTemplate -addControl \"center\";\n"
			"        editorTemplate -addControl \"inertiaRow1\";\n"
			"        editorTemplate -addControl \"inertiaRow2\";\n"
			"        editorTemplate -addControl \"inertiaRow3\";\n"
			"    editorTemplate -endScrollLayout;\n"
			"}\n";

		MStatus aeStatus = MGlobal::executeCommand(aeTemplateScript, true, false);
		if (!aeStatus) {
			MGlobal::displayWarning("Failed to register AEnifBhkRigidBodyTemplate - Bhk Flags will show in Maya's default one-column layout instead of 2 columns.");
		}
	}

	status = plugin.registerNode("nifBhkShape", NifBhkShape::id,
		NifBhkShape::creator, NifBhkShape::initialize);
	if (!status) {
		status.perror("registerNode nifBhkShape failed");
		MGlobal::displayError(MString("registerNode nifBhkShape failed: ") + status.errorString());
	}
	else {
		MGlobal::displayInfo("registerNode nifBhkShape succeeded.");
	}

	// AETemplate for nifBhkShape - just suppresses the standard
	// Caching/Frozen/Node State rows (same reasoning as nifBhkRigidBody:
	// compute() does nothing, so there's nothing for these to apply to) and
	// lists the node's two real attributes explicitly so the page reads as
	// one unified flow instead of splitting into a separate Extra Attributes
	// section.
	{
		MString aeShapeTemplateScript =
			"global proc AEnifBhkShapeTemplate(string $nodeName) {\n"
			"    editorTemplate -suppress \"caching\";\n"
			"    editorTemplate -suppress \"frozen\";\n"
			"    editorTemplate -suppress \"nodeState\";\n"
			"    editorTemplate -beginScrollLayout;\n"
			"        editorTemplate -addControl \"material\";\n"
			"        editorTemplate -addControl \"materialOverriddenByList\";\n"
			"        editorTemplate -addControl \"radius\";\n"
			"    editorTemplate -endScrollLayout;\n"
			"}\n";

		MStatus aeShapeStatus = MGlobal::executeCommand(aeShapeTemplateScript, true, false);
		if (!aeShapeStatus) {
			MGlobal::displayWarning("Failed to register AEnifBhkShapeTemplate.");
		}
	}

	status = plugin.registerNode("bsLightningShader", BSLightningShader::id, BSLightningShader::creator, BSLightningShader::initialize, MPxNode::kDependNode);
	if (!status) {
		status.perror("registerBSLightningShader");
		return status;
	}

	MGlobal::executeCommand("nifTranslatorMenuCreate");

	return status;
}

//--uninitializePlugin--//

MStatus uninitializePlugin(MObject obj)
{
	MStatus   status;
	MFnPlugin plugin(obj);

	status = plugin.deregisterFileTranslator(TRANSLATOR_NAME);
	if (!status) {
		status.perror("deregisterFileTranslator");
		return status;
	}

	status = plugin.deregisterNode(NifDismemberPartition::id);
	if (!status) {
		status.perror("deregisterFileTranslator");
		return status;
	}

	status = plugin.deregisterNode(NifBhkRigidBody::id);
	if (!status) {
		status.perror("deregisterNode nifBhkRigidBody failed");
	}

	status = plugin.deregisterNode(NifBhkShape::id);
	if (!status) {
		status.perror("deregisterNode nifBhkShape failed");
	}

	MGlobal::executeCommand("nifTranslatorMenuRemove");
	if (!status) {
		status.perror("deregisterNifDismemberPartition");
		return status;
	}

	return status;
}

//--NifTranslator::reader--//

MStatus NifTranslator::reader(const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	NifTranslatorDataRef translator_data(new NifTranslatorData());
	NifTranslatorOptionsRef translator_options(new NifTranslatorOptions());
	NifTranslatorUtilsRef translator_utils(new NifTranslatorUtils(translator_data, translator_options));

	translator_options->ParseOptionsString(optionsString);

	const char* logPath = "C:\\Users\\sauron\\Documents\\maya\\2025\\scripts\\nifTranslator_debug.log";
	std::string filename = file.fullName().asChar();

	{
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			std::time_t now = std::time(nullptr);
			log << "[NifTranslator::reader] time=" << std::put_time(std::localtime(&now), "%F %T") << std::endl;
			log << "[NifTranslator::reader] file=" << filename << std::endl;
		}
	}

	if (NiflyImporter::ShouldUseNifly(filename)) {
		{
			std::ofstream log(logPath, std::ios::out | std::ios::app);
			if (log.is_open()) {
				log << "[NifTranslator::reader] Using NiflyImporter for modern NIF" << std::endl;
			}
		}

		NiflyImportOptions niflyOptions;
		niflyOptions.importScale = translator_options->importScale;
		niflyOptions.texturePath = translator_options->texturePath;
		niflyOptions.importFileDir = file.rawPath().asChar();
		niflyOptions.useNameMangling = translator_options->useNameMangling;
		niflyOptions.importNormalizedWeights = translator_options->importNormalizedWeights;

		NiflyImporter niflyImporter;
		niflyImporter.SetOptions(niflyOptions);
		MStatus status = niflyImporter.Import(file);

		if (status == MStatus::kSuccess) {
			return status;
		}

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

		if (block_types[block_types_index[0]] == NiControllerSequence::TYPE.GetTypeName()) {
			import_type = ImportType::AnimationKF;
		}
		else if (block_types[block_types_index[0]] == BSFadeNode::TYPE.GetTypeName() ||
			file_header.getUserVersion() == 12 ||
			(file_header.getUserVersion() == 11 && file_header.getUserVersion2() == 57) ||
			(file_header.getUserVersion() == 11 && file_header.getUserVersion2() == 34)) {
			import_type = ImportType::SkyrimFallout;
		}
		else {
			for (size_t i = 0; i < block_types.size(); i++) {
				if (block_types[i] == BSDismemberSkinInstance::TYPE.GetTypeName() || block_types[i] == BSShaderTextureSet::TYPE.GetTypeName()) {
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

		if (import_type == ImportType::AnimationKF) {
			importer = new NifKFImportingFixture(translator_options, translator_data, translator_utils);
		}
		else if (import_type == ImportType::SkyrimFallout) {
			importer = new NifSkyrimImportingFixture(translator_options, translator_data, translator_utils);
		}
		else if (import_type == ImportType::Default) {
			importer = new NifDefaultImportingFixture(translator_data, translator_options, translator_utils);
		}

		return importer->ReadNodes(file);

	}
	catch (const std::exception& e) {
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			log << "[NifTranslator::reader] exception: " << e.what() << std::endl;
		}
		MGlobal::displayError(MString("NIF import failed: ") + e.what());
		return MStatus::kFailure;
	}
	catch (...) {
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			log << "[NifTranslator::reader] unknown exception." << std::endl;
		}
		MGlobal::displayError("NIF import failed: unknown exception.");
		return MStatus::kFailure;
	}
}

//--NifTranslator::writer--//

MStatus NifTranslator::writer(const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	NifTranslatorOptionsRef translator_options(new NifTranslatorOptions());
	NifTranslatorDataRef translator_data(new NifTranslatorData());
	NifTranslatorUtilsRef translator_utils(new NifTranslatorUtils(translator_data, translator_options));

	translator_options->ParseOptionsString(optionsString);

	NifExportingFixtureRef exporting_fixture;

	string export_type = "geometry";
	if (translator_options->exportType.length() > 1) {
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

	if (export_type == "geometry") {
		if (translator_options->exportMaterialType == "standardmaterial") {
			exporting_fixture = new NifDefaultExportingFixture(translator_data, translator_options, translator_utils);
		}
		if (translator_options->exportMaterialType == "falloutmaterial") {
			exporting_fixture = new NifFalloutExportingFixture(
				translator_data, translator_options, translator_utils);
		}
		if (translator_options->exportMaterialType == "skyrimmaterial") {
			NiflyExportOptions niflyOptions;
			niflyOptions.exportedShapes = translator_options->exportedShapes;
			niflyOptions.exportUserVersion2 = translator_options->exportUserVersion2;
			NiflyExportingFixture niflyExporter(niflyOptions);
			return niflyExporter.WriteNodes(file);
		}
	}

	if (export_type == "animation") {
		exporting_fixture = new NifKFExportingFixture(translator_options, translator_data, translator_utils);
	}

	if (exporting_fixture != NULL) {
		MStatus status = exporting_fixture->WriteNodes(file);
		if (status != MStatus::kSuccess) {
			return status;
		}

		if (export_type == "geometry" && translator_options->exportMaterialType == "skyrimmaterial") {
			std::string resaveError;
			if (!ResaveWithNifly(file, resaveError)) {
				MGlobal::displayWarning(MString(resaveError.c_str()));
			}
			else {
				MGlobal::displayInfo("Nifly resave completed.");
			}
		}

		return status;
	}

	return MStatus::kFailure;
}