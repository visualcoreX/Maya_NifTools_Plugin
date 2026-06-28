#include "include/Importers/NifCollisionImporter.h"
#include "include/Custom Nodes/NifBhkRigidBody.h"
#include "include/Custom Nodes/NifBhkShape.h"

#include <maya/MFnTransform.h>
#include <maya/MFnMesh.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MDGModifier.h>
#include <algorithm>
#include <utility>
#include <cmath>

// Conversion factor: 1 Havok unit = 7 Bethesda/NIF units. All collision shape
// geometry and bhkRigidBody translation are stored in Havok units in the NIF,
// so everything here must be multiplied by this before going into Maya, and
// divided by it again on export.
static const float HAVOK_TO_BETHESDA_SCALE = 7.0f;

NifCollisionImporter::NifCollisionImporter() {
}

NifCollisionImporter::NifCollisionImporter(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils)
	: NifTranslatorFixtureItem(translatorOptions, translatorData, translatorUtils) {
}

NifCollisionImporter::~NifCollisionImporter() {
}

MStatus NifCollisionImporter::ImportCollision(NiNodeRef niNode, MObject parentTransform) {
	// Skip entirely if the user disabled collision import (see NifTranslatorOptions)
	if (this->translatorOptions->importCollision == false) {
		MGlobal::displayInfo("NifCollisionImporter: importCollision option is OFF, skipping.");
		return MStatus::kSuccess;
	}

	MGlobal::displayInfo(MString("NifCollisionImporter: checking node \"") + niNode->GetName().c_str() + "\" for collision object");

	NiCollisionObjectRef collisionObj = niNode->GetCollisionObject();
	if (collisionObj == NULL) {
		MGlobal::displayInfo("NifCollisionImporter: no NiCollisionObject on this node.");
		return MStatus::kSuccess; // most nodes have no collision, this is normal
	}

	MGlobal::displayInfo(MString("NifCollisionImporter: found NiCollisionObject, type=\"") + collisionObj->GetType().GetTypeName().c_str() + "\"");

	bhkCollisionObjectRef bhkCollision = DynamicCast<bhkCollisionObject>(collisionObj);
	if (bhkCollision == NULL) {
		// Could be a bare bhkNiCollisionObject or other non-Havok collision type.
		// Not handled in this first pass.
		MGlobal::displayWarning(MString("NifCollisionImporter: collision object is not a bhkCollisionObject (actual type: ") + collisionObj->GetType().GetTypeName().c_str() + "), skipping.");
		return MStatus::kSuccess;
	}

	NiObjectRef bodyObj = bhkCollision->GetBody();
	if (bodyObj == NULL) {
		MGlobal::displayWarning("NifCollisionImporter: bhkCollisionObject has no body (GetBody() returned NULL), skipping.");
		return MStatus::kSuccess;
	}

	MGlobal::displayInfo(MString("NifCollisionImporter: body type=\"") + bodyObj->GetType().GetTypeName().c_str() + "\"");

	bhkWorldObjectRef worldObj = DynamicCast<bhkWorldObject>(bodyObj);
	bhkRigidBodyRef rigidBody = DynamicCast<bhkRigidBody>(worldObj);
	if (rigidBody == NULL) {
		MGlobal::displayWarning(MString("NifCollisionImporter: body is not a bhkRigidBody/bhkRigidBodyT (actual type: ") + bodyObj->GetType().GetTypeName().c_str() + "), skipping.");
		return MStatus::kSuccess; // unsupported body type (e.g. bhkConstraint chains)
	}

	MGlobal::displayInfo("NifCollisionImporter: rigid body found, importing...");

	unsigned short bhkFlagsValue = bhkCollision->GetFlags();

	return this->ImportRigidBody(rigidBody, parentTransform, bhkFlagsValue);
}

MStatus NifCollisionImporter::ImportRigidBody(bhkRigidBodyRef rigidBody, MObject parentTransform, unsigned short bhkFlagsValue) {
	MStatus status;

	// bhkRigidBodyT is a real niflib type (own TYPE constant, "bhkRigidBodyT"),
	// just with no extra fields beyond bhkRigidBody - the "T" only changes
	// whether the engine applies translation/rotation. Confirmed via niflib
	// source: const Type bhkRigidBodyT::TYPE("bhkRigidBodyT", &bhkRigidBody::TYPE).
	// Checked here, before naming the transform, so the transform name
	// actually reflects which one this is instead of always saying
	// "bhkRigidBody" even for bhkRigidBodyT objects.
	bool isHavokT = rigidBody->IsDerivedType(bhkRigidBodyT::TYPE);

	// Create a child transform to hold the physics attribute node + shape mesh,
	// same approach NifNodeImporter uses for the bounding box sub-transform.
	MFnTransform transFn;
	MObject rigidBodyTransform = transFn.create(parentTransform, &status);
	if (status != MStatus::kSuccess) {
		MGlobal::displayError("NifCollisionImporter: MFnTransform::create() failed for bhkRigidBody transform.");
		return status;
	}
	transFn.setName(isHavokT ? "bhkRigidBodyT" : "bhkRigidBody");
	MGlobal::displayInfo(MString("NifCollisionImporter: created ") + (isHavokT ? "bhkRigidBodyT" : "bhkRigidBody") + " transform.");

	// Attach the attribute node that stores all physics properties
	MFnDependencyNode depFn;
	MObject attrNodeObj = depFn.create(NifBhkRigidBody::id, "bhkRigidBodyAttrs", &status);
	if (status != MStatus::kSuccess) {
		MGlobal::displayError("NifCollisionImporter: MFnDependencyNode::create() failed for NifBhkRigidBody node - is it registered in initializePlugin?");
		return status;
	}
	depFn.setObject(attrNodeObj);
	MGlobal::displayInfo("NifCollisionImporter: created bhkRigidBodyAttrs node.");

	NifBhkRigidBody::UnpackBhkFlags(attrNodeObj, bhkFlagsValue);

	MPlug(attrNodeObj, NifBhkRigidBody::isHavokT).setValue(isHavokT);

	// bhkWorldObject field. NOTE: uses GetSkyrimLayer(), not GetLayer() -
	// confirmed via niflib source (bhkWorldObject.cpp::Read): for any file with
	// info.version >= VER_20_2_0_7 (every FNV file is version 20.2.0.7), niflib
	// reads the on-disk layer byte into the skyrimLayer field. The legacy
	// layer/OblivionLayer field is never touched for these files and stays at
	// its unread constructor default (OL_STATIC) - that mismatch was the root
	// cause of collision proxies always showing up as "static" colored/typed
	// regardless of their actual layer in the NIF.
	SkyrimLayer layerValue = rigidBody->GetSkyrimLayer();
	MPlug(attrNodeObj, NifBhkRigidBody::layer).setValue((short)layerValue);

	// bhkWorldObject::flagsAndPartNumber - the raw byte NifSkope shows as
	// the "Flags" component of "Havok Filter". Same version reasoning as
	// GetSkyrimLayer() above: bhkWorldObject::Read only populates this field
	// (vs. the legacy colFilter) for info.version >= VER_20_2_0_7, which
	// every FNV file satisfies - confirmed via the niflib fork patch adding
	// public GetFlagsAndPartNumber()/GetColFilter() accessors.
	Niflib::byte havokFilterFlagsValue = rigidBody->GetFlagsAndPartNumber();
	MPlug(attrNodeObj, NifBhkRigidBody::havokFilterFlags).setValue((short)havokFilterFlagsValue);

	// bhkRigidBody fields
	MPlug(attrNodeObj, NifBhkRigidBody::motionSystem).setValue((short)rigidBody->GetMotionSystem());
	MPlug(attrNodeObj, NifBhkRigidBody::qualityType).setValue((short)rigidBody->GetQualityType());
	MPlug(attrNodeObj, NifBhkRigidBody::deactivatorType).setValue((short)rigidBody->GetDeactivatorType());
	MPlug(attrNodeObj, NifBhkRigidBody::solverDeactivation).setValue((short)rigidBody->GetSolverDeactivation());
	// CONFIRMED via the actual bhkRigidBody.h header: collisionResponse_ is a
	// protected field with no public getter or setter at all (not even with a
	// trailing underscore - that was a wrong lead from a stale third-party
	// mirror of niflib). It genuinely cannot be read through the public API in
	// this niflib build. The dropdown defaults to RESPONSE_SIMPLE_CONTACT
	// (matching niflib's own constructor default for this field), and editing
	// it manually is the only way to set a non-default value before export.

	MPlug(attrNodeObj, NifBhkRigidBody::mass).setValue(rigidBody->GetMass());
	MPlug(attrNodeObj, NifBhkRigidBody::friction).setValue(rigidBody->GetFriction());
	MPlug(attrNodeObj, NifBhkRigidBody::restitution).setValue(rigidBody->GetRestitution());
	MPlug(attrNodeObj, NifBhkRigidBody::linearDamping).setValue(rigidBody->GetLinearDamping());
	MPlug(attrNodeObj, NifBhkRigidBody::angularDamping).setValue(rigidBody->GetAngularDamping());
	MPlug(attrNodeObj, NifBhkRigidBody::maxLinearVelocity).setValue(rigidBody->GetMaxLinearVelocity());
	MPlug(attrNodeObj, NifBhkRigidBody::maxAngularVelocity).setValue(rigidBody->GetMaxAngularVelocity());
	MPlug(attrNodeObj, NifBhkRigidBody::penetrationDepth).setValue(rigidBody->GetPenetrationDepth());

	// Translation/rotation: stored in Havok units, convert to Bethesda units for
	// display/edit consistency with the rest of the scene. Only really meaningful
	// when isHavokT is true, but we store it regardless so nothing is lost.
	Vector4 havokTrans = rigidBody->GetTranslation();
	const float scale = this->translatorOptions->importScale;
	MPlug(attrNodeObj, NifBhkRigidBody::havokTranslation).child(0).setValue(havokTrans.x * HAVOK_TO_BETHESDA_SCALE * scale);
	MPlug(attrNodeObj, NifBhkRigidBody::havokTranslation).child(1).setValue(havokTrans.y * HAVOK_TO_BETHESDA_SCALE * scale);
	MPlug(attrNodeObj, NifBhkRigidBody::havokTranslation).child(2).setValue(havokTrans.z * HAVOK_TO_BETHESDA_SCALE * scale);

	QuaternionXYZW havokRot = rigidBody->GetRotation();
	MPlug(attrNodeObj, NifBhkRigidBody::havokRotation).child(0).setValue(havokRot.x);
	MPlug(attrNodeObj, NifBhkRigidBody::havokRotation).child(1).setValue(havokRot.y);
	MPlug(attrNodeObj, NifBhkRigidBody::havokRotation).child(2).setValue(havokRot.z);
	MPlug(attrNodeObj, NifBhkRigidBody::havokRotationW).setValue(havokRot.w);

	// Center of mass - confirmed via bhkRigidBody.h: GetCenter() returns a
	// Vector4 (w unused). Same Havok-to-Bethesda unit scale as translation,
	// since it's a spatial position like translation, not a purely physical
	// quantity like mass/inertia.
	Vector4 havokCenter = rigidBody->GetCenter();
	MPlug(attrNodeObj, NifBhkRigidBody::center).child(0).setValue(havokCenter.x * HAVOK_TO_BETHESDA_SCALE * scale);
	MPlug(attrNodeObj, NifBhkRigidBody::center).child(1).setValue(havokCenter.y * HAVOK_TO_BETHESDA_SCALE * scale);
	MPlug(attrNodeObj, NifBhkRigidBody::center).child(2).setValue(havokCenter.z * HAVOK_TO_BETHESDA_SCALE * scale);

	// Inertia tensor - confirmed via bhkRigidBody.h/nif_math.h: GetInertia()
	// returns an InertiaMatrix (3 rows of 4 floats, m11-m34, m14/m24/m34
	// unused). Stored RAW/unscaled - this is a physical quantity (moment of
	// inertia), not a spatial position, so the Havok-to-Bethesda position
	// scale doesn't apply here. NOTE: this scaling choice (i.e. none) is not
	// independently verified against a real round-trip yet - if exported
	// values come back visibly wrong, this is the first place to check.
	InertiaMatrix havokInertia = rigidBody->GetInertia();
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM11).child(0).setValue(havokInertia[0][0]);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM11).child(1).setValue(havokInertia[0][1]);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM11).child(2).setValue(havokInertia[0][2]);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM21).child(0).setValue(havokInertia[1][0]);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM21).child(1).setValue(havokInertia[1][1]);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM21).child(2).setValue(havokInertia[1][2]);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM31).child(0).setValue(havokInertia[2][0]);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM31).child(1).setValue(havokInertia[2][1]);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM31).child(2).setValue(havokInertia[2][2]);

	// Apply the translation/rotation to the actual transform only when this is
	// a real bhkRigidBodyT - per niflib's own comment on the class ("the 'T'
	// suffix marks this body as active for translation and rotation, a normal
	// bhkRigidBody ignores those properties"), the engine never reads these
	// fields on a plain bhkRigidBody even though the data is technically
	// present. Moving the transform for a non-T body would visually offset
	// the collision proxy in Maya in a way the game itself would never do.
	if (isHavokT) {
		MFnTransform rigidBodyTransFn(rigidBodyTransform);

		MVector mayaTranslation(
			havokTrans.x * HAVOK_TO_BETHESDA_SCALE * scale,
			havokTrans.y * HAVOK_TO_BETHESDA_SCALE * scale,
			havokTrans.z * HAVOK_TO_BETHESDA_SCALE * scale);
		rigidBodyTransFn.setTranslation(mayaTranslation, MSpace::kTransform);

		// niflib's QuaternionXYZW already matches Maya's MQuaternion field
		// order (x, y, z, w) - no component reordering needed.
		MQuaternion mayaRotation(havokRot.x, havokRot.y, havokRot.z, havokRot.w);
		rigidBodyTransFn.setRotation(mayaRotation);

		MGlobal::displayInfo("NifCollisionImporter: applied bhkRigidBodyT translation/rotation to transform.");
	}

	// Connect the attribute node to the transform via a message attribute so
	// it shows up nicely when the transform is selected (matches the
	// targetShape/targetFaces message-attribute pattern from NifDismemberPartition).
	MFnDependencyNode transformDepFn(rigidBodyTransform);
	if (!transformDepFn.hasAttribute("bhkRigidBodyAttrs")) {
		MFnMessageAttribute msgAttrFn;
		MObject linkAttr = msgAttrFn.create("bhkRigidBodyAttrs", "bhkRBA");
		transformDepFn.addAttribute(linkAttr);
	}
	MPlug linkPlug = transformDepFn.findPlug("bhkRigidBodyAttrs", false, &status);
	MPlug messagePlug = MFnDependencyNode(attrNodeObj).findPlug("message", false);

	// MPlug has no connectTo() method - connections go through MDGModifier
	MDGModifier dgMod;
	dgMod.connect(messagePlug, linkPlug);
	dgMod.doIt();

	// Import the shape geometry as a child mesh under the same rigid body transform
	bhkShapeRef shape = rigidBody->GetShape();
	if (shape == NULL) {
		MGlobal::displayWarning("NifCollisionImporter: rigid body has no shape (GetShape() returned NULL) - transform created but empty.");
	}
	else {
		MGlobal::displayInfo(MString("NifCollisionImporter: shape type=\"") + shape->GetType().GetTypeName().c_str() + "\"");

		MObject shapeNode;
		MStatus shapeStatus = this->ImportShape(shape, rigidBodyTransform, layerValue, attrNodeObj, shapeNode);
		if (shapeStatus != MStatus::kSuccess) {
			MGlobal::displayWarning("NifCollisionImporter: failed to import collision shape geometry.");
		}
		else {
			MGlobal::displayInfo("NifCollisionImporter: shape geometry imported successfully.");

			// Connect the shape's message attribute to targetShape so the
			// physics attribute node has a real, navigable link to its geometry
			// (this is what the "targetShape"/"map" field in the Attribute
			// Editor was originally meant to show - it was never wired up before).
			if (shapeNode.isNull()) {
				MGlobal::displayWarning("NifCollisionImporter: shapeNode is null after ImportShape succeeded - outShapeNode was never set in the shape-specific function.");
			}
			else {
				MFnDependencyNode shapeDepFn(shapeNode);
				MGlobal::displayInfo(MString("NifCollisionImporter: connecting targetShape to node \"") + shapeDepFn.name() + "\" (type: " + shapeDepFn.typeName() + ")");

				MStatus findPlugStatus;
				MPlug shapeMessagePlug = shapeDepFn.findPlug("message", false, &findPlugStatus);
				if (findPlugStatus != MStatus::kSuccess || shapeMessagePlug.isNull()) {
					MGlobal::displayWarning("NifCollisionImporter: findPlug(\"message\") on shape node failed.");
				}
				else {
					MPlug targetShapePlug(attrNodeObj, NifBhkRigidBody::targetShape);
					if (targetShapePlug.isNull()) {
						MGlobal::displayWarning("NifCollisionImporter: targetShapePlug is null - NifBhkRigidBody::targetShape attribute may not be registered correctly.");
					}
					else {
						MDGModifier targetShapeDgMod;
						MStatus connectStatus = targetShapeDgMod.connect(shapeMessagePlug, targetShapePlug);
						if (connectStatus != MStatus::kSuccess) {
							MGlobal::displayWarning(MString("NifCollisionImporter: MDGModifier::connect() failed: ") + connectStatus.errorString());
						}
						else {
							MStatus doItStatus = targetShapeDgMod.doIt();
							if (doItStatus != MStatus::kSuccess) {
								MGlobal::displayWarning(MString("NifCollisionImporter: MDGModifier::doIt() failed: ") + doItStatus.errorString());
							}
							else {
								MGlobal::displayInfo("NifCollisionImporter: targetShape connected successfully.");
							}
						}
					}
				}
			}
		}
	}

	return MStatus::kSuccess;
}

// NOTE: LayerToColor moved to NifBhkRigidBody (see NifBhkRigidBody::LayerToColor)
// so it's a single source of truth shared with that node's live compute()
// output, rather than two copies that could drift apart.

MStatus NifCollisionImporter::ImportShape(bhkShapeRef shape, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode) {
	if (shape->IsDerivedType(bhkMoppBvTreeShape::TYPE)) {
		// MOPP is just an acceleration structure wrapping a real shape - unwrap it
		return this->ImportMoppBvTreeShape(DynamicCast<bhkMoppBvTreeShape>(shape), parentTransform, layer, rigidBodyAttrsNode, outShapeNode);
	}
	if (shape->IsDerivedType(bhkPackedNiTriStripsShape::TYPE)) {
		return this->ImportPackedTriStripsShape(DynamicCast<bhkPackedNiTriStripsShape>(shape), parentTransform, layer, rigidBodyAttrsNode, outShapeNode);
	}
	if (shape->IsDerivedType(bhkConvexVerticesShape::TYPE)) {
		return this->ImportConvexVerticesShape(DynamicCast<bhkConvexVerticesShape>(shape), parentTransform, layer, rigidBodyAttrsNode, outShapeNode);
	}
	if (shape->IsDerivedType(bhkBoxShape::TYPE)) {
		return this->ImportBoxShape(DynamicCast<bhkBoxShape>(shape), parentTransform, layer, rigidBodyAttrsNode, outShapeNode);
	}
	if (shape->IsDerivedType(bhkSphereShape::TYPE)) {
		return this->ImportSphereShape(DynamicCast<bhkSphereShape>(shape), parentTransform, layer, rigidBodyAttrsNode, outShapeNode);
	}
	if (shape->IsDerivedType(bhkCapsuleShape::TYPE)) {
		return this->ImportCapsuleShape(DynamicCast<bhkCapsuleShape>(shape), parentTransform, layer, rigidBodyAttrsNode, outShapeNode);
	}
	if (shape->IsDerivedType(bhkListShape::TYPE)) {
		return this->ImportListShape(DynamicCast<bhkListShape>(shape), parentTransform, layer, rigidBodyAttrsNode, outShapeNode);
	}

	MGlobal::displayWarning(MString("NifCollisionImporter: unsupported bhkShape type: ") + shape->GetType().GetTypeName().c_str());
	return MStatus::kFailure;
}

MStatus NifCollisionImporter::ImportMoppBvTreeShape(Ref<bhkMoppBvTreeShape> mopp, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode) {
	// MOPP code itself is just a bounding-volume acceleration structure for the
	// Havok engine - the actual collidable geometry is the wrapped shape.
	// We deliberately do not parse moppData/origin/scale here; they get
	// regenerated by Havok tooling, not hand-edited in Maya.
	bhkShapeRef innerShape = mopp->GetShape();
	if (innerShape == NULL) {
		return MStatus::kFailure;
	}
	return this->ImportShape(innerShape, parentTransform, layer, rigidBodyAttrsNode, outShapeNode);
}

MStatus NifCollisionImporter::ImportPackedTriStripsShape(Ref<bhkPackedNiTriStripsShape> packed, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode) {
	Ref<hkPackedNiTriStripsData> data = packed->GetData();
	if (data == NULL) {
		return MStatus::kFailure;
	}

	vector<Vector3> verts = data->GetVertices();
	vector<Triangle> tris = data->GetTriangles();

	// SubShapes carry per-piece layer/material - not surfaced as separate
	// Maya objects in this first pass, the whole shape becomes one mesh.
	// vector<OblivionSubShape> subShapes = data->GetSubShapes();

	MObject meshObj = this->CreateMeshFromVertices(verts, tris, parentTransform, "bhkPackedTriStrips", layer, rigidBodyAttrsNode);
	outShapeNode = meshObj;
	return meshObj.isNull() ? MStatus::kFailure : MStatus::kSuccess;
}

MStatus NifCollisionImporter::ImportConvexVerticesShape(Ref<bhkConvexVerticesShape> cv, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode) {
	vector<Vector3> verts = cv->GetVertices();

	this->AttachShapeAttrs(StaticCast<bhkSphereRepShape>(cv), parentTransform);

	// bhkConvexVerticesShape stores only vertices + separating-plane half-spaces,
	// no explicit triangle list (it's a convex hull description, not a mesh) -
	// so the actual faces have to be computed from the point set.
	vector<Triangle> hullTris = NifCollisionImporter::ComputeConvexHull(verts);

	if (hullTris.empty()) {
		MGlobal::displayWarning("NifCollisionImporter: bhkConvexVerticesShape hull computation failed (degenerate point set) - falling back to empty transform.");
		MFnTransform transFn;
		MObject fallbackTransform = transFn.create(parentTransform);
		transFn.setName("bhkConvexVertices");
		outShapeNode = fallbackTransform;
		return MStatus::kSuccess;
	}

	MObject meshObj = this->CreateMeshFromVertices(verts, hullTris, parentTransform, "bhkConvexVertices", layer, rigidBodyAttrsNode);
	outShapeNode = meshObj;
	return meshObj.isNull() ? MStatus::kFailure : MStatus::kSuccess;
}

// --- Incremental 3D convex hull --------------------------------------------
// Standard incremental algorithm: start with a tetrahedron from 4 non-coplanar
// points, then for each remaining point, find all faces it sees (is in front
// of), remove them, find the horizon (boundary edges between visible and
// non-visible faces), and create new faces connecting the point to the horizon.
//
// All faces are kept CCW when viewed from outside the hull, i.e. for a face
// (a,b,c) the normal (b-a)x(c-a) points away from the hull interior. This
// matches the winding Maya expects for outward-facing normals.

namespace {
	struct HullFace {
		int a, b, c;
	};

	Vector3 Sub(const Vector3& p, const Vector3& q) {
		return Vector3(p.x - q.x, p.y - q.y, p.z - q.z);
	}
	Vector3 Cross(const Vector3& u, const Vector3& v) {
		return Vector3(
			u.y * v.z - u.z * v.y,
			u.z * v.x - u.x * v.z,
			u.x * v.y - u.y * v.x);
	}
	float Dot(const Vector3& u, const Vector3& v) {
		return u.x * v.x + u.y * v.y + u.z * v.z;
	}
	float Length(const Vector3& v) {
		return sqrtf(Dot(v, v));
	}

	// Signed distance from point p to the plane through (a,b,c), positive on
	// the side the (b-a)x(c-a) normal points toward.
	float SignedDistanceToFace(const vector<Vector3>& pts, const HullFace& f, const Vector3& p) {
		Vector3 normal = Cross(Sub(pts[f.b], pts[f.a]), Sub(pts[f.c], pts[f.a]));
		return Dot(normal, Sub(p, pts[f.a]));
	}

	// Orients face so its outward normal points away from interiorPoint
	HullFace OrientOutward(const vector<Vector3>& pts, HullFace f, const Vector3& interiorPoint) {
		float d = SignedDistanceToFace(pts, f, interiorPoint);
		if (d > 0.0f) {
			// interior point is in front - flip winding so it ends up behind
			std::swap(f.b, f.c);
		}
		return f;
	}
}

vector<Triangle> NifCollisionImporter::ComputeConvexHull(const vector<Vector3>& points) {
	vector<Triangle> result;
	const size_t n = points.size();
	if (n < 4) {
		return result; // need at least a tetrahedron
	}

	// Tolerance scaled to the point set's extent, so this works regardless of
	// whether the shape is tiny (a button) or huge (a building collision mesh).
	float maxExtent = 0.0f;
	for (size_t i = 0; i < n; ++i) {
		maxExtent = std::max(maxExtent, std::max(fabsf(points[i].x), std::max(fabsf(points[i].y), fabsf(points[i].z))));
	}
	const float EPS = std::max(maxExtent, 1.0f) * 1e-5f;

	// --- Step 1: find 4 non-coplanar points to seed the initial tetrahedron ---
	int i0 = 0, i1 = -1, i2 = -1, i3 = -1;

	// furthest point from points[0] gives a non-degenerate first edge
	float bestDist = -1.0f;
	for (size_t i = 1; i < n; ++i) {
		float d = Length(Sub(points[i], points[i0]));
		if (d > bestDist) { bestDist = d; i1 = (int)i; }
	}
	if (i1 < 0 || bestDist < EPS) {
		return result; // all points coincident
	}

	// furthest point from the line (i0,i1) gives a non-collinear third point
	float bestArea = -1.0f;
	for (size_t i = 0; i < n; ++i) {
		if ((int)i == i0 || (int)i == i1) continue;
		float area = Length(Cross(Sub(points[i1], points[i0]), Sub(points[i], points[i0])));
		if (area > bestArea) { bestArea = area; i2 = (int)i; }
	}
	if (i2 < 0 || bestArea < EPS) {
		return result; // all points collinear
	}

	// furthest point from the plane (i0,i1,i2) gives a non-coplanar fourth point
	float bestVol = -1.0f;
	for (size_t i = 0; i < n; ++i) {
		if ((int)i == i0 || (int)i == i1 || (int)i == i2) continue;
		HullFace base = { i0, i1, i2 };
		float d = fabsf(SignedDistanceToFace(points, base, points[i]));
		if (d > bestVol) { bestVol = d; i3 = (int)i; }
	}
	if (i3 < 0 || bestVol < EPS) {
		return result; // all points coplanar
	}

	Vector3 centroid((points[i0].x + points[i1].x + points[i2].x + points[i3].x) / 4.0f,
		(points[i0].y + points[i1].y + points[i2].y + points[i3].y) / 4.0f,
		(points[i0].z + points[i1].z + points[i2].z + points[i3].z) / 4.0f);

	vector<HullFace> faces;
	faces.push_back(OrientOutward(points, HullFace{ i0, i1, i2 }, centroid));
	faces.push_back(OrientOutward(points, HullFace{ i0, i1, i3 }, centroid));
	faces.push_back(OrientOutward(points, HullFace{ i0, i2, i3 }, centroid));
	faces.push_back(OrientOutward(points, HullFace{ i1, i2, i3 }, centroid));

	vector<bool> used(n, false);
	used[i0] = used[i1] = used[i2] = used[i3] = true;

	// --- Step 2: incrementally add every remaining point ---
	for (size_t pi = 0; pi < n; ++pi) {
		if (used[pi]) continue;
		const Vector3& p = points[pi];

		// Find every face this point is in front of ("sees")
		vector<bool> visible(faces.size(), false);
		bool seesAny = false;
		for (size_t fi = 0; fi < faces.size(); ++fi) {
			if (SignedDistanceToFace(points, faces[fi], p) > EPS) {
				visible[fi] = true;
				seesAny = true;
			}
		}

		if (!seesAny) {
			continue; // point is inside (or on) the current hull, skip it
		}

		// Find horizon edges: edges of visible faces that are shared with a
		// non-visible face (these become the base of the new faces fanning to p)
		vector<std::pair<int, int>> horizon;
		for (size_t fi = 0; fi < faces.size(); ++fi) {
			if (!visible[fi]) continue;
			int edges[3][2] = { {faces[fi].a, faces[fi].b}, {faces[fi].b, faces[fi].c}, {faces[fi].c, faces[fi].a} };
			for (int e = 0; e < 3; ++e) {
				int ea = edges[e][0], eb = edges[e][1];
				// an edge is on the horizon if no other visible face shares it
				// (in the opposite direction, which is how a closed manifold hull works)
				bool sharedByVisible = false;
				for (size_t fj = 0; fj < faces.size(); ++fj) {
					if (fj == fi || !visible[fj]) continue;
					int oEdges[3][2] = { {faces[fj].a, faces[fj].b}, {faces[fj].b, faces[fj].c}, {faces[fj].c, faces[fj].a} };
					for (int oe = 0; oe < 3; ++oe) {
						if (oEdges[oe][0] == eb && oEdges[oe][1] == ea) {
							sharedByVisible = true;
							break;
						}
					}
					if (sharedByVisible) break;
				}
				if (!sharedByVisible) {
					horizon.push_back(std::make_pair(ea, eb));
				}
			}
		}

		// Remove visible faces, keep the rest
		vector<HullFace> newFaces;
		newFaces.reserve(faces.size() - 1 + horizon.size());
		for (size_t fi = 0; fi < faces.size(); ++fi) {
			if (!visible[fi]) {
				newFaces.push_back(faces[fi]);
			}
		}

		// Fan new faces from the point to each horizon edge, preserving winding
		// (horizon edges are already ordered so (ea,eb,p) gives outward winding)
		for (size_t hi = 0; hi < horizon.size(); ++hi) {
			newFaces.push_back(HullFace{ horizon[hi].first, horizon[hi].second, (int)pi });
		}

		faces = newFaces;
		used[pi] = true;
	}

	result.reserve(faces.size());
	for (size_t i = 0; i < faces.size(); ++i) {
		Triangle t;
		t.v1 = (unsigned short)faces[i].a;
		t.v2 = (unsigned short)faces[i].b;
		t.v3 = (unsigned short)faces[i].c;
		result.push_back(t);
	}
	return result;
}

// --- Capsule mesh generation -------------------------------------------------
// Builds a capsule as a cylindrical shaft between p1/p2 with a hemisphere cap
// at each end, all with the given radius. Uses a fixed ring resolution (12
// segments around, 6 latitude steps per hemisphere) - enough to look like a
// capsule in wireframe without generating an excessive vertex count for what
// is, after all, just a collision proxy.
void NifCollisionImporter::BuildCapsuleMesh(const Vector3& p1, const Vector3& p2, float radius, vector<Vector3>& outVerts, vector<Triangle>& outTris) {
	outVerts.clear();
	outTris.clear();

	Vector3 axis(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);
	float axisLength = sqrtf(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
	if (axisLength < 1e-6f) {
		// Degenerate axis (p1 == p2) - can't build an orientation frame, this
		// is really just a sphere at that point. Left to the caller to decide
		// what to do (e.g. fall back to ImportSphereShape's implicitSphere).
		return;
	}

	Vector3 axisDir(axis.x / axisLength, axis.y / axisLength, axis.z / axisLength);

	// Build an orthonormal frame (axisDir, side1, side2) around the axis.
	// Pick whichever world axis is least parallel to axisDir as a seed for
	// the cross product, to avoid a near-zero result when the capsule is
	// aligned close to that axis.
	Vector3 seed(0.0f, 0.0f, 1.0f);
	if (fabsf(axisDir.z) > 0.9f) {
		seed = Vector3(1.0f, 0.0f, 0.0f);
	}
	Vector3 side1(
		axisDir.y * seed.z - axisDir.z * seed.y,
		axisDir.z * seed.x - axisDir.x * seed.z,
		axisDir.x * seed.y - axisDir.y * seed.x);
	float side1Len = sqrtf(side1.x * side1.x + side1.y * side1.y + side1.z * side1.z);
	side1.x /= side1Len; side1.y /= side1Len; side1.z /= side1Len;
	Vector3 side2(
		axisDir.y * side1.z - axisDir.z * side1.y,
		axisDir.z * side1.x - axisDir.x * side1.z,
		axisDir.x * side1.y - axisDir.y * side1.x);

	const int segments = 12;     // around the circumference
	const int hemiRings = 6;     // latitude steps per hemisphere, pole excluded (added separately)
	const float kPi = 3.14159265358979323846f;

	// Helper: point on the capsule surface at axial position centerOffset
	// (measured along axisDir from p1 or p2), ring angle theta, and latitude
	// phi (0 = equator/ring level, kPi/2 = pole).
	auto ringPoint = [&](const Vector3& center, float phi, float theta) -> Vector3 {
		float ringRadius = radius * cosf(phi);
		float axialOffset = radius * sinf(phi);
		float cosT = cosf(theta), sinT = sinf(theta);
		return Vector3(
			center.x + axisDir.x * axialOffset + (side1.x * cosT + side2.x * sinT) * ringRadius,
			center.y + axisDir.y * axialOffset + (side1.y * cosT + side2.y * sinT) * ringRadius,
			center.z + axisDir.z * axialOffset + (side1.z * cosT + side2.z * sinT) * ringRadius);
		};

	vector<vector<int>> rings; // each entry is a list of vertex indices for one latitude ring, in winding order

	// Start pole (cap on p1's hemisphere, pointing away from p2)
	int startPoleIdx = (int)outVerts.size();
	outVerts.push_back(ringPoint(p1, -kPi * 0.5f, 0.0f)); // phi=-90 collapses to the pole regardless of theta

	// p1-side hemisphere rings, phi from just past the pole up to the equator
	for (int lat = 1; lat < hemiRings; ++lat) {
		float phi = -kPi * 0.5f + (kPi * 0.5f) * ((float)lat / (float)hemiRings);
		vector<int> ring;
		for (int seg = 0; seg < segments; ++seg) {
			float theta = 2.0f * kPi * (float)seg / (float)segments;
			ring.push_back((int)outVerts.size());
			outVerts.push_back(ringPoint(p1, phi, theta));
		}
		rings.push_back(ring);
	}

	// Shaft rings: the equator ring at p1, then the equator ring at p2
	// (cylindrical part is just these two rings connected by quads/triangles)
	{
		vector<int> ring;
		for (int seg = 0; seg < segments; ++seg) {
			float theta = 2.0f * kPi * (float)seg / (float)segments;
			ring.push_back((int)outVerts.size());
			outVerts.push_back(ringPoint(p1, 0.0f, theta));
		}
		rings.push_back(ring);
	}
	{
		vector<int> ring;
		for (int seg = 0; seg < segments; ++seg) {
			float theta = 2.0f * kPi * (float)seg / (float)segments;
			ring.push_back((int)outVerts.size());
			outVerts.push_back(ringPoint(p2, 0.0f, theta));
		}
		rings.push_back(ring);
	}

	// p2-side hemisphere rings, phi from equator up toward the pole
	for (int lat = 1; lat < hemiRings; ++lat) {
		float phi = (kPi * 0.5f) * ((float)lat / (float)hemiRings);
		vector<int> ring;
		for (int seg = 0; seg < segments; ++seg) {
			float theta = 2.0f * kPi * (float)seg / (float)segments;
			ring.push_back((int)outVerts.size());
			outVerts.push_back(ringPoint(p2, phi, theta));
		}
		rings.push_back(ring);
	}

	int endPoleIdx = (int)outVerts.size();
	outVerts.push_back(ringPoint(p2, kPi * 0.5f, 0.0f));

	// Stitch the start pole to the first ring (triangle fan)
	const vector<int>& firstRing = rings.front();
	for (int seg = 0; seg < segments; ++seg) {
		int next = (seg + 1) % segments;
		Triangle t;
		t.v1 = (unsigned short)startPoleIdx;
		t.v2 = (unsigned short)firstRing[seg];
		t.v3 = (unsigned short)firstRing[next];
		outTris.push_back(t);
	}

	// Stitch consecutive rings together (quads split into 2 triangles each)
	for (size_t r = 0; r + 1 < rings.size(); ++r) {
		const vector<int>& ringA = rings[r];
		const vector<int>& ringB = rings[r + 1];
		for (int seg = 0; seg < segments; ++seg) {
			int next = (seg + 1) % segments;
			Triangle t1;
			t1.v1 = (unsigned short)ringA[seg];
			t1.v2 = (unsigned short)ringB[seg];
			t1.v3 = (unsigned short)ringB[next];
			outTris.push_back(t1);

			Triangle t2;
			t2.v1 = (unsigned short)ringA[seg];
			t2.v2 = (unsigned short)ringB[next];
			t2.v3 = (unsigned short)ringA[next];
			outTris.push_back(t2);
		}
	}

	// Stitch the last ring to the end pole (triangle fan)
	const vector<int>& lastRing = rings.back();
	for (int seg = 0; seg < segments; ++seg) {
		int next = (seg + 1) % segments;
		Triangle t;
		t.v1 = (unsigned short)endPoleIdx;
		t.v2 = (unsigned short)lastRing[next];
		t.v3 = (unsigned short)lastRing[seg];
		outTris.push_back(t);
	}
}

MStatus NifCollisionImporter::ImportBoxShape(Ref<bhkBoxShape> box, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode) {
	Vector3 dims = box->GetDimensions(); // half-extents, in Havok units
	const float scale = this->translatorOptions->importScale;

	MFnTransform transFn;
	MObject boxTransform = transFn.create(parentTransform);
	transFn.setName("bhkBoxShape");

	// NOTE: x2 factor confirmed by direct visual comparison against NifSkope
	// (side-by-side screenshot showed the Maya box at half the correct size
	// until this was added). Maya's implicitBox.sizeX/Y/Z attributes are
	// themselves a half-extent (sizeX=1 spans -0.5 to +0.5), so niflib's
	// already-half-extent Dimensions need to be doubled first to get the
	// correct full-extent value implicitBox expects, before applying the
	// Havok-to-Bethesda unit conversion.
	MFnDagNode dagFn;
	MObject implicitBox = dagFn.create("implicitBox", boxTransform);
	dagFn.findPlug("sizeX").setValue(dims.x * 2.0f * HAVOK_TO_BETHESDA_SCALE * scale);
	dagFn.findPlug("sizeY").setValue(dims.y * 2.0f * HAVOK_TO_BETHESDA_SCALE * scale);
	dagFn.findPlug("sizeZ").setValue(dims.z * 2.0f * HAVOK_TO_BETHESDA_SCALE * scale);

	float r, g, b;
	NifBhkRigidBody::LayerToColor(layer, r, g, b);
	this->ApplyWireframeDisplayOverride(boxTransform, rigidBodyAttrsNode, r, g, b);

	this->AttachShapeAttrs(StaticCast<bhkSphereRepShape>(box), boxTransform);

	outShapeNode = boxTransform;
	return MStatus::kSuccess;
}

MStatus NifCollisionImporter::ImportSphereShape(Ref<bhkSphereShape> sphere, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode) {
	// bhkSphereShape's radius comes from the shared bhkConvexShape base (GetRadius).
	float radius = sphere->GetRadius();
	const float scale = this->translatorOptions->importScale;

	MFnTransform transFn;
	MObject sphereTransform = transFn.create(parentTransform);
	transFn.setName("bhkSphereShape");

	MFnDagNode dagFn;
	MObject implicitSphere = dagFn.create("implicitSphere", sphereTransform);
	MPlug implicitRadiusPlug = dagFn.findPlug("radius");
	// NOTE: no setValue() here - the multiplyDivide connection below drives
	// this plug instead, so a static value would just be immediately
	// overridden and is misleading to leave in.

	float r, g, b;
	NifBhkRigidBody::LayerToColor(layer, r, g, b);
	this->ApplyWireframeDisplayOverride(sphereTransform, rigidBodyAttrsNode, r, g, b);

	this->AttachShapeAttrs(StaticCast<bhkSphereRepShape>(sphere), sphereTransform);

	// Link the visual implicitSphere.radius (Bethesda units) to the stored
	// NifBhkShape.radius (raw Havok units) via a multiplyDivide node, so
	// editing either one keeps the other in sync through the same conversion
	// factor used on import - rather than leaving two independent numbers
	// that silently drift apart if someone tweaks the visible sphere size.
	MFnDependencyNode shapeAttrsDepFn(sphereTransform);
	MPlug shapeAttrsLinkPlug = shapeAttrsDepFn.findPlug("bhkShapeAttrs", false);
	MPlugArray shapeAttrsConnections;
	shapeAttrsLinkPlug.connectedTo(shapeAttrsConnections, true, false);
	if (shapeAttrsConnections.length() > 0) {
		MObject shapeAttrsNode = shapeAttrsConnections[0].node();

		MFnDependencyNode multDivFn;
		MObject multDivNode = multDivFn.create("multiplyDivide", "bhkRadiusConvert");
		multDivFn.findPlug("operation").setValue(1); // multiply
		multDivFn.findPlug("input2X").setValue(HAVOK_TO_BETHESDA_SCALE * scale);

		MDGModifier dgMod;
		dgMod.connect(MPlug(shapeAttrsNode, NifBhkShape::radius), multDivFn.findPlug("input1X"));
		dgMod.connect(multDivFn.findPlug("outputX"), implicitRadiusPlug);
		dgMod.doIt();
	}
	else {
		// Fallback: AttachShapeAttrs may have failed (e.g. NifBhkShape not
		// registered) - set a plain static value rather than leaving the
		// sphere at its implicit default radius of 0.
		MGlobal::displayWarning("NifCollisionImporter: bhkShapeAttrs link not found on sphere transform, falling back to a static radius value.");
		implicitRadiusPlug.setValue(radius * HAVOK_TO_BETHESDA_SCALE * scale);
	}

	outShapeNode = sphereTransform;
	return MStatus::kSuccess;
}

MStatus NifCollisionImporter::ImportCapsuleShape(Ref<bhkCapsuleShape> capsule, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode) {
	// Capsules have no direct Maya primitive equivalent, so the geometry is
	// generated by hand: a cylinder (the shaft) plus a hemisphere cap at each
	// end, aligned to the axis between firstPoint and secondPoint. Confirmed
	// via niflib source (bhkCapsuleShape.h): GetFirstPoint()/GetSecondPoint()
	// give the two axis endpoints, GetRadius() (inherited from
	// bhkConvexShape) is the capsule radius - Radius1()/Radius2() are
	// documented as "seems to match the first/second capsule radius", i.e.
	// redundant duplicates of the same value, not used here.
	Vector3 p1 = capsule->GetFirstPoint();
	Vector3 p2 = capsule->GetSecondPoint();
	float radius = capsule->GetRadius();
	const float scale = this->translatorOptions->importScale;

	MFnTransform transFn;
	MObject capsuleTransform = transFn.create(parentTransform);
	transFn.setName("bhkCapsuleShape");

	vector<Vector3> verts;
	vector<Triangle> tris;
	NifCollisionImporter::BuildCapsuleMesh(p1, p2, radius, verts, tris);

	MObject meshObj;
	if (!verts.empty() && !tris.empty()) {
		meshObj = this->CreateMeshFromVertices(verts, tris, capsuleTransform, "bhkCapsuleShapeMesh", layer, rigidBodyAttrsNode);
	}
	if (meshObj.isNull()) {
		MGlobal::displayWarning("NifCollisionImporter: bhkCapsuleShape mesh generation failed (degenerate capsule) - transform created but empty.");
	}

	this->AttachShapeAttrs(StaticCast<bhkSphereRepShape>(capsule), capsuleTransform);

	outShapeNode = capsuleTransform;
	return MStatus::kSuccess;
}

MStatus NifCollisionImporter::ImportListShape(Ref<bhkListShape> list, MObject parentTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode, MObject& outShapeNode) {
	vector<bhkShapeRef> subShapes = list->GetSubShapes();

	MFnTransform transFn;
	MObject listTransform = transFn.create(parentTransform);
	transFn.setName("bhkListShape");

	// bhkListShape has its own Material field that overrides every sub-shape's
	// individual material for sound/decal purposes - confirmed via geckwiki's
	// NIF Block Types page ("Overrides the impact material of all sub shapes,
	// any decal will behave as if it was the material specified. Footstep
	// sounds use the sub shape material sound."). Read once here, applied to
	// each sub-shape's NifBhkShape node below after ImportShape creates it.
	// NOTE: confirmed via niflib source (bhkListShape.h) that GetMaterial()
	// only returns HavokMaterial - there's no public getter for the parallel
	// skyrimMaterial field, same situation as bhkRigidBody::collisionResponse_.
	HavokMaterial listMaterial = list->GetMaterial();

	for (size_t i = 0; i < subShapes.size(); ++i) {
		MObject subShapeNode; // individual sub-shape nodes aren't surfaced to the caller -
		// targetShape points at the list transform as a whole
		this->ImportShape(subShapes[i], listTransform, layer, rigidBodyAttrsNode, subShapeNode);

		// Apply the list-level material override on top of whatever
		// AttachShapeAttrs already wrote from the sub-shape's own material.
		if (!subShapeNode.isNull()) {
			MFnDependencyNode subShapeDepFn(subShapeNode);
			MPlug shapeAttrsLinkPlug = subShapeDepFn.findPlug("bhkShapeAttrs", false);
			if (!shapeAttrsLinkPlug.isNull()) {
				MPlugArray shapeAttrsConnections;
				shapeAttrsLinkPlug.connectedTo(shapeAttrsConnections, true, false);
				if (shapeAttrsConnections.length() > 0) {
					MObject shapeAttrsNode = shapeAttrsConnections[0].node();
					MPlug(shapeAttrsNode, NifBhkShape::material).setValue((short)listMaterial);
					MPlug(shapeAttrsNode, NifBhkShape::materialOverriddenByList).setValue(true);
				}
			}
		}
	}

	outShapeNode = listTransform;
	return MStatus::kSuccess;
}

void NifCollisionImporter::ApplyWireframeDisplayOverride(MObject transform, MObject rigidBodyAttrsNode, float fallbackR, float fallbackG, float fallbackB) {
	MFnDagNode dagFn(transform);

	MPlug overrideEnabled = dagFn.findPlug("overrideEnabled", false);
	overrideEnabled.setValue(true);

	// overrideShading = false means "draw template/wireframe only, no shaded
	// surface" - this alone gives both the see-through look and the visible
	// outline, no material/shader needed.
	MPlug overrideShading = dagFn.findPlug("overrideShading", false);
	overrideShading.setValue(false);

	// overrideRGBColors switches the override from the 32-color index palette
	// (overrideColor) to a free RGB value (overrideColorRGB), so the wireframe
	// isn't limited to Maya's preset color swatches.
	MPlug overrideRGBColors = dagFn.findPlug("overrideRGBColors", false);
	overrideRGBColors.setValue(true);

	MPlug overrideColorRGB = dagFn.findPlug("overrideColorRGB", false);

	// Connect to the rigid body attrs node's live layerColor output instead of
	// writing a static value, so the wireframe color updates automatically if
	// layer is edited in the Attribute Editor after import (see
	// NifBhkRigidBody::compute()). Falls back to a static value only if no
	// attrs node was passed (shouldn't happen in the normal import flow, but
	// keeps this function safe to call standalone).
	if (!rigidBodyAttrsNode.isNull()) {
		MPlug layerColorPlug(rigidBodyAttrsNode, NifBhkRigidBody::layerColor);
		MDGModifier dgMod;
		dgMod.connect(layerColorPlug, overrideColorRGB);
		dgMod.doIt();
	}
	else {
		overrideColorRGB.child(0).setValue(fallbackR);
		overrideColorRGB.child(1).setValue(fallbackG);
		overrideColorRGB.child(2).setValue(fallbackB);
	}

	// overrideDisplayType: 0=Normal, 1=Template, 2=Reference.
	// Normal keeps these proxies fully clickable/selectable in the viewport -
	// Reference would make them visible but unselectable (good for backdrop
	// geometry, wrong for collision proxies the user needs to pick directly).
	MPlug overrideDisplayType = dagFn.findPlug("overrideDisplayType", false);
	overrideDisplayType.setValue(0);
}

void NifCollisionImporter::AttachShapeAttrs(Ref<bhkSphereRepShape> sphereRepShape, MObject shapeTransform) {
	MStatus status;

	MFnDependencyNode depFn;
	MObject attrNodeObj = depFn.create(NifBhkShape::id, "bhkShapeAttrs", &status);
	if (status != MStatus::kSuccess) {
		MGlobal::displayError("NifCollisionImporter: MFnDependencyNode::create() failed for NifBhkShape node - is it registered in initializePlugin?");
		return;
	}
	depFn.setObject(attrNodeObj);

	// NOTE: reads GetMaterial(), not GetSkyrimMaterial() - per
	// bhkSphereRepShape.cpp::Read(), userVersion < 12 (FNV exports with 11)
	// populates material; only userVersion >= 12 (Skyrim) populates
	// skyrimMaterial. Same kind of version-dependent field split as layer/
	// skyrimLayer, just keyed on userVersion here instead of version.
	MPlug(attrNodeObj, NifBhkShape::material).setValue((short)sphereRepShape->GetMaterial());

	// NOTE: stored as the raw Havok-unit value, NOT scaled by
	// HAVOK_TO_BETHESDA_SCALE - confirmed against a NifSkope screenshot of
	// bhkConvexVerticesShape (file had 0.1, NifSkope displayed 0.1 unscaled).
	// This is the bounding-sphere radius used internally by Havok for quick
	// collision rejection, not a visual geometry dimension, so it doesn't get
	// the same Bethesda-unit conversion applied to actual shape dimensions
	// (box sizeX/Y/Z, sphere shape radius, mesh vertex positions, etc.).
	MPlug(attrNodeObj, NifBhkShape::radius).setValue(sphereRepShape->GetRadius());

	// Link the attrs node to the shape transform via message attribute, same
	// pattern as bhkRigidBodyAttrs on the rigid body transform.
	MFnDependencyNode transformDepFn(shapeTransform);
	if (!transformDepFn.hasAttribute("bhkShapeAttrs")) {
		MFnMessageAttribute msgAttrFn;
		MObject linkAttr = msgAttrFn.create("bhkShapeAttrs", "bhkSA");
		transformDepFn.addAttribute(linkAttr);
	}
	MPlug linkPlug = transformDepFn.findPlug("bhkShapeAttrs", false, &status);
	MPlug messagePlug = MFnDependencyNode(attrNodeObj).findPlug("message", false);

	MDGModifier dgMod;
	dgMod.connect(messagePlug, linkPlug);
	dgMod.doIt();
}

MObject NifCollisionImporter::CreateMeshFromVertices(const vector<Vector3>& verts, const vector<Triangle>& tris, MObject parent, const char* name, SkyrimLayer layer, MObject rigidBodyAttrsNode) {
	const float scale = this->translatorOptions->importScale;

	MFloatPointArray points;
	for (size_t i = 0; i < verts.size(); ++i) {
		points.append(MFloatPoint(
			verts[i].x * HAVOK_TO_BETHESDA_SCALE * scale,
			verts[i].y * HAVOK_TO_BETHESDA_SCALE * scale,
			verts[i].z * HAVOK_TO_BETHESDA_SCALE * scale));
	}

	MIntArray polygonCounts;
	MIntArray polygonConnects;
	for (size_t i = 0; i < tris.size(); ++i) {
		polygonCounts.append(3);
		polygonConnects.append(tris[i].v1);
		polygonConnects.append(tris[i].v2);
		polygonConnects.append(tris[i].v3);
	}

	MStatus status;
	MFnMesh meshFn;
	MObject meshObj = meshFn.create(
		(int)points.length(), (int)polygonCounts.length(),
		points, polygonCounts, polygonConnects,
		parent, &status);

	if (status != MStatus::kSuccess) {
		return MObject();
	}

	MFnDagNode dagFn(meshObj);
	dagFn.setName(name);

	// Display override goes on the mesh's parent transform (the same transform
	// already named e.g. "bhkBoxShape"/"bhkConvexVertices") since overrideEnabled
	// on a transform also propagates visually to its shape children in the viewport.
	float r, g, b;
	NifBhkRigidBody::LayerToColor(layer, r, g, b);
	this->ApplyWireframeDisplayOverride(parent, rigidBodyAttrsNode, r, g, b);

	return meshObj;
}

MObject NifCollisionImporter::CreateMeshFromVertices(const vector<Vector4>& verts, const vector<Triangle>& tris, MObject parent, const char* name, SkyrimLayer layer, MObject rigidBodyAttrsNode) {
	// Convenience overload for shapes that hand back Vector4 (w is unused) -
	// just strip the w component and delegate to the Vector3 version.
	vector<Vector3> verts3;
	verts3.reserve(verts.size());
	for (size_t i = 0; i < verts.size(); ++i) {
		verts3.push_back(Vector3(verts[i].x, verts[i].y, verts[i].z));
	}
	return this->CreateMeshFromVertices(verts3, tris, parent, name, layer, rigidBodyAttrsNode);
}

string NifCollisionImporter::asString(bool verbose /*= false */) const {
	stringstream out;
	out << NifTranslatorFixtureItem::asString(verbose);
	out << "NifCollisionImporter" << endl;
	return out.str();
}

const Type& NifCollisionImporter::GetType() const {
	return TYPE;
}

const Type NifCollisionImporter::TYPE("NifCollisionImporter", &NifTranslatorFixtureItem::TYPE);