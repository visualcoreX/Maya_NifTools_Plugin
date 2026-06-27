#include "include/Importers/NifNodeImporter.h"

NifNodeImporter::NifNodeImporter() {

}

NifNodeImporter::NifNodeImporter(NifTranslatorOptionsRef translatorOptions,NifTranslatorDataRef translatorData,NifTranslatorUtilsRef translatorUtils) 
	: NifTranslatorFixtureItem(translatorOptions,translatorData,translatorUtils) {

}

void NifNodeImporter::ImportNodes( NiAVObjectRef niAVObj, map< NiAVObjectRef, MDagPath > & objs, MObject parent )
{
	MObject obj;
	const float scale = this->translatorOptions->importScale;

	//out << "Importing " << niAVObj << endl;

	////Stop at a non-node
	//if ( node == NULL )
	//	return;

	//Check if this node is a skinned NiTriBasedGeom.  If so, parent it to the scene instead
	//of whereever the NIF file has it.

	bool is_skin = false;

	vector<NiAVObjectRef> nodesToTest;

	if ( niAVObj->IsDerivedType( NiNode::TYPE ) ) {
		NiNodeRef nnr = DynamicCast<NiNode>(niAVObj);
		if ( nnr->IsSplitMeshProxy() ) {
			//Test all children
			nodesToTest = nnr->GetChildren();
		}
	} else if ( niAVObj->IsDerivedType(NiTriBasedGeom::TYPE ) ) {
		//This is a shape, so test it.
		nodesToTest.push_back(niAVObj);
	} else if ( niAVObj->IsDerivedType(BSShape::TYPE ) ) {
		//This is a BSTriShape/BSShape, so test it.
		nodesToTest.push_back(niAVObj);
	}

	for ( size_t i = 0; i < nodesToTest.size(); ++i ) {
		NiTriBasedGeomRef niTriGeom = DynamicCast<NiTriBasedGeom>( nodesToTest[i] );
		if ( niTriGeom && niTriGeom->IsSkin() ) {
			is_skin = true;
			break;
		}
	}

	if ( is_skin ) {
		parent = MObject::kNullObj;
	}

	//This must be a node, so process its basic attributes	
	MFnTransform transFn;
	MString name = this->translatorUtils->MakeMayaName( niAVObj->GetName() );
	string strName = name.asChar();

	int flags = niAVObj->GetFlags();


	NiNodeRef niNode = DynamicCast<NiNode>(niAVObj);

	MStringArray joint_matches;

	MString joint_match_mstring(this->translatorOptions->jointMatch.c_str());
	joint_match_mstring.split(';', joint_matches);

	//Determine whether this node is an IK joint
	bool is_joint = false;
	if ( niNode != NULL) {
		for (int c = 0; c <  joint_matches.length(); c++)
		{
			if (this->translatorOptions->jointMatch != "" && strName.find(joint_matches[c].asChar()) != string::npos) {
				is_joint = true;
			}
		}

		if ( this->translatorOptions->jointMatch != "" && strName.find(this->translatorOptions->jointMatch) != string::npos ) {
			is_joint = true;
		} else if ( niNode->IsSkinInfluence() == true ) {
			is_joint = true;
		}

		if (this->translatorOptions->importAllNodesAsJoints == true) {
			is_joint = true;
		}
	}

	//Check if the user wants us to try to combine new skins with an
	//existing skeleton
	if ( this->translatorOptions->importCombineSkeletons ) {
		//Check if joint already exits in scene.  If so, use it.
		if ( is_joint ) {
			obj = this->translatorUtils->GetExistingJoint( name.asChar() );
		}
	}

	if ( obj.isNull() == false ) {
		//A matching object was found
		transFn.setObject( obj );
	} else {
		if ( is_joint ) {
			// This is a bone, create an IK joint parented to the given parent
			MFnIkJoint IKjointFn;
			obj = IKjointFn.create( parent );

			//Set Transform Fn to work on the new IK joint
			transFn.setObject( obj );
		}
		else {
			//Not a bone, create a transform node parented to the given parent
			obj = transFn.create( parent );
		}

		//--Set the Transform Matrix--//
		Matrix44 transform;

		if ( is_skin ) {
			transform = niAVObj->GetWorldTransform();
		} else {
			transform = niAVObj->GetLocalTransform();
		}

		//put matrix into a float array
		float trans_arr[4][4];
		transform.AsFloatArr( trans_arr );

		trans_arr[3][0] *= scale;
		trans_arr[3][1] *= scale;
		trans_arr[3][2] *= scale;
		transFn.set( MTransformationMatrix(MMatrix(trans_arr)) );
		transFn.setRotationOrder( MTransformationMatrix::kXYZ, false );

		//Set name
		MFnDependencyNode dnFn;
		dnFn.setObject(obj);
		dnFn.setName( name );	
	}

	//Add the newly created object to the objects list
	MDagPath path;
	transFn.getPath( path );
	objs[niAVObj] = path;

	//Check if this object has a bounding box
	if ( niAVObj->HasBoundingBox() ) {
		//Get bounding box info
		BoundingBox bb = niAVObj->GetBoundingBox();		

		//Create a transform node to move the bounding box around
		MFnTransform tranFn;
		tranFn.create( obj );
		tranFn.setName("BoundingBox");
		Vector3 scaledTranslation = bb.translation * scale;
		Matrix44 bbTrans( scaledTranslation, bb.rotation, 1.0f);
		//out << "bbTrans:" << endl << bbTrans << endl;
		//out << "Translation:  " << bb.translation << endl;
		//out << "Rotation:  " << bb.rotation << endl;

		//put matrix into a float array
		float bb_arr[4][4];
		bbTrans.AsFloatArr( bb_arr );
		MStatus stat = tranFn.set( MTransformationMatrix(MMatrix(bb_arr)) );
		if ( stat != MS::kSuccess ) {
			//out << stat.errorString().asChar() << endl;
			throw runtime_error("Failed to set bounding box transforms.");
		}
		tranFn.setRotationOrder( MTransformationMatrix::kXYZ, false );

		//Create an implicit box parented to the node we just created
		MFnDagNode dagFn;
		dagFn.create( "implicitBox", tranFn.object() );
		dagFn.findPlug("sizeX").setValue( bb.radius.x * scale );
		dagFn.findPlug("sizeY").setValue( bb.radius.y * scale );
		dagFn.findPlug("sizeZ").setValue( bb.radius.z * scale );

	}

	//Check if this node has a Havok collision object attached
	if (niNode != NULL) {
		MGlobal::displayInfo(MString("NifNodeImporter: niNode is valid for \"") + name + "\", checking collisionImporter...");
		if (this->collisionImporter == NULL) {
			MGlobal::displayWarning("NifNodeImporter: collisionImporter is NULL - check NifDefaultImportingFixture constructor wiring and NifNodeImporter.h field declaration.");
		}
		else {
			MDagPath objPath;
			MFnDagNode objDagFn(obj);
			objDagFn.getPath(objPath);
			this->collisionImporter->ImportCollision(niNode, objPath);
		}
	}
	else {
		MGlobal::displayInfo(MString("NifNodeImporter: \"") + name + "\" is not a NiNode (niNode == NULL), skipping collision check.");
	}

	//Check to see if this is a mesh
	if ( niAVObj->IsDerivedType( NiTriBasedGeom::TYPE ) || niAVObj->IsDerivedType( BSShape::TYPE ) ) {
		//This is a mesh, so add it to the mesh list
		this->translatorData->importedMeshes.push_back( pair<NiAVObjectRef,MObject>(niAVObj,obj) );
	}

	if (niNode != NULL) {
		//Call this function for any children
		vector<NiAVObjectRef> children = niNode->GetChildren();
		for (unsigned int i = 0; i < children.size(); ++i) {
			this->ImportNodes(children[i], objs, obj);
		}
	}
}

NifNodeImporter::~NifNodeImporter() {

}

std::string NifNodeImporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose);
	out<<"NifNodeImporter"<<endl;

	return out.str();
}

const Type& NifNodeImporter::GetType() const {
	return TYPE;
}

const Type NifNodeImporter::TYPE("NifTranslatorNodeImporter",&NifTranslatorFixtureItem::TYPE);

