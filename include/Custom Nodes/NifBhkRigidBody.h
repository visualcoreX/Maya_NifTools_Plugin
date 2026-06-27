#ifndef _NIFBHKRIGIDBODY_H
#define _NIFBHKRIGIDBODY_H

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MEulerRotation.h>
#include <maya/MFileObject.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnBase.h>
#include <maya/MFnComponent.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPhongShader.h>
#include <maya/MFnSet.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MFStream.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MIOStream.h>
#include <maya/MItDag.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPointArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MVector.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MAnimUtil.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MPxNode.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnCompoundAttribute.h>

#include <gen/enums.h>

using namespace Niflib;

// Stores all bhkRigidBody physics properties so they can be round-tripped on
// export without guessing defaults. Holds no geometry itself; targetShape just
// points at the Maya mesh that represents the collision shape.
// Note: per niflib's bhkRigidBody.h, Translation/Rotation are fields on
// bhkRigidBody itself - isHavokT only records whether the original NIF
// object's runtime type was bhkRigidBodyT, since the engine only applies
// those fields when the "T" variant is used.
// Unlike NifDismemberPartition, compute() does real work here: layerColor is
// a live output driven by layer, so the collision shape's wireframe color
// stays in sync if the user edits layer after import (see LayerToColor and
// NifCollisionImporter::ImportRigidBody, which connects layerColor to the
// shape transform's overrideColorRGB instead of writing a static value).
class NifBhkRigidBody : public MPxNode {
public:
	NifBhkRigidBody();
	virtual ~NifBhkRigidBody();

	virtual MStatus compute(const MPlug& plug, MDataBlock& dataBlock);
	static void* creator();
	static MStatus initialize();

public:
	// Enum <-> string helpers, same idea as NifDismemberPartition's flag converters,
	// so values stay human-readable in the Attribute Editor instead of raw numbers.
	// IMPORTANT: the underlying C++ type is SkyrimLayer (see note below on why),
	// but the strings shown/accepted here use Fallout3Layer names (FOL_*) to
	// match what NifSkope displays for FNV files and to keep round-tripping on
	// export unambiguous - SkyrimLayer and Fallout3Layer share the same numeric
	// values 0-18, and FNV never uses anything above 18 in practice, so this is
	// a safe display choice. Values 19+ fall back through SkyrimLayer's naming
	// since Fallout3Layer doesn't define anything past FOL_NULL=43 - in
	// practice these never appear in real FNV content.
	// NOTE: uses SkyrimLayer, not OblivionLayer. Confirmed via niflib source
	// (bhkWorldObject.cpp::Read): for info.version >= VER_20_2_0_7 (which includes
	// every FNV file, version 20.2.0.7), niflib reads the layer byte into the
	// skyrimLayer field, not the legacy layer (OblivionLayer) field - the latter
	// stays at its unread default (OL_STATIC) for every Skyrim/FNV file.
	static MString layerToString(SkyrimLayer layer_value);
	static SkyrimLayer stringToLayer(const MString& str);

	// Maps a SkyrimLayer to an RGB color matching NifSkope's documented
	// per-layer collision wireframe colors (values 0-18 are numerically
	// identical between SkyrimLayer/Fallout3Layer/OblivionLayer, e.g.
	// "STATIC (red)", "BIPED (green)", "WEAPON (orange)"). Single source of
	// truth used both by compute() below (for the live layerColor output)
	// and by NifCollisionImporter's static shape-coloring code (box/sphere/
	// mesh wireframe overrides applied once at import time, since that
	// geometry doesn't exist before import to connect anything to).
	static void LayerToColor(SkyrimLayer layer, float& r, float& g, float& b);

	// Strings match NifSkope's display names (SPHERE_INERTIA/SPHERE_STABILIZED/
	// BOX_INERTIA for values 2/3/4), not niflib's own enum constant names
	// (SPHERE/SPHERE_INERTIA/BOX) - confirmed against a NifSkope dropdown
	// screenshot. The numeric values written to/read from the file are
	// identical either way; only the C++ identifier differs from the display
	// name niflib happened to pick.
	static MString motionSystemToString(MotionSystem ms);
	static MotionSystem stringToMotionSystem(const MString& str);

	// niflib's GetQualityType()/SetQualityType() actually use MotionQuality
	static MString qualityTypeToString(MotionQuality qt);
	static MotionQuality stringToQualityType(const MString& str);

	static MString responseTypeToString(hkResponseType rt);
	static hkResponseType stringToResponseType(const MString& str);

	static MString deactivatorTypeToString(DeactivatorType dt);
	static DeactivatorType stringToDeactivatorType(const MString& str);

	static MString solverDeactivationToString(SolverDeactivation sd);
	static SolverDeactivation stringToSolverDeactivation(const MString& str);

	// Packs/unpacks the 12 individual bhkFlags booleans below into/from the
	// raw bhkCollisionObject::flags unsigned short, for reading from and
	// writing back to niflib. Bit order confirmed against a NifSkope bhkCOFlags
	// screenshot: 0=ACTIVE, 1=RESET_TRANS, 2=NOTIFY, 3=SET_LOCAL, 4=DBG_DISPLAY,
	// 5=USE_VEL, 6=RESET, 7=SYNC_ON_UPDATE, 8=BLEND_POS, 9=ALWAYS_BLEND,
	// 10=ANIM_TARGETED, 11=DISMEMBERED_LIMB.
	static unsigned short PackBhkFlags(MObject attrNode);
	static void UnpackBhkFlags(MObject attrNode, unsigned short flags);

public:
	// Link to the Maya mesh that holds the collision shape geometry (message attr, no data)
	static MObject targetShape;

	// bhkCollisionObject::flags, shown as individual checkboxes. The
	// "Bhk Flags" grouping and 2-column layout in the Attribute Editor is
	// handled by an AETemplate MEL proc registered at plugin load time (see
	// NifTranslator.cpp::initializePlugin), not by the attribute schema here.
	// See PackBhkFlags/UnpackBhkFlags for the bit <-> attribute mapping.
	static MObject bhkFlagActive;
	static MObject bhkFlagResetTrans;
	static MObject bhkFlagNotify;
	static MObject bhkFlagSetLocal;
	static MObject bhkFlagDbgDisplay;
	static MObject bhkFlagUseVel;
	static MObject bhkFlagReset;
	static MObject bhkFlagSyncOnUpdate;
	static MObject bhkFlagBlendPos;
	static MObject bhkFlagAlwaysBlend;
	static MObject bhkFlagAnimTargeted;
	static MObject bhkFlagDismemberedLimb;

	// True if this body's runtime niflib type was bhkRigidBodyT (translation/rotation active)
	static MObject isHavokT;

	// bhkWorldObject field (inherited by bhkRigidBody via bhkEntity)
	static MObject layer;

	// Output-only float3, live-computed from layer via LayerToColor (see
	// compute() in the .cpp). Connected to a collision shape transform's
	// overrideColorRGB on import so the wireframe color stays in sync if
	// layer is edited afterward, instead of only reflecting whatever layer
	// value was set at import time.
	static MObject layerColor;
	static MObject layerColorR;
	static MObject layerColorG;
	static MObject layerColorB;

	// bhkRigidBody fields
	static MObject motionSystem;
	static MObject qualityType;
	static MObject deactivatorType;
	static MObject solverDeactivation;
	static MObject collisionResponse;
	static MObject mass;
	static MObject friction;
	static MObject restitution;
	static MObject linearDamping;
	static MObject angularDamping;
	static MObject maxLinearVelocity;
	static MObject maxAngularVelocity;
	static MObject penetrationDepth;

	// bhkRigidBody::translation / rotation (Vector4 / QuaternionXYZW in niflib).
	// Stored here as 3 floats + 1 float since MFnNumericAttribute compounds
	// only support up to 3 children per create() call.
	static MObject havokTranslation; // float3 (x,y,z) - w component of Vector4 is always 0, not stored
	static MObject havokRotation;    // float3 (x,y,z of the quaternion)
	static MObject havokRotationW;   // float (w of the quaternion)

	static MTypeId id;
};

#endif