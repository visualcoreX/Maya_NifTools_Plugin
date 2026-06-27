#include "include/Importers/NifDefaultImportingFixture.h"
#include <fstream>

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
			if (root_node->GetLocalTransform() == Matrix44::IDENTITY && !hasHashNode)
			{
				// Root has no transform, so treat it as the scene root - we
				// deliberately don't call ImportNodes on root_node itself
				// here (that would create an unwanted extra "Scene Root"
				// transform in Maya), only on its children below.
				//
				// BUT: any data attached directly to the root itself (not
				// its children) would then be silently lost, since
				// ImportNodes is the only place that currently checks for a
				// collision object. Confirmed with a real FNV file where
				// bhkCollisionObject was attached straight to the root
				// BSFadeNode - the import never even checked for it. Fix:
				// explicitly check for collision data on root_node here, and
				// if there is any, give it a small dedicated transform under
				// the scene root (named after the root node) to attach to -
				// same approach NifNodeImporter uses for any other node's
				// collision data, just done once here instead of inside the
				// recursive ImportNodes.
				if (this->collisionImporter != NULL) {
					MFnTransform rootCollisionTransFn;
					MObject rootCollisionTransform = rootCollisionTransFn.create(MObject::kNullObj);
					MString rootName = this->translatorUtils->MakeMayaName(root_node->GetName());
					rootCollisionTransFn.setName(rootName + "_RootCollision");

					MDagPath rootCollisionDagPath;
					rootCollisionTransFn.getPath(rootCollisionDagPath);

					this->collisionImporter->ImportCollision(root_node, rootCollisionDagPath);

					// If no collision was actually found on the root, this
					// transform is harmless but unnecessary clutter - remove
					// it. ImportCollision returns success either way (no
					// collision found is not an error), so check the
					// transform's child count instead: ImportCollision adds
					// a "bhkRigidBody"/"bhkRigidBodyT" child only when it
					// actually finds something to import.
					MFnDagNode rootCollisionDagFn(rootCollisionTransform);
					if (rootCollisionDagFn.childCount() == 0) {
						MGlobal::deleteNode(rootCollisionTransform);
					}
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