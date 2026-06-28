#ifndef _NIFCOLLISIONIMPORTER_H
#define _NIFCOLLISIONIMPORTER_H

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

using namespace Niflib;
using namespace std;

class NifCollisionImporter;
typedef Ref<NifCollisionImporter> NifCollisionImporterRef;

// Imports the bhkCollisionObject chain hanging off a NiNode:
//   NiNode -> bhkCollisionObject -> bhkRigidBody / bhkRigidBodyT -> bhkShape
// bhkRigidBodyT has no extra fields over bhkRigidBody - same Translation/Rotation
// fields exist on the base class - but the engine only applies them when the
// runtime type is bhkRigidBodyT specifically, so we track that as isHavokT.
// Creates a child transform under the node's Maya transform holding a
// NifBhkRigidBody attribute node (physics props) plus a child mesh
// representing the collision shape geometry.
class NifCollisionImporter : public NifTranslatorFixtureItem {
public:
	NifCollisionImporter();
	NifCollisionImporter(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils);
	virtual ~NifCollisionImporter();

	// Entry point - called once per NiNode from NifNodeImporter::ImportNodes.
	// parentTransform is the transform already created for niNode, or
	// MObject::kNullObj for root-level collision when there's no real Maya
	// transform to attach to (importRootNode off) - bhkRigidBody is then
	// created directly under the Maya scene root instead of under a
	// dedicated wrapper transform, since MFnTransform::create() accepts
	// kNullObj natively as "no parent, create at scene root".
	virtual MStatus ImportCollision(NiNodeRef niNode, MObject parentTransform);

	virtual string asString(bool verbose = false) const;
	virtual const Type& GetType() const;
	const static Type TYPE;

	// Incremental 3D convex hull (Vector3 points -> Triangle faces, CCW winding,
	// outward-facing normals). Pure geometry helper, no Maya/niflib dependency
	// beyond the Vector3/Triangle types already used elsewhere in this class.
	// Returns an empty vector if the input is degenerate (fewer than 4 points,
	// or all points coplanar/collinear/coincident).
	// Public (not protected, despite being an internal implementation detail
	// for this class) so NifCollisionExporter can reuse the exact same
	// algorithm when reconstructing bhkConvexVerticesShape half-spaces on
	// export - keeps a single source of truth instead of two copies that
	// could drift apart.
	static vector<Triangle> ComputeConvexHull(const vector<Vector3>& points);

protected:
	// Builds the rigid body sub-transform + attribute node, fills in physics props
	MStatus ImportRigidBody(bhkRigidBodyRef rigidBody, MObject parentTransform, unsigned short bhkFlags);

	// Dispatches by concrete bhkShape subtype. layer/rigidBodyAttrsNode are
	// threaded through so the resulting geometry can be colored to match
	// NifSkope's per-layer collision colors via a live connection to
	// rigidBodyAttrsNode's layerColor output (see ApplyWireframeDisplayOverride) -
	// neither has any effect on geometry itself.
	// outShapeNode receives the created shape transform/mesh (or stays null on
	// failure) so the caller can connect it to NifBhkRigidBody::targetShape.
	MStatus ImportShape(bhkShapeRef shape, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode);

	MStatus ImportBoxShape(Ref<bhkBoxShape> box, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode);
	MStatus ImportSphereShape(Ref<bhkSphereShape> sphere, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode);
	MStatus ImportCapsuleShape(Ref<bhkCapsuleShape> capsule, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode);
	MStatus ImportConvexVerticesShape(Ref<bhkConvexVerticesShape> cv, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode);
	MStatus ImportPackedTriStripsShape(Ref<bhkPackedNiTriStripsShape> packed, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode);
	MStatus ImportMoppBvTreeShape(Ref<bhkMoppBvTreeShape> mopp, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode);
	MStatus ImportListShape(Ref<bhkListShape> list, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode);

	// Creates a Maya mesh from raw vertex/triangle arrays in Havok units,
	// converting to Bethesda/Maya units (x7) and applying import scale.
	// Colors the result via a live connection to rigidBodyAttrsNode's
	// layerColor (see ApplyWireframeDisplayOverride).
	MObject CreateMeshFromVertices(const vector<Vector4>& verts, const vector<Triangle>& tris, MObject parent, const char* name, SkyrimLayer layer, MObject rigidBodyAttrsNode);
	MObject CreateMeshFromVertices(const vector<Vector3>& verts, const vector<Triangle>& tris, MObject parent, const char* name, SkyrimLayer layer, MObject rigidBodyAttrsNode);

	// Generates a capsule mesh (cylinder shaft + hemisphere cap at each end)
	// between p1 and p2 with the given radius, in the same Havok-unit space
	// the rest of this importer works in (scaling to Bethesda units happens
	// later in CreateMeshFromVertices, same as every other shape). Leaves
	// verts/tris empty if p1==p2 (degenerate axis, can't build an orientation
	// frame) - caller is expected to handle that case (e.g. fall back to a
	// sphere, or just leave the transform empty).
	static void BuildCapsuleMesh(const Vector3& p1, const Vector3& p2, float radius, vector<Vector3>& outVerts, vector<Triangle>& outTris);

	// Sets the DAG display override on a transform so it draws as wireframe
	// only (no shaded surface at all, no material needed) - this is how every
	// piece of collision geometry shows up in the viewport: visible outline,
	// see-through interior, regardless of the active shading mode.
	// Connects overrideColorRGB to rigidBodyAttrsNode's live layerColor output
	// (see NifBhkRigidBody::compute()) so the color stays in sync if layer is
	// edited after import; falls back to a static fallbackR/G/B value only if
	// rigidBodyAttrsNode is null.
	void ApplyWireframeDisplayOverride(MObject transform, MObject rigidBodyAttrsNode, float fallbackR = 0.3f, float fallbackG = 1.0f, float fallbackB = 0.3f);

	// Creates a NifBhkShape attribute node attached to shapeTransform and
	// fills it from the shape's Material/Radius. Only meaningful for shapes
	// deriving from bhkSphereRepShape (box/sphere/capsule/convexVertices);
	// callers should check IsDerivedType before calling this.
	void AttachShapeAttrs(Ref<bhkSphereRepShape> sphereRepShape, MObject shapeTransform);
};

#endif