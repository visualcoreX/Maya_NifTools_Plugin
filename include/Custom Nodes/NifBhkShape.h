#ifndef _NIFBHKSHAPE_H
#define _NIFBHKSHAPE_H

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
#include <maya/MItMeshVertex.h>
#include <maya/MPxNode.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>

#include <gen/enums.h>

using namespace Niflib;

// Stores bhkSphereRepShape properties (Material, Radius) - these belong to
// the collision shape itself (bhkBoxShape/bhkSphereShape/bhkCapsuleShape/
// bhkConvexVerticesShape all derive from bhkSphereRepShape), not to the
// bhkRigidBody. Attached to the shape's own transform so a bhkListShape with
// several sub-shapes gets one of these per sub-shape, each with its own
// material/radius - putting these fields on the shared rigid body attrs node
// would have made that impossible.
// Mirrors the NifBhkRigidBody/NifDismemberPartition pattern: pure attribute
// container, compute() does nothing, all real work happens in the importer/exporter.
class NifBhkShape : public MPxNode {
public:
	NifBhkShape();
	virtual ~NifBhkShape();

	virtual MStatus compute(const MPlug& plug, MDataBlock& dataBlock);
	static void* creator();
	static MStatus initialize();

public:
	// HavokMaterial: underlying value matches niflib's HavokMaterial enum for
	// 0-31, but the dropdown labels shown are Fallout3HavokMaterial names
	// (FO_HAV_MAT_*) from nif.xml, matching what NifSkope displays for FNV.
	// The full range is 0-127 (32 base materials x4 suffix groups: none,
	// _PLATFORM, _STAIRS, _STAIRS_PLATFORM) - Fallout3HavokMaterial isn't a
	// real C++ type in this niflib build, so values 32-127 use raw integers
	// with no matching named constant. Values 0-13 mean the same thing in
	// HavokMaterial and Fallout3HavokMaterial; 14-31 diverge completely
	// (confirmed via a NifSkope screenshot: value 26 displays as
	// FO_HAV_MAT_PISTOL, not HAV_MAT_HEAVY_METAL_STAIRS). See
	// NifBhkShape.cpp::initialize() for the full mapping.
	// NOTE: only meaningful for shapes deriving from bhkSphereRepShape (box,
	// sphere, capsule, convexVertices) - other shape types (e.g.
	// bhkPackedNiTriStripsShape) store material per sub-shape instead and
	// don't use this node at all.
	// FNV reads GetMaterial() (not GetSkyrimMaterial()) since it exports with
	// userVersion=11 - see bhkSphereRepShape.cpp::Read() for the version check
	// (userVersion < 12 -> material, userVersion >= 12 -> skyrimMaterial).
	static MObject material;

	// True if this shape is a sub-shape of a bhkListShape whose own Material
	// field overrode this shape's individual material (see geckwiki's
	// bhkListShape documentation - "Overrides the impact material of all sub
	// shapes... Footstep sounds use the sub shape material sound"). Purely
	// informational, doesn't affect anything by itself - just makes it clear
	// in the Attribute Editor that the displayed material value isn't what
	// this specific shape originally stored.
	static MObject materialOverriddenByList;

	// Radius of the bounding sphere, stored as the raw Havok-unit value -
	// confirmed unscaled against a NifSkope screenshot (file had 0.1,
	// NifSkope displayed 0.1). This is an internal Havok value used for quick
	// collision rejection, not a visual geometry dimension.
	static MObject radius;

	static MTypeId id;
};

#endif