#include "include/Exporters/NifDefaultExportingFixture.h"
#include "include/Exporters/NifCollisionExporter.h"
#include <maya/MItDag.h>
#include <maya/MFnDagNode.h>
#include "include/Exporters/NifKFAnimationExporter.h"

NifDefaultExportingFixture::NifDefaultExportingFixture() {

}

NifDefaultExportingFixture::NifDefaultExportingFixture(NifTranslatorDataRef translatorData, NifTranslatorOptionsRef translatorOptions, NifTranslatorUtilsRef translatorUtils) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
	this->nodeExporter = new NifNodeExporter(translatorOptions, translatorData, translatorUtils);
	this->meshExporter = new NifMeshExporter(this->nodeExporter, translatorOptions, translatorData, translatorUtils);
	this->meshExporter->morphExporter = new NifMorphExporter(translatorOptions, translatorData, translatorUtils); // NEW: create and attach the morph exporter
	this->materialExporter = new NifMaterialExporter(translatorOptions, translatorData, translatorUtils);
	this->animationExporter = new NifAnimationExporter(translatorOptions, translatorData, translatorUtils);

	// Same wiring pattern as collisionImporter on the import side
	// (NifDefaultImportingFixture's constructor). Set on nodeExporter so
	// ExportDAGNodes can check each exported joint's own DAG children for a
	// collision transform; also used directly below in WriteNodes() for the
	// scene-root collision case (collision attached to a "bhkRigidBody"/
	// "bhkRigidBodyT" transform that sits at the top level of the Maya
	// scene, with no joint parent at all - mirrors how the importer gives
	// root-level collision data its own dedicated handling rather than
	// relying on the recursive per-node import).
	this->collisionExporter = new NifCollisionExporter(translatorOptions, translatorData, translatorUtils);
	this->nodeExporter->collisionExporter = this->collisionExporter;

	// nodeExporter needs the KF exporter specifically (it owns BuildTransformInterpolator).
	this->nodeExporter->animationExporter = new NifKFAnimationExporter(translatorOptions, translatorData, translatorUtils);
}

MStatus NifDefaultExportingFixture::WriteNodes(const MFileObject& file) {
	try {
		//out << "Creating root node...";
		//Create new root node
		if (this->translatorOptions->exportBsFadeNodeRoot == true) {
			this->translatorData->exportedSceneRoot = new BSFadeNode;
		}
		else {
			this->translatorData->exportedSceneRoot = new NiNode;
		}

		this->translatorData->exportedSceneRoot->SetName("Scene Root");
		//out << sceneRoot << endl;

		// Set FNV-standard flags on root: Selective Update + Selective Update Transforms 
		// + Selective Update Controller + No Anim Sync
		this->translatorData->exportedSceneRoot->SetFlags(524302);


		//out << "Exporting shaders..." << endl;
		this->translatorData->shaders.clear();
		//Export shaders and textures
		this->materialExporter->ExportMaterials();

		//out << "Clearing Nodes" << endl;
		this->translatorData->nodes.clear();
		//out << "Clearing Meshes" << endl;
		this->translatorData->meshes.clear();
		//Export nodes

		//out << "Exporting nodes..." << endl;
		this->nodeExporter->ExportDAGNodes();

		// Scene-root collision: a "bhkRigidBody"/"bhkRigidBodyT" transform whose
		// parent was never exported as its own NiNode. Two cases:
		//   1. The collision transform itself has no parent at all (the
		//      importer creates these directly when importRootNode is off, with
		//      no wrapper transform around them).
		//   2. The collision transform's parent IS a real Maya joint/transform
		//      (e.g. "MMPistol"), but that joint was deliberately not selected
		//      for export (not in exportedJoints/exportedShapes) - its children
		//      become direct children of exportedSceneRoot/BSFadeNode instead.
		//      Confirmed with a real scene: ExportDAGNodes only runs its
		//      collision check INSIDE the branch that actually creates a NiNode
		//      for a joint - if isExportedJoint() says no for that joint, the
		//      branch (and the collision check inside it) never runs at all,
		//      even though the collision data is still there in the DAG.
		//
		// The reliable way to tell whether a transform got a NiNode is to check
		// translatorData->nodes directly (the map ExportDAGNodes itself
		// populates, keyed by MFnDagNode::fullPathName()) rather than guessing
		// from DAG parent depth, which doesn't actually correlate with whether
		// a given transform was selected for export.
		if (this->collisionExporter != NULL) {
			MItDag dagIt(MItDag::kDepthFirst, MFn::kTransform);
			for (; !dagIt.isDone(); dagIt.next()) {
				MFnDagNode dagFn(dagIt.item());
				MString itemName = dagFn.name();
				if (itemName != "bhkRigidBody" && itemName != "bhkRigidBodyT") {
					continue; // not a collision transform at all
				}

				bool parentHasNiNode = false;
				if (dagFn.parentCount() > 0) {
					MFnDagNode parentDagFn(dagFn.parent(0));
					string parentPath = parentDagFn.fullPathName().asChar();
					parentHasNiNode = (this->translatorData->nodes.find(parentPath) != this->translatorData->nodes.end());
				}

				if (!parentHasNiNode) {
					// Either no parent at all, or the parent exists in Maya but
					// was never given its own NiNode - either way, nobody else
					// is going to pick this collision up, so attach it to
					// exportedSceneRoot here.
					this->collisionExporter->ExportCollisionDirect(dagIt.item(), this->translatorData->exportedSceneRoot);
				}
				// If parentHasNiNode is true, the parent's own joint branch in
				// ExportDAGNodes already called ExportCollision for it - don't
				// do it again here.
			}
		}

		if (this->translatorOptions->exportMaterialType == "falloutmaterial") {
			BSXFlagsRef bsx_flags = new BSXFlags();
			bsx_flags->SetName("BSX");
			bsx_flags->SetData(67); // Bit 0 (Animated) + Bit 1 (Havok) + Bit 6 (Dynamic)
			this->translatorData->exportedSceneRoot->AddExtraData(
				StaticCast<NiExtraData>(bsx_flags)
			);
		}

		//out << "Enumerating skin clusters..." << endl;
		this->translatorData->meshClusters.clear();
		this->meshExporter->EnumerateSkinClusters();
		this->meshExporter->morphExporter->EnumerateBlendShapes(); // NEW: cache mesh -> blendShape lookups once, before any ExportMesh() calls

		//out << "Exporting meshes..." << endl;
		for (list<MObject>::iterator mesh = this->translatorData->meshes.begin(); mesh != this->translatorData->meshes.end(); ++mesh) {
			this->meshExporter->ExportMesh(*mesh);
		}

		//out << "Applying Skin offsets..." << endl;
		this->meshExporter->ApplyAllSkinOffsets(StaticCast<NiAVObject>(this->translatorData->exportedSceneRoot));



		//--Write finished NIF file--//
		NifInfo nif_info(this->translatorOptions->exportVersion, this->translatorOptions->exportUserVersion);
		nif_info.endian = ENDIAN_LITTLE;
		nif_info.exportInfo1 = "NifTools Maya NIF Plug-in " + string(PLUGIN_VERSION);
		nif_info.userVersion2 = this->translatorOptions->exportUserVersion2;

		{
			std::ofstream log("C:\\Users\\rober\\Documents\\maya\\2025\\scripts\\nifTranslator_debug.log",
				std::ios::out | std::ios::app);
			if (log.is_open()) {
				log << "[WriteNodes] exportVersion=" << this->translatorOptions->exportVersion << std::endl;
				log << "[WriteNodes] exportUserVersion=" << this->translatorOptions->exportUserVersion << std::endl;
				log << "[WriteNodes] exportUserVersion2=" << this->translatorOptions->exportUserVersion2 << std::endl;
				log << "[WriteNodes] exportMaterialType=" << this->translatorOptions->exportMaterialType << std::endl;
			}
		}

		WriteNifTree(file.fullName().asChar(), StaticCast<NiObject>(this->translatorData->exportedSceneRoot), nif_info);

		//out << "Export Complete." << endl;

		//Clear temporary data
		//out << "Clearing temporary data" << endl;
		this->translatorData->Reset();
	}
	catch (exception& e) {
		stringstream out;
		out << "Error:  " << e.what() << endl;
		MGlobal::displayError(out.str().c_str());
		return MStatus::kFailure;
	}
	catch (...) {
		MGlobal::displayError("Error:  Unknown Exception.");
		return MStatus::kFailure;
	}

	//#ifndef _DEBUG
	////Clear the stringstream so it doesn't waste a bunch of RAM
	//out.clear();
	//#endif

	return MS::kSuccess;
}

string NifDefaultExportingFixture::asString(bool verbose /*= false */) const {
	stringstream out;

	out << NifExportingFixture::asString(verbose) << endl;
	out << this->nodeExporter->asString(verbose) << endl;
	out << this->meshExporter->asString(verbose) << endl;
	out << this->materialExporter->asString(verbose) << endl;

	return out.str();
}

const Type& NifDefaultExportingFixture::getType() const {
	return TYPE;
}


const Type NifDefaultExportingFixture::TYPE("NifDefaultExportingFixture", &NifExportingFixture::TYPE);