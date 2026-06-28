#ifndef _NIFCOLLISIONEXPORTER_H
#define _NIFCOLLISIONEXPORTER_H

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MEulerRotation.h>
#include <maya/MFileObject.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
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

#include <string>
#include <vector>
#include <sstream>

#include <niflib.h>
#include <obj/NiNode.h>
#include <obj/NiObject.h>
#include <obj/NiCollisionObject.h>
#include <obj/bhkCollisionObject.h>
#include <obj/bhkWorldObject.h>
#include <obj/bhkEntity.h>
#include <obj/bhkRigidBody.h>
#include <obj/bhkRigidBodyT.h>
#include <obj/bhkShape.h>
#include <obj/bhkConvexShape.h>
#include <obj/bhkSphereRepShape.h>
#include <obj/bhkBoxShape.h>
#include <obj/bhkSphereShape.h>
#include <obj/bhkCapsuleShape.h>
#include <obj/bhkConvexVerticesShape.h>
#include <obj/bhkPackedNiTriStripsShape.h>
#include <obj/bhkMoppBvTreeShape.h>
#include <obj/bhkListShape.h>
#include <obj/hkPackedNiTriStripsData.h>

#include "include/Common/NifTranslatorRefObject.h"
#include "include/Common/NifTranslatorOptions.h"
#include "include/Common/NifTranslatorData.h"
#include "include/Common/NifTranslatorUtils.h"
#include "include/Common/NifTranslatorFixtureItem.h"
#include "include/Custom Nodes/NifBhkRigidBody.h"
#include "include/Custom Nodes/NifBhkShape.h"
#include "include/Importers/NifCollisionImporter.h"

using namespace Niflib;
using namespace std;

class NifCollisionExporter;
typedef Ref<NifCollisionExporter> NifCollisionExporterRef;

// Reconstructs the bhkCollisionObject chain from a Maya transform back into
// niflib objects and attaches it to niNode:
//   NiNode -> bhkCollisionObject -> bhkRigidBody / bhkRigidBodyT -> bhkShape
// Symmetric counterpart to NifCollisionImporter - reads back what the
// importer wrote: a "bhkRigidBody"/"bhkRigidBodyT" named child transform
// holding a NifBhkRigidBody attribute node, with its own "bhkBoxShape"/
// "bhkSphereShape"/"bhkConvexVertices"/etc. named child transform (and that
// child's NifBhkShape attribute node, for shapes that have one) representing
// the collision geometry.
//
// IMPORTANT: shape reconstruction from mesh geometry (convexVertices, packed
// tri-strips) trusts the mesh AS-IS - it does not re-run convex hull
// validation or check that the mesh is actually convex. If the user edited
// the mesh in Maya in a way that broke convexity, the exported
// bhkConvexVerticesShape will silently contain a non-convex point set. This
// mirrors the same trust-the-scene assumption the rest of this translator
// makes for ordinary mesh export.
class NifCollisionExporter : public NifTranslatorFixtureItem {
public:
	NifCollisionExporter();
	NifCollisionExporter(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils);
	virtual ~NifCollisionExporter();

	// Entry point - called once per exported NiNode from NifNodeExporter.
	// dagNode is the Maya transform niNode was exported from; looks for a
	// "bhkRigidBody"/"bhkRigidBodyT" named child transform among dagNode's
	// children and, if found, reconstructs and attaches the collision chain
	// to niNode. Does nothing (returns success) if no such child exists -
	// most nodes have no collision, same as on import.
	virtual MStatus ExportCollision(MObject dagNode, NiNodeRef niNode);

	// Variant for when the collision transform itself is already known and
	// has no Maya parent to search through (e.g. a "bhkRigidBody"/
	// "bhkRigidBodyT" transform sitting with no parent at the very top
	// level of the scene - the importer creates these directly when
	// importRootNode is off, with no wrapper transform around them).
	// rigidBodyTransform is the collision transform itself, not something
	// to search the children of.
	virtual MStatus ExportCollisionDirect(MObject rigidBodyTransform, NiNodeRef niNode);

	virtual string asString(bool verbose = false) const;
	virtual const Type& GetType() const;
	const static Type TYPE;

protected:
	// Builds a bhkRigidBody/bhkRigidBodyT (chosen by isHavokT) from the
	// NifBhkRigidBody attribute node attached to rigidBodyTransform, and its
	// shape from rigidBodyTransform's named shape child (if any).
	bhkRigidBodyRef ExportRigidBody(MObject rigidBodyTransform);

	// Dispatches by the shape transform's name (set by the importer:
	// "bhkBoxShape", "bhkSphereShape", "bhkCapsuleShape"/"bhkCapsuleShapeMesh",
	// "bhkConvexVertices", "bhkPackedTriStrips", "bhkListShape"). Returns NULL
	// if shapeTransform's name doesn't match any known shape type.
	bhkShapeRef ExportShape(MObject shapeTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode);

	bhkShapeRef ExportBoxShape(MObject shapeTransform);
	bhkShapeRef ExportSphereShape(MObject shapeTransform);
	bhkShapeRef ExportCapsuleShape(MObject shapeTransform);
	bhkShapeRef ExportConvexVerticesShape(MObject shapeTransform);
	bhkShapeRef ExportPackedTriStripsShape(MObject shapeTransform);
	bhkShapeRef ExportListShape(MObject shapeTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode);

	// Reads the mesh under meshTransform (its direct MFn::kMesh child) back
	// into Havok-unit vertex/triangle arrays, undoing CreateMeshFromVertices's
	// import-time scaling (divides by HAVOK_TO_BETHESDA_SCALE * importScale).
	// Returns false if meshTransform has no mesh shape child.
	bool ReadMeshFromTransform(MObject meshTransform, vector<Vector3>& outVerts, vector<Triangle>& outTris);

	// Finds the bhkShapeAttrs node linked to shapeTransform via its
	// "bhkShapeAttrs" message attribute (set by AttachShapeAttrs on import).
	// Returns a null MObject if no such link/node exists.
	MObject FindShapeAttrsNode(MObject shapeTransform);

	// Finds a direct child of parent whose name matches one of the known
	// collision shape transform names, returning a null MObject if none match.
	MObject FindShapeChild(MObject parentTransform);
};

#endif