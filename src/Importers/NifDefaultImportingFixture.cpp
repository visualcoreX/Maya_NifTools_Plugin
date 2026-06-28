#include "include/Importers/NifDefaultImportingFixture.h"
#include <fstream>
#include <iomanip>
#include <cmath>

// Exact Matrix44::IDENTITY == comparison fails for files with tiny
// accumulated floating-point rounding error from export/conversion tools -
// confirmed with a real file (10MMPistol.nif) whose root has a ~4.9e-08
// radian rotation that NifSkope displays as "-0.00" (visually zero) but is
// not bit-exact zero, causing the exact comparison to wrongly report the
// transform as non-identity. 1e-5 is comfortably above that kind of rounding
// noise while still being far below any rotation/translation a file would
// plausibly have on purpose.
static bool IsApproximatelyIdentity(const Matrix44& mat) {
	const float EPS = 1e-5f;
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			float expected = (row == col) ? 1.0f : 0.0f;
			if (fabsf(mat[row][col] - expected) > EPS) {
				return false;
			}
		}
	}
	return true;
}

NifDefaultImportingFixture::NifDefaultImportingFixture()
{
}

NifDefaultImportingFixture::NifDefaultImportingFixture(NifTranslatorDataRef translatorData, NifTranslatorOptionsRef translatorOptions, NifTranslatorUtilsRef translatorUtils)
{
	this->translatorData = translatorData;
	this->translatorOptions = translatorOptions;
	this->translatorUtils = translatorUtils;
	this->nodeImporter = new NifNodeImporter(translatorOptions, translatorData, translatorUtils);
	this->meshImporter = new NifMeshImporter(translatorOptions, translatorData, translatorUtils);
	this->materialImporter = new NifMaterialImporter(translatorOptions, translatorData, translatorUtils);
	this->animationImporter = new NifAnimationImporter(translatorOptions, translatorData, translatorUtils);
	this->collisionImporter = new NifCollisionImporter(translatorOptions, translatorData, translatorUtils);
	this->nodeImporter->collisionImporter = this->collisionImporter;
}

MStatus NifDefaultImportingFixture::ReadNodes(const MFileObject& file)
{
	try
	{
		NiObjectRef root = ReadNifTree(file.fullName().asChar());

		NiNodeRef root_node = DynamicCast<NiNode>(root);
		if (root_node != NULL)
		{
			if (this->translatorOptions->importBindPose)
			{
				SendNifTreeToBindPos(root_node);
			}

			if (this->translatorOptions->importCombineSkeletons)
			{
				this->translatorData->existingNodes.clear();
				MItDag dagIt(MItDag::kDepthFirst);

				for (; !dagIt.isDone(); dagIt.next())
				{
					MFnTransform transFn(dagIt.item());
					MDagPath nodePath;
					dagIt.getPath(nodePath);
					this->translatorData->existingNodes[transFn.name().asChar()] = nodePath;
				}

				NiAVObjectRef rootAV = DynamicCast<NiAVObject>(root);
				if (rootAV != NULL)
				{
					this->translatorUtils->AdjustSkeleton(rootAV);
				}
			}

			bool hasHashNode = false;
			vector<NiAVObjectRef> root_children_check = root_node->GetChildren();
			for (unsigned int i = 0; i < root_children_check.size(); ++i)
			{
				string childName = root_children_check[i]->GetName();
				if (childName.size() >= 2 && childName[0] == '#' && childName[1] == '#')
				{
					hasHashNode = true;
					break;
				}
			}

			// TEMPORARY DIAGNOSTIC - logs the root's local transform matrix at
			// high precision, to determine whether root_node->GetLocalTransform()
			// == Matrix44::IDENTITY is failing due to floating-point precision
			// (near-zero but not exactly zero values) or some other reason.
			// Remove once the importRootNode/hasHashNode issue is resolved.
			{
				Matrix44 rootMat = root_node->GetLocalTransform();
				bool isIdentityResult = IsApproximatelyIdentity(rootMat);
				const char* logPath = "C:\\Users\\sauron\\Documents\\maya\\2025\\scripts\\nifTranslator_debug.log";
				std::ofstream log(logPath, std::ios::out | std::ios::app);
				if (log.is_open()) {
					log << std::setprecision(15);
					log << "[NifDefaultImportingFixture] root local transform diagnostic:" << std::endl;
					log << "  row0: " << rootMat[0][0] << ", " << rootMat[0][1] << ", " << rootMat[0][2] << ", " << rootMat[0][3] << std::endl;
					log << "  row1: " << rootMat[1][0] << ", " << rootMat[1][1] << ", " << rootMat[1][2] << ", " << rootMat[1][3] << std::endl;
					log << "  row2: " << rootMat[2][0] << ", " << rootMat[2][1] << ", " << rootMat[2][2] << ", " << rootMat[2][3] << std::endl;
					log << "  row3: " << rootMat[3][0] << ", " << rootMat[3][1] << ", " << rootMat[3][2] << ", " << rootMat[3][3] << std::endl;
					log << "  hasHashNode=" << hasHashNode << ", isIdentityResult=" << isIdentityResult << ", importRootNode=" << this->translatorOptions->importRootNode << std::endl;
				}
			}

			// importRootNode lets the user choose whether the root NiNode
			// itself (e.g. BSFadeNode) becomes a real Maya joint/transform,
			// or is skipped so its children become the scene's top-level
			// objects (the long-standing default). When skipped, any data
			// attached directly to the root (not its children) - most
			// notably a bhkCollisionObject - would otherwise be silently
			// lost, since ImportNodes (the only place that checks for
			// collision) never runs on the root in that branch. The
			// dedicated "_RootCollision" transform below covers that case
			// when importRootNode is off; when it's on, the root gets a
			// real transform/joint via ImportNodes and its collision is
			// picked up naturally, same as any other node.
			//
			// NOTE: this check is now fully overridden by importRootNode,
			// even when hasHashNode is true. hasHashNode normally forces the
			// root to be imported regardless (it signals "##"-prefixed
			// children - weapon mechanism parts like ##Slide/##Trigger/
			// ##Hammer that are animated relative to the root as a whole),
			// so explicitly turning importRootNode off on a file like that
			// is unusual enough to warrant a warning rather than silently
			// doing something the file's structure suggests it doesn't want.
			if (hasHashNode && !this->translatorOptions->importRootNode) {
				MGlobal::displayWarning("NifDefaultImportingFixture: this file has \"##\"-prefixed children (e.g. weapon mechanism parts like ##Slide/##Trigger/##Hammer), which normally means the root node needs to be imported for their relative animation to work correctly. \"Import Root Node\" is currently OFF, so the root will be skipped anyway - this may break that animation.");
			}

			bool skipRoot = (IsApproximatelyIdentity(root_node->GetLocalTransform()) && !this->translatorOptions->importRootNode);

			if (skipRoot)
			{
				// Root has no transform and the user hasn't asked for it to
				// be imported, so treat it purely as the scene root - we
				// deliberately don't call ImportNodes on root_node itself
				// here (that would create an unwanted extra "Scene Root"
				// transform in Maya), only on its children below.
				//
				// Collision attached directly to the root still needs
				// somewhere to go though. Since ImportCollision now takes a
				// plain MObject (not MDagPath), MObject::kNullObj can be
				// passed straight through - ImportRigidBody's
				// MFnTransform::create(parentTransform) call accepts
				// kNullObj natively as "no parent, create at the Maya scene
				// root", so the resulting "bhkRigidBody"/"bhkRigidBodyT"
				// transform appears directly at the top level of the scene,
				// with no dedicated wrapper transform needed around it.
				if (this->collisionImporter != NULL) {
					this->collisionImporter->ImportCollision(root_node, MObject::kNullObj);
				}

				vector<NiAVObjectRef> root_children = root_node->GetChildren();

				bool reserved = MProgressWindow::reserve();

				if (reserved == true)
				{
					MProgressWindow::setProgressMin(0);
					MProgressWindow::setProgressMax(root_children.size() - 1);
					MProgressWindow::setTitle("Importing nodes");
					MProgressWindow::startProgress();
					MProgressWindow::setInterruptable(false);

					for (unsigned int i = 0; i < root_children.size(); ++i)
					{
						this->nodeImporter->ImportNodes(root_children[i], this->translatorData->importedNodes);
						MProgressWindow::advanceProgress(1);
					}

					MProgressWindow::endProgress();
				}
				else
				{
					for (unsigned int i = 0; i < root_children.size(); ++i)
					{
						this->nodeImporter->ImportNodes(root_children[i], this->translatorData->importedNodes);
					}
				}
			}
			else
			{
				// Either the root has a real transform, has "##"-prefixed
				// children, or the user explicitly asked for the root node
				// to be imported (importRootNode=true) - either way,
				// ImportNodes runs on the root itself, recursing into its
				// children normally and picking up any collision data
				// attached to the root the same way it would for any other
				// node.
				this->nodeImporter->ImportNodes(StaticCast<NiAVObject>(root_node), this->translatorData->importedNodes);
			}
		}
		else
		{
			NiAVObjectRef rootAVObj = DynamicCast<NiAVObject>(root);
			if (rootAVObj != NULL)
			{
				this->nodeImporter->ImportNodes(rootAVObj, this->translatorData->importedNodes);
			}
			else
			{
				MGlobal::displayError("The root of this NIF file is not derived from the NiAVObject class.  It cannot be imported.");
				return MStatus::kFailure;
			}
		}

		NiAVObjectRef rootAVObj = DynamicCast<NiAVObject>(root);
		if (rootAVObj != NULL)
		{
			this->materialImporter->ImportMaterialsAndTextures(rootAVObj);
		}

		bool reserved;

		reserved = MProgressWindow::reserve();

		if (reserved == true)
		{
			MProgressWindow::setProgressMin(0);
			MProgressWindow::setProgressMax(this->translatorData->importedMeshes.size() - 1);
			MProgressWindow::setTitle("Importing meshes");
			MProgressWindow::startProgress();

			for (unsigned i = 0; i < this->translatorData->importedMeshes.size(); ++i)
			{
				MDagPath meshPath = this->meshImporter->ImportMesh(this->translatorData->importedMeshes[i].first, this->translatorData->importedMeshes[i].second);

				MProgressWindow::advanceProgress(1);
			}

			MProgressWindow::endProgress();
		}
		else
		{
			for (unsigned i = 0; i < this->translatorData->importedMeshes.size(); ++i)
			{
				MDagPath meshPath = this->meshImporter->ImportMesh(this->translatorData->importedMeshes[i].first, this->translatorData->importedMeshes[i].second);
			}
		}

		MGlobal::clearSelectionList();

		this->translatorData->Reset();
	}
	catch (exception& e)
	{
		const string message = e.what();
		if (message.find("premature end of stream") != string::npos) {
			const char* logPath = "C:\\Users\\sauron\\Documents\\maya\\2025\\scripts\\nifTranslator_debug.log";
			std::ofstream log(logPath, std::ios::out | std::ios::app);
			if (log.is_open()) {
				log << "[NifDefaultImportingFixture] abort: premature end of stream" << std::endl;
			}
			MGlobal::displayError("NIF import failed: premature end of stream");
			return MStatus::kFailure;
		}
		MGlobal::displayError(e.what());
		return MStatus::kFailure;
	}
	catch (...)
	{
		MGlobal::displayError("Error:  Unknown Exception.");
		return MStatus::kFailure;
	}
}

string NifDefaultImportingFixture::asString(bool verbose /*= false */) const
{
	stringstream out;

	out << NifImportingFixture::asString(verbose) << endl;
	out << "NifDefaultImporterFixture" << endl;
	out << "NifTranslatorData" << endl;
	out << this->translatorData->asString(verbose) << endl;
	out << "NifTranslatorOptions" << endl;
	out << this->translatorOptions->asString(verbose) << endl;
	out << "NifTranslatorUtils" << endl;
	out << this->translatorUtils->asString(verbose) << endl;
	out << "NifNodeImporter" << endl;
	out << this->nodeImporter->asString(verbose) << endl;
	out << "NifMeshImporter" << endl;
	out << this->meshImporter->asString(verbose) << endl;
	out << "NifMaterialImporter" << endl;
	out << this->materialImporter->asString(verbose) << endl;
	out << "NifAnimationImporter" << endl;
	out << this->animationImporter->asString(verbose) << endl;

	return out.str();
}

const Type& NifDefaultImportingFixture::GetType() const
{
	return TYPE;
}

const Type NifDefaultImportingFixture::TYPE("NifDefaultImporterFixture", &NifImportingFixture::TYPE);