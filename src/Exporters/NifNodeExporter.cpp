#include "include/Exporters/NifNodeExporter.h"
#include <cfloat> // FLT_MAX
#include "include/Exporters/NifKFAnimationExporter.h" // BuildTransformInterpolator for embedded animation

NifNodeExporter::NifNodeExporter() {

}

NifNodeExporter::NifNodeExporter(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils)
	: NifTranslatorFixtureItem(translatorOptions, translatorData, translatorUtils) {

}

void NifNodeExporter::ExportDAGNodes()
{
	//out << "NifTranslator::ExportDAGNodes {" << endl
		//<< "Creating DAG iterator..." << endl;
	//Create iterator to go through all DAG nodes depth first
	MItDag it(MItDag::kDepthFirst);

	//out << "Looping through all DAG nodes..." << endl;
	for (; !it.isDone(); it.next()) {
		//out << "Attaching function set for DAG node to the object." << endl;

		// attach a function set for a dag node to the
		// object. Rather than access data directly,
		// we access it via the function set.
		MFnDagNode nodeFn(it.item());

		const char* name = nodeFn.name().asChar();


		//out << "Object name is:  " << nodeFn.name().asChar() << endl;

		//Skip over Maya's default objects by name
		if (
			nodeFn.name().substring(0, 13) == "UniversalManip" ||
			nodeFn.name() == "groundPlane_transform" ||
			nodeFn.name() == "ViewCompass" ||
			nodeFn.name() == "Manipulator1" ||
			nodeFn.name() == "persp" ||
			nodeFn.name() == "top" ||
			nodeFn.name() == "front" ||
			nodeFn.name() == "side"
			) {
			continue;
		}

		// only want non-history items
		if (!nodeFn.isIntermediateObject()) {
			//out << "Object is not a history item" << endl;

			//Check if this is a transform node
			if (it.item().hasFn(MFn::kTransform)) {
				//This is a transform node, check if it is an IK joint or a shape
				//out << "Object is a transform node." << endl;

				NiAVObjectRef avObj;

				bool tri_shape = false;
				bool bounding_box = false;
				bool intermediate = false;
				MObject matching_child;

				MString nn = nodeFn.name();

				// Collision transforms and their shape geometry are handled
				// entirely by collisionExporter (called separately, in the
				// joint branch below and at the scene-root level in
				// NifDefaultExportingFixture::WriteNodes) - never as an
				// ordinary mesh/joint/bounding-box. Without this check, a
				// collision shape transform with real mesh geometry under
				// it (e.g. "bhkConvexVertices", which holds a real Maya
				// mesh) would get auto-included in the MEL export dialog's
				// exportedShapes list whenever it happened to be selected in
				// the scene, and isExportedShape() would then wrongly let it
				// through the tri_shape detection below, exporting it as a
				// plain NiTriStrips mesh instead of being skipped here and
				// reconstructed as proper collision geometry by
				// NifCollisionExporter.
				if (nn == "bhkRigidBody" || nn == "bhkRigidBodyT" ||
					nn == "bhkBoxShape" || nn == "bhkSphereShape" ||
					nn == "bhkCapsuleShape" || nn == "bhkConvexVertices" ||
					nn == "bhkPackedTriStrips" || nn == "bhkListShape") {
					continue;
				}

				//Check to see what kind of node we should create
				for (int i = 0; i != nodeFn.childCount(); ++i) {

					//out << "API Type:  " << nodeFn.child(i).apiTypeStr() << endl;
					// get a handle to the child
					if (nodeFn.child(i).hasFn(MFn::kMesh) && this->translatorUtils->isExportedShape(nodeFn.name())) {
						MFnMesh meshFn(nodeFn.child(i));
						//history items don't count
						if (!meshFn.isIntermediateObject()) {
							//out << "Object is a mesh." << endl;
							tri_shape = true;
							matching_child = nodeFn.child(i);
							break;
						}
						else {
							//This has an intermediate mesh under it.  Don't export it at all.
							intermediate = true;
							break;
						}

					}
					else if (nodeFn.child(i).hasFn(MFn::kBox)) {
						//out << "Found Bounding Box" << endl;

						//Set bounding box info
						BoundingBox bb;

						Matrix44 niMat = this->translatorUtils->MatrixM2N(nodeFn.transformationMatrix());

						bb.translation = niMat.GetTranslation();
						bb.rotation = niMat.GetRotation();
						bb.unknownInt = 1;

						//Get size of box
						MFnDagNode dagFn(nodeFn.child(i));
						dagFn.findPlug("sizeX").getValue(bb.radius.x);
						dagFn.findPlug("sizeY").getValue(bb.radius.y);
						dagFn.findPlug("sizeZ").getValue(bb.radius.z);

						//Find the parent NiNode, if any
						if (nodeFn.parentCount() > 0) {
							MFnDagNode parFn(nodeFn.parent(0));
							if (this->translatorData->nodes.find(parFn.fullPathName().asChar()) != this->translatorData->nodes.end()) {
								NiNodeRef parNode = this->translatorData->nodes[parFn.fullPathName().asChar()];
								parNode->SetBoundingBox(bb);
							}
						}

						bounding_box = true;
					}
				}

				if (!intermediate) {

					if (tri_shape == true) {
						//out << "Adding Mesh to list to be exported later..." << endl;
						this->translatorData->meshes.push_back(it.item());
						//NiTriShape
					}
					else if (bounding_box == true) {
						//Do nothing
					}
					else if (it.item().hasFn(MFn::kJoint) && this->translatorUtils->isExportedJoint(nodeFn.name())) {
						//out << "Creating a NiNode..." << endl;
						//NiNode
						NiNodeRef niNode = new NiNode;
						ExportAV(StaticCast<NiAVObject>(niNode), it.item(), this->translatorOptions->exportFlatenedSkeleton);

						//out << "Associating NiNode with node DagPath..." << endl;
						//Associate NIF object with node DagPath
						string path = nodeFn.fullPathName().asChar();
						this->translatorData->nodes[path] = niNode;

						//Parent should have already been created since we used a
						//depth first iterator in ExportDAGNodes
						NiNodeRef parNode = this->translatorUtils->GetDAGParent(it.item());

						if (this->translatorOptions->exportFlatenedSkeleton == false) {
							parNode = this->translatorUtils->GetDAGParent(it.item());
						}
						else {
							parNode = this->translatorData->exportedSceneRoot;
						}

						parNode->AddChild(StaticCast<NiAVObject>(niNode));

						// Check this joint's own Maya DAG children for a
						// collision transform (the importer creates
						// "bhkRigidBody"/"bhkRigidBodyT" as a direct child of
						// whichever transform the collision belongs to - see
						// NifCollisionImporter::ImportRigidBody). This does
						// NOT create a separate NiNode for the collision -
						// ExportCollision just attaches a bhkCollisionObject
						// to the niNode this joint already produced, the
						// same way NiNode::GetCollisionObject() was read on
						// import.
						if (this->collisionExporter != NULL) {
							this->collisionExporter->ExportCollision(it.item(), niNode);
						}
					}
				}
			}
		}
	}
	//out << "Loop complete" << endl;
}

void NifNodeExporter::ExportAV(NiAVObjectRef avObj, MObject dagNode, bool worldTransform)
{
	// attach a function set for a dag node to the object.
	MFnDagNode nodeFn(dagNode);

	//out << "Fixing name from " << nodeFn.name().asChar() << " to ";

	//Fix name
	string name = this->translatorUtils->MakeNifName(nodeFn.name());
	avObj->SetName(name);
	//out << name << endl;

	MMatrix my_trans;

	if (worldTransform == false) {
		MTransformationMatrix scaleTrans(my_trans);

		my_trans = nodeFn.transformationMatrix();

		//Warn user about any scaling problems
		double myScale[3];
		scaleTrans.getScale(myScale, MSpace::kPreTransform);
		if (abs(myScale[0] - 1.0) > 0.0001 || abs(myScale[1] - 1.0) > 0.0001 || abs(myScale[2] - 1.0) > 0.0001) {
			MGlobal::displayWarning("Some games such as the Elder Scrolls do not react well when scale is not 1.0.  Consider freezing scale transforms on all nodes before export.");
		}
		if (abs(myScale[0] - myScale[1]) > 0.0001 || abs(myScale[0] - myScale[2]) > 0.001 || abs(myScale[1] - myScale[2]) > 0.001) {
			MGlobal::displayWarning("The NIF format does not support separate scales for X, Y, and Z.  An average will be used.  Consider freezing scale transforms on all nodes before export.");
		}
	}
	else {
		MCommandResult result;
		MString command = "getAttr " + nodeFn.name() + ".worldMatrix";
		const char* as_char = command.asChar();
		MGlobal::executeCommand(command, result, true);
		MCommandResult::Type result_type = result.resultType();
		MDoubleArray worldMatrixArray;
		double worldMatrixSingleArray[16];
		double worldMatrix[4][4];
		result.getResult(worldMatrixArray);
		unsigned length = worldMatrixArray.length();

		worldMatrixArray.get(worldMatrixSingleArray);
		worldMatrix[0][0] = worldMatrixSingleArray[0];
		worldMatrix[0][1] = worldMatrixSingleArray[1];
		worldMatrix[0][2] = worldMatrixSingleArray[2];
		worldMatrix[0][3] = worldMatrixSingleArray[3];
		worldMatrix[1][0] = worldMatrixSingleArray[4];
		worldMatrix[1][1] = worldMatrixSingleArray[5];
		worldMatrix[1][2] = worldMatrixSingleArray[6];
		worldMatrix[1][3] = worldMatrixSingleArray[7];
		worldMatrix[2][0] = worldMatrixSingleArray[8];
		worldMatrix[2][1] = worldMatrixSingleArray[9];
		worldMatrix[2][2] = worldMatrixSingleArray[10];
		worldMatrix[2][3] = worldMatrixSingleArray[11];
		worldMatrix[3][0] = worldMatrixSingleArray[12];
		worldMatrix[3][1] = worldMatrixSingleArray[13];
		worldMatrix[3][2] = worldMatrixSingleArray[14];
		worldMatrix[3][3] = worldMatrixSingleArray[15];

		my_trans = worldMatrix;
	}

	//Set default collision propagation type of "Use triangles"
	avObj->SetFlags(524302);

	//Set visibility
	MPlug vis = nodeFn.findPlug(MString("visibility"));
	bool value;
	vis.getValue(value);
	//out << "Visibility of " << nodeFn.name().asChar() << " is " << value << endl;
	if (value == false) {
		avObj->SetVisibility(false);
	}


	//out << "Copying Maya matrix to Niflib matrix" << endl;
	//Copy Maya matrix to Niflib matrix
	Matrix44 ni_trans(
		(float)my_trans[0][0], (float)my_trans[0][1], (float)my_trans[0][2], (float)my_trans[0][3],
		(float)my_trans[1][0], (float)my_trans[1][1], (float)my_trans[1][2], (float)my_trans[1][3],
		(float)my_trans[2][0], (float)my_trans[2][1], (float)my_trans[2][2], (float)my_trans[2][3],
		(float)my_trans[3][0], (float)my_trans[3][1], (float)my_trans[3][2], (float)my_trans[3][3]
	);

	//out << "Storing local transform values..." << endl;
	//Store Transform Values
	avObj->SetLocalTransform(ni_trans);

	// Service nodes (ProjectileNode, ShellCasingNode, SightingNode) must NOT have
	// a transform controller — adding one breaks weapon visibility while aiming.
	MString nodeNameStr = nodeFn.name();
	bool isServiceNode = (nodeNameStr == "ProjectileNode" ||
		nodeNameStr == "ShellCasingNode" ||
		nodeNameStr.indexW("SightingNode") != -1);

	if (!isServiceNode) {
		// For FNV: each node carries a NiTransformController. In plain geometry
		// export its interpolator is an empty sentinel (animation lives in a .kf).
		// In "nifanimation" mode, bake the node's anim curves straight into the
		// interpolator so the animation ships inside the .nif itself.
		NiTransformControllerRef controller = new NiTransformController();
		controller->SetFlags(76);
		controller->SetFrequency(1.0f);
		controller->SetPhase(0.0f);
		controller->SetStartTime(0.0f);
		controller->SetStopTime(0.0f);

		NiTransformInterpolatorRef interpolator;

		bool embedAnim = (this->translatorOptions->exportType == "nifanimation");
		if (embedAnim && this->animationExporter != NULL && MAnimUtil::isAnimated(dagNode)) {
			// Per-node key range -> controller Start/Stop Time, like vanilla FNV.
			float animStart = 0.0f;
			float animStop = 0.0f;
			interpolator = this->animationExporter->BuildTransformInterpolator(dagNode, &animStart, &animStop);
			if (interpolator != NULL) {
				controller->SetStartTime(animStart);
				controller->SetStopTime(animStop);
			}
		}

		if (interpolator == NULL) {
			// No animation (or geometry-only export): empty sentinel interpolator.
			// Gamebryo treats -FLT_MAX per channel as "not set".
			interpolator = new NiTransformInterpolator();
			const float NIF_NO_VALUE = -FLT_MAX; // -3.402823466e+38
			interpolator->SetTranslation(Vector3(NIF_NO_VALUE, NIF_NO_VALUE, NIF_NO_VALUE));
			interpolator->SetRotation(Quaternion(NIF_NO_VALUE, NIF_NO_VALUE, NIF_NO_VALUE, NIF_NO_VALUE));
			interpolator->SetScale(NIF_NO_VALUE);
		}

		controller->SetInterpolator(interpolator.operator->());
		avObj->AddController(StaticCast<NiTimeController>(controller));
	}
}

string NifNodeExporter::asString(bool verbose /*= false */) const {
	stringstream out;

	out << NifTranslatorFixtureItem::asString(verbose) << endl;
	out << "NifNodeExporter" << endl;

	return out.str();
}

const Type& NifNodeExporter::GetType() const {
	return TYPE;
}

const Type NifNodeExporter::TYPE("NifNodeExporter", &NifTranslatorFixtureItem::TYPE);