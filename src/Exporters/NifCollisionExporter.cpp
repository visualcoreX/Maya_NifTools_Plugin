#include "include/Exporters/NifCollisionExporter.h"
#include "include/Custom Nodes/NifBhkRigidBody.h"
#include "include/Custom Nodes/NifBhkShape.h"

#include <maya/MFnTransform.h>
#include <maya/MFnMesh.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MItMeshPolygon.h>

// Same conversion factor used by the importer (NifCollisionImporter.cpp):
// 1 Havok unit = 7 Bethesda/NIF units. Geometry/translation coming OUT of
// Maya needs to be divided by this to go back into Havok units for the file.
static const float HAVOK_TO_BETHESDA_SCALE = 7.0f;

NifCollisionExporter::NifCollisionExporter() {
}

NifCollisionExporter::NifCollisionExporter(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils)
	: NifTranslatorFixtureItem(translatorOptions, translatorData, translatorUtils) {
}

NifCollisionExporter::~NifCollisionExporter() {
}

MStatus NifCollisionExporter::ExportCollision(MObject dagNode, NiNodeRef niNode) {
	if (this->translatorOptions->importCollision == false) {
		// Same toggle used on import (see NifTranslatorOptions::importCollision) -
		// re-used here so "skip collision" means skip it in both directions
		// consistently, rather than needing a separate export-side option.
		return MStatus::kSuccess;
	}

	MFnDagNode dagFn(dagNode);

	MObject rigidBodyTransform;
	bool found = false;
	for (unsigned int i = 0; i < dagFn.childCount() && !found; ++i) {
		MObject child = dagFn.child(i);
		if (!child.hasFn(MFn::kTransform)) {
			continue;
		}
		MFnDagNode childDagFn(child);
		MString childName = childDagFn.name();
		// Strip Maya's automatic namespace/path prefixes are not an issue here
		// since we're comparing the short name childDagFn.name() gives us,
		// same as the importer used when calling setName("bhkRigidBody")/
		// setName("bhkRigidBodyT").
		if (childName == "bhkRigidBody" || childName == "bhkRigidBodyT") {
			rigidBodyTransform = child;
			found = true;
		}
	}

	if (!found) {
		return MStatus::kSuccess; // most nodes have no collision, this is normal
	}

	MGlobal::displayInfo(MString("NifCollisionExporter: found collision transform \"") + MFnDagNode(rigidBodyTransform).name() + "\" under \"" + dagFn.name() + "\"");

	return this->ExportCollisionDirect(rigidBodyTransform, niNode);
}

MStatus NifCollisionExporter::ExportCollisionDirect(MObject rigidBodyTransform, NiNodeRef niNode) {
	if (this->translatorOptions->importCollision == false) {
		return MStatus::kSuccess;
	}

	bhkRigidBodyRef rigidBody = this->ExportRigidBody(rigidBodyTransform);
	if (rigidBody == NULL) {
		MGlobal::displayWarning("NifCollisionExporter: ExportRigidBody failed, skipping collision for this node.");
		return MStatus::kSuccess;
	}

	bhkCollisionObjectRef collisionObj = new bhkCollisionObject();
	// CONFIRMED via bhkNiCollisionObject.h: SetBody(NiObject* value) takes a
	// raw pointer, not a Ref - StaticCast<NiObject>(rigidBody) gives a
	// Ref<NiObject>, so .operator->() unwraps it to the raw pointer SetBody expects.
	collisionObj->SetBody(StaticCast<NiObject>(rigidBody).operator->());

	// bhkFlags were packed onto the NifBhkRigidBody attrs node by
	// NifBhkRigidBody::PackBhkFlags on import - read them back the same way.
	MFnDependencyNode rigidBodyTransformDepFn(rigidBodyTransform);
	MPlug linkPlug = rigidBodyTransformDepFn.findPlug("bhkRigidBodyAttrs", false);
	if (!linkPlug.isNull()) {
		MPlugArray connections;
		linkPlug.connectedTo(connections, true, false);
		if (connections.length() > 0) {
			MObject attrNodeObj = connections[0].node();
			unsigned short bhkFlagsValue = NifBhkRigidBody::PackBhkFlags(attrNodeObj);
			collisionObj->SetFlags(bhkFlagsValue);
		}
	}

	// NOTE: following the same raw-pointer pattern confirmed for SetBody()
	// above (NiNode's setters for object references consistently take raw
	// pointers, not Ref<T>, in this niflib fork).
	niNode->SetCollisionObject(StaticCast<NiCollisionObject>(collisionObj).operator->());

	MGlobal::displayInfo("NifCollisionExporter: collision exported successfully.");
	return MStatus::kSuccess;
}

MObject NifCollisionExporter::FindShapeChild(MObject parentTransform) {
	MFnDagNode dagFn(parentTransform);
	for (unsigned int i = 0; i < dagFn.childCount(); ++i) {
		MObject child = dagFn.child(i);

		// bhkBoxShape/bhkSphereShape/bhkCapsuleShape/bhkListShape are each
		// their own named TRANSFORM (created via MFnTransform::create in
		// the importer's ImportBoxShape/ImportSphereShape/etc).
		if (child.hasFn(MFn::kTransform)) {
			MFnDagNode childDagFn(child);
			MString childName = childDagFn.name();
			if (childName == "bhkBoxShape" || childName == "bhkSphereShape" ||
				childName == "bhkCapsuleShape" || childName == "bhkListShape") {
				return child;
			}
		}

		// bhkConvexVerticesShape/bhkPackedNiTriStripsShape are different:
		// CreateMeshFromVertices creates the MESH SHAPE NODE directly under
		// parentTransform with no intermediate transform of its own (see
		// NifCollisionImporter::CreateMeshFromVertices -
		// MFnMesh::create(..., parent, &status)), and the
		// "bhkConvexVertices"/"bhkPackedTriStrips" name is assigned to that
		// mesh shape node itself, not a wrapping transform. Confirmed via a
		// real scene where this child only became visible in the Outliner
		// after enabling "Show Shapes" - it's a shape node, not a
		// transform, so child.hasFn(MFn::kTransform) above would never
		// match it.
		if (child.hasFn(MFn::kMesh)) {
			MFnDagNode childDagFn(child);
			MString childName = childDagFn.name();
			if (childName == "bhkConvexVertices" || childName == "bhkPackedTriStrips") {
				return child; // the mesh shape node itself - ExportShape dispatches by its name, and ReadMeshFromTransform accepts a bare mesh node directly (see its updated implementation)
			}
		}
	}
	return MObject();
}

MObject NifCollisionExporter::FindShapeAttrsNode(MObject shapeTransform) {
	MFnDependencyNode shapeDepFn(shapeTransform);
	MPlug linkPlug = shapeDepFn.findPlug("bhkShapeAttrs", false);
	if (!linkPlug.isNull()) {
		MPlugArray connections;
		linkPlug.connectedTo(connections, true, false);
		if (connections.length() > 0) {
			return connections[0].node();
		}
	}

	// Not found directly on shapeTransform - for bhkConvexVerticesShape and
	// bhkPackedNiTriStripsShape, shapeTransform here is actually the bare
	// mesh shape node (see FindShapeChild's comments: CreateMeshFromVertices
	// creates the mesh directly under the rigid body transform with no
	// transform of its own), and AttachShapeAttrs links material/radius to
	// that OWNING transform instead of the mesh node. Check there too.
	if (shapeTransform.hasFn(MFn::kMesh)) {
		MFnDagNode meshDagFn(shapeTransform);
		if (meshDagFn.parentCount() > 0) {
			MObject owningTransform = meshDagFn.parent(0);
			MFnDependencyNode owningDepFn(owningTransform);
			MPlug ownerLinkPlug = owningDepFn.findPlug("bhkShapeAttrs", false);
			if (!ownerLinkPlug.isNull()) {
				MPlugArray ownerConnections;
				ownerLinkPlug.connectedTo(ownerConnections, true, false);
				if (ownerConnections.length() > 0) {
					return ownerConnections[0].node();
				}
			}
		}
	}

	return MObject();
}

bhkRigidBodyRef NifCollisionExporter::ExportRigidBody(MObject rigidBodyTransform) {
	MFnDependencyNode rigidBodyTransformDepFn(rigidBodyTransform);
	MString transformName = rigidBodyTransformDepFn.name();
	bool isHavokT = (transformName == "bhkRigidBodyT");

	MPlug linkPlug = rigidBodyTransformDepFn.findPlug("bhkRigidBodyAttrs", false);
	if (linkPlug.isNull()) {
		MGlobal::displayWarning("NifCollisionExporter: rigid body transform has no \"bhkRigidBodyAttrs\" link attribute.");
		return NULL;
	}
	MPlugArray connections;
	linkPlug.connectedTo(connections, true, false);
	if (connections.length() == 0) {
		MGlobal::displayWarning("NifCollisionExporter: \"bhkRigidBodyAttrs\" link attribute is not connected to anything.");
		return NULL;
	}
	MObject attrNodeObj = connections[0].node();

	// CONFIRMED via bhkRigidBodyT.h earlier in this project: bhkRigidBodyT
	// has no extra fields of its own - same Translation/Rotation fields live
	// on bhkRigidBody, the engine just only applies them for the "T" variant.
	// So which concrete type to construct is decided purely by isHavokT here.
	// bhkRigidBodyT is a subclass of bhkRigidBody (confirmed: bhkRigidBodyT::TYPE
	// has bhkRigidBody::TYPE as its parent), so a plain pointer assignment
	// upcasts correctly - no StaticCast needed for this direction.
	bhkRigidBodyRef rigidBody;
	if (isHavokT) {
		rigidBody = new bhkRigidBodyT();
	}
	else {
		rigidBody = new bhkRigidBody();
	}

	const float scale = this->translatorOptions->importScale;

	float massVal, frictionVal, restitutionVal, linearDampingVal, angularDampingVal;
	float maxLinearVelocityVal, maxAngularVelocityVal, penetrationDepthVal;
	MPlug(attrNodeObj, NifBhkRigidBody::mass).getValue(massVal);
	MPlug(attrNodeObj, NifBhkRigidBody::friction).getValue(frictionVal);
	MPlug(attrNodeObj, NifBhkRigidBody::restitution).getValue(restitutionVal);
	MPlug(attrNodeObj, NifBhkRigidBody::linearDamping).getValue(linearDampingVal);
	MPlug(attrNodeObj, NifBhkRigidBody::angularDamping).getValue(angularDampingVal);
	MPlug(attrNodeObj, NifBhkRigidBody::maxLinearVelocity).getValue(maxLinearVelocityVal);
	MPlug(attrNodeObj, NifBhkRigidBody::maxAngularVelocity).getValue(maxAngularVelocityVal);
	MPlug(attrNodeObj, NifBhkRigidBody::penetrationDepth).getValue(penetrationDepthVal);

	rigidBody->SetMass(massVal);
	rigidBody->SetFriction(frictionVal);
	rigidBody->SetRestitution(restitutionVal);
	rigidBody->SetLinearDamping(linearDampingVal);
	rigidBody->SetAngularDamping(angularDampingVal);
	rigidBody->SetMaxLinearVelocity(maxLinearVelocityVal);
	rigidBody->SetMaxAngularVelocity(maxAngularVelocityVal);
	rigidBody->SetPenetrationDepth(penetrationDepthVal);

	short motionSystemVal, qualityTypeVal, deactivatorTypeVal, solverDeactivationVal;
	MPlug(attrNodeObj, NifBhkRigidBody::motionSystem).getValue(motionSystemVal);
	MPlug(attrNodeObj, NifBhkRigidBody::qualityType).getValue(qualityTypeVal);
	MPlug(attrNodeObj, NifBhkRigidBody::deactivatorType).getValue(deactivatorTypeVal);
	MPlug(attrNodeObj, NifBhkRigidBody::solverDeactivation).getValue(solverDeactivationVal);

	rigidBody->SetMotionSystem((MotionSystem)motionSystemVal);
	rigidBody->SetQualityType((MotionQuality)qualityTypeVal);
	rigidBody->SetDeactivatorType((DeactivatorType)deactivatorTypeVal);
	rigidBody->SetSolverDeactivation((SolverDeactivation)solverDeactivationVal);

	// bhkWorldObject field, inherited - CONFIRMED via bhkWorldObject.h:
	// SetSkyrimLayer(SkyrimLayer value). This is the field niflib actually
	// writes to the file for version >= VER_20_2_0_7 (every FNV file), the
	// same field GetSkyrimLayer() reads on import - see the importer's notes
	// on why SetLayer()/OblivionLayer is NOT used here.
	short layerVal;
	MPlug(attrNodeObj, NifBhkRigidBody::layer).getValue(layerVal);
	rigidBody->SetSkyrimLayer((SkyrimLayer)layerVal);
	// bhkRigidBody's "copy" of the layer (shown in NifSkope as the Layer
	// component of "Havok Filter" under "Rigid Body Info") switches between
	// skyrimLayerCopy/flagsAndPartNumberCopy and layerCopy/colFilterCopy
	// based on info.userVersion >= 12 (NOT info.version, unlike the
	// top-level Layer field above) - confirmed via the real Read()/Write()
	// code. FNV exports use userVersion=11, which is BELOW that threshold,
	// so it's actually layerCopy (OblivionLayer) that gets written for FNV,
	// not skyrimLayerCopy. SkyrimLayer's numeric values 0-18 are identical
	// to Fallout3Layer/OblivionLayer (confirmed earlier in this project), so
	// a numeric cast is safe for the FNV-relevant range. Both setters are
	// called so the correct one takes effect regardless of which userVersion
	// threshold a given export actually uses.
	rigidBody->SetSkyrimLayerCopy((SkyrimLayer)layerVal);
	rigidBody->SetLayerCopy((OblivionLayer)layerVal);

	// bhkWorldObject::colFilter/flagsAndPartNumber and their bhkRigidBody
	// "copy" equivalents (shown in NifSkope as the Flags component of the
	// top-level fields and of "Havok Filter" under "Rigid Body Info") - now
	// writable thanks to the niflib fork patch adding public Set methods for
	// all four. Writing to both the FNV/Oblivion-era setter (SetColFilter/
	// SetColFilterCopy) and the Skyrim-era one (SetFlagsAndPartNumber/
	// SetFlagsAndPartNumberCopy) regardless of target export version, since
	// Write() itself picks the right pair based on info.version - writing
	// both here is harmless and keeps this code version-agnostic.
	short havokFilterFlagsVal;
	MPlug(attrNodeObj, NifBhkRigidBody::havokFilterFlags).getValue(havokFilterFlagsVal);
	Niflib::byte havokFilterFlagsByte = (Niflib::byte)havokFilterFlagsVal;
	rigidBody->SetColFilter(havokFilterFlagsByte);
	rigidBody->SetFlagsAndPartNumber(havokFilterFlagsByte);
	rigidBody->SetColFilterCopy(havokFilterFlagsByte);
	rigidBody->SetFlagsAndPartNumberCopy(havokFilterFlagsByte);

	// Translation/rotation: only meaningful for bhkRigidBodyT, but the
	// importer stores it regardless of isHavokT, so write it back the same
	// way regardless here too - if it's a plain bhkRigidBody the engine just
	// ignores these fields anyway, matching niflib's own behavior.
	float transX, transY, transZ;
	MPlug(attrNodeObj, NifBhkRigidBody::havokTranslation).child(0).getValue(transX);
	MPlug(attrNodeObj, NifBhkRigidBody::havokTranslation).child(1).getValue(transY);
	MPlug(attrNodeObj, NifBhkRigidBody::havokTranslation).child(2).getValue(transZ);
	rigidBody->SetTranslation(Vector4(
		transX / (HAVOK_TO_BETHESDA_SCALE * scale),
		transY / (HAVOK_TO_BETHESDA_SCALE * scale),
		transZ / (HAVOK_TO_BETHESDA_SCALE * scale),
		0.0f));

	float rotX, rotY, rotZ, rotW;
	MPlug(attrNodeObj, NifBhkRigidBody::havokRotation).child(0).getValue(rotX);
	MPlug(attrNodeObj, NifBhkRigidBody::havokRotation).child(1).getValue(rotY);
	MPlug(attrNodeObj, NifBhkRigidBody::havokRotation).child(2).getValue(rotZ);
	MPlug(attrNodeObj, NifBhkRigidBody::havokRotationW).getValue(rotW);
	QuaternionXYZW rotation;
	rotation.x = rotX;
	rotation.y = rotY;
	rotation.z = rotZ;
	rotation.w = rotW;
	rigidBody->SetRotation(rotation);

	// Center of mass - undo the same Havok-to-Bethesda scale applied on import
	float centerX, centerY, centerZ;
	MPlug(attrNodeObj, NifBhkRigidBody::center).child(0).getValue(centerX);
	MPlug(attrNodeObj, NifBhkRigidBody::center).child(1).getValue(centerY);
	MPlug(attrNodeObj, NifBhkRigidBody::center).child(2).getValue(centerZ);
	rigidBody->SetCenter(Vector4(
		centerX / (HAVOK_TO_BETHESDA_SCALE * scale),
		centerY / (HAVOK_TO_BETHESDA_SCALE * scale),
		centerZ / (HAVOK_TO_BETHESDA_SCALE * scale),
		0.0f));

	// Inertia tensor - stored raw/unscaled on import (see
	// NifCollisionImporter::ImportRigidBody's notes), so written back as-is.
	// m14/m24/m34 (the unused 4th column) are left at 0, matching every
	// real-world example seen so far.
	float iM11, iM12, iM13, iM21, iM22, iM23, iM31, iM32, iM33;
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM11).child(0).getValue(iM11);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM11).child(1).getValue(iM12);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM11).child(2).getValue(iM13);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM21).child(0).getValue(iM21);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM21).child(1).getValue(iM22);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM21).child(2).getValue(iM23);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM31).child(0).getValue(iM31);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM31).child(1).getValue(iM32);
	MPlug(attrNodeObj, NifBhkRigidBody::inertiaM31).child(2).getValue(iM33);
	rigidBody->SetInertia(InertiaMatrix(
		iM11, iM12, iM13, 0.0f,
		iM21, iM22, iM23, 0.0f,
		iM31, iM32, iM33, 0.0f));

	// Shape: look for the named shape child transform under rigidBodyTransform
	// (the Maya hierarchy created by the importer mirrors the niflib one:
	// rigidBodyTransform's children include the shape transform, same as
	// bhkRigidBody's Shape field points at the bhkShape object).
	MObject shapeTransform = this->FindShapeChild(rigidBodyTransform);
	if (!shapeTransform.isNull()) {
		MObject shapeAttrsNode = this->FindShapeAttrsNode(shapeTransform);
		bhkShapeRef shape = this->ExportShape(shapeTransform, (SkyrimLayer)layerVal, attrNodeObj);
		if (shape != NULL) {
			// CONFIRMED via bhkWorldObject.h: SetShape(bhkShape* value) takes
			// a raw pointer, not a Ref.
			rigidBody->SetShape(shape.operator->());
		}
		else {
			MGlobal::displayWarning(MString("NifCollisionExporter: ExportShape failed for transform \"") + MFnDagNode(shapeTransform).name() + "\".");
		}
	}
	else {
		MGlobal::displayWarning("NifCollisionExporter: no recognized shape child transform found under the rigid body transform - exporting with no shape.");
	}

	return rigidBody;
}

bhkShapeRef NifCollisionExporter::ExportShape(MObject shapeTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode) {
	MFnDependencyNode shapeDepFn(shapeTransform);
	MString shapeName = shapeDepFn.name();

	if (shapeName == "bhkBoxShape") {
		return this->ExportBoxShape(shapeTransform);
	}
	if (shapeName == "bhkSphereShape") {
		return this->ExportSphereShape(shapeTransform);
	}
	// NOTE: the capsule's mesh is named "bhkCapsuleShapeMesh" (a child of the
	// "bhkCapsuleShape" transform - see ImportCapsuleShape/CreateMeshFromVertices),
	// but the transform we dispatch on here is "bhkCapsuleShape" itself.
	if (shapeName == "bhkCapsuleShape") {
		return this->ExportCapsuleShape(shapeTransform);
	}
	if (shapeName == "bhkConvexVertices") {
		return this->ExportConvexVerticesShape(shapeTransform);
	}
	if (shapeName == "bhkPackedTriStrips") {
		return this->ExportPackedTriStripsShape(shapeTransform);
	}
	if (shapeName == "bhkListShape") {
		return this->ExportListShape(shapeTransform, layer, rigidBodyAttrsNode);
	}

	MGlobal::displayWarning(MString("NifCollisionExporter: unrecognized shape transform name \"") + shapeName + "\" - skipping shape export.");
	return NULL;
}

bool NifCollisionExporter::ReadMeshFromTransform(MObject meshTransform, vector<Vector3>& outVerts, vector<Triangle>& outTris) {
	outVerts.clear();
	outTris.clear();

	MObject meshShape;
	bool found = false;

	// meshTransform may itself already be the mesh shape node (the
	// convexVertices/packedTriStrips case - see FindShapeChild's comments),
	// or a transform with a mesh child (every other case that goes through
	// here, e.g. capsule's "bhkCapsuleShapeMesh" child).
	if (meshTransform.hasFn(MFn::kMesh)) {
		MFnDagNode meshDagFn(meshTransform);
		if (!meshDagFn.isIntermediateObject()) {
			meshShape = meshTransform;
			found = true;
		}
	}
	else {
		MFnDagNode dagFn(meshTransform);
		for (unsigned int i = 0; i < dagFn.childCount() && !found; ++i) {
			MObject child = dagFn.child(i);
			if (child.hasFn(MFn::kMesh)) {
				MFnDagNode childDagFn(child);
				if (!childDagFn.isIntermediateObject()) {
					meshShape = child;
					found = true;
				}
			}
		}
	}

	if (!found) {
		return false;
	}

	const float scale = this->translatorOptions->importScale;
	MFnMesh meshFn(meshShape);

	MFloatPointArray points;
	meshFn.getPoints(points, MSpace::kObject);
	outVerts.reserve(points.length());
	for (unsigned int i = 0; i < points.length(); ++i) {
		outVerts.push_back(Vector3(
			points[i].x / (HAVOK_TO_BETHESDA_SCALE * scale),
			points[i].y / (HAVOK_TO_BETHESDA_SCALE * scale),
			points[i].z / (HAVOK_TO_BETHESDA_SCALE * scale)));
	}

	// Triangulate every polygon (Maya meshes are not guaranteed to already be
	// triangles, even though everything this importer creates is) - fan
	// triangulation from vertex 0 of each face, matching how most simple
	// convex/flat faces would be split.
	MItMeshPolygon polyIt(meshShape);
	for (; !polyIt.isDone(); polyIt.next()) {
		MIntArray verts;
		polyIt.getVertices(verts);
		for (unsigned int i = 1; i + 1 < verts.length(); ++i) {
			Triangle t;
			t.v1 = (unsigned short)verts[0];
			t.v2 = (unsigned short)verts[i];
			t.v3 = (unsigned short)verts[i + 1];
			outTris.push_back(t);
		}
	}

	return true;
}

bhkShapeRef NifCollisionExporter::ExportBoxShape(MObject shapeTransform) {
	Ref<bhkBoxShape> box = new bhkBoxShape();

	MFnDagNode dagFn(shapeTransform);
	MObject implicitBox;
	bool found = false;
	for (unsigned int i = 0; i < dagFn.childCount() && !found; ++i) {
		MObject child = dagFn.child(i);
		MFnDependencyNode childDepFn(child);
		if (childDepFn.typeName() == "implicitBox") {
			implicitBox = child;
			found = true;
		}
	}

	if (!found) {
		MGlobal::displayWarning("NifCollisionExporter: bhkBoxShape transform has no implicitBox child - exporting zero-size box.");
		return StaticCast<bhkShape>(box);
	}

	MFnDagNode implicitBoxDagFn(implicitBox);
	float sizeX, sizeY, sizeZ;
	implicitBoxDagFn.findPlug("sizeX").getValue(sizeX);
	implicitBoxDagFn.findPlug("sizeY").getValue(sizeY);
	implicitBoxDagFn.findPlug("sizeZ").getValue(sizeZ);

	const float scale = this->translatorOptions->importScale;
	// Undo the import-time x2 (full-extent -> half-extent, see ImportBoxShape's
	// note on implicitBox.sizeX already being a half-extent on its own) and
	// the Havok-to-Bethesda unit scale, in that order.
	Vector3 dims(
		sizeX / (2.0f * HAVOK_TO_BETHESDA_SCALE * scale),
		sizeY / (2.0f * HAVOK_TO_BETHESDA_SCALE * scale),
		sizeZ / (2.0f * HAVOK_TO_BETHESDA_SCALE * scale));
	box->SetDimensions(dims);

	MObject shapeAttrsNode = this->FindShapeAttrsNode(shapeTransform);
	if (!shapeAttrsNode.isNull()) {
		short materialVal;
		float radiusVal;
		MPlug(shapeAttrsNode, NifBhkShape::material).getValue(materialVal);
		MPlug(shapeAttrsNode, NifBhkShape::radius).getValue(radiusVal);
		box->SetMaterial((HavokMaterial)materialVal);
		box->SetRadius(radiusVal); // stored unscaled already, see NifBhkShape's notes
	}

	return StaticCast<bhkShape>(box);
}

bhkShapeRef NifCollisionExporter::ExportSphereShape(MObject shapeTransform) {
	Ref<bhkSphereShape> sphere = new bhkSphereShape();

	MObject shapeAttrsNode = this->FindShapeAttrsNode(shapeTransform);
	if (!shapeAttrsNode.isNull()) {
		short materialVal;
		float radiusVal;
		MPlug(shapeAttrsNode, NifBhkShape::material).getValue(materialVal);
		MPlug(shapeAttrsNode, NifBhkShape::radius).getValue(radiusVal);
		sphere->SetMaterial((HavokMaterial)materialVal);
		// radiusVal is the same value driving implicitSphere.radius via the
		// multiplyDivide node on import (see ImportSphereShape) - it's stored
		// unscaled in NifBhkShape, so no unit conversion needed here.
		sphere->SetRadius(radiusVal);
	}
	else {
		MGlobal::displayWarning("NifCollisionExporter: bhkSphereShape transform has no bhkShapeAttrs link - exporting zero radius.");
	}

	return StaticCast<bhkShape>(sphere);
}

bhkShapeRef NifCollisionExporter::ExportCapsuleShape(MObject shapeTransform) {
	Ref<bhkCapsuleShape> capsule = new bhkCapsuleShape();

	// The importer doesn't store firstPoint/secondPoint anywhere retrievable
	// (BuildCapsuleMesh consumes them once to generate the mesh and they're
	// gone) - reconstruct them from the generated mesh's bounding extremes
	// along its longest axis instead. This is an approximation: it recovers
	// the capsule's endpoints correctly only if the mesh wasn't reshaped by
	// hand in Maya in a way that changes its principal axis.
	vector<Vector3> verts;
	vector<Triangle> tris;
	bool hasMesh = this->ReadMeshFromTransform(shapeTransform, verts, tris);

	MObject shapeAttrsNode = this->FindShapeAttrsNode(shapeTransform);
	float radiusVal = 0.0f;
	if (!shapeAttrsNode.isNull()) {
		short materialVal;
		MPlug(shapeAttrsNode, NifBhkShape::material).getValue(materialVal);
		MPlug(shapeAttrsNode, NifBhkShape::radius).getValue(radiusVal);
		capsule->SetMaterial((HavokMaterial)materialVal);
	}

	if (hasMesh && !verts.empty()) {
		// Find the two vertices that are furthest apart - for a capsule mesh
		// built by BuildCapsuleMesh, these are exactly the two pole vertices,
		// i.e. firstPoint/secondPoint themselves (capsule poles sit exactly
		// radius beyond p1/p2 along the axis, so subtracting the radius along
		// that same axis direction recovers the original points).
		int bestI = 0, bestJ = 0;
		float bestDistSq = -1.0f;
		for (size_t i = 0; i < verts.size(); ++i) {
			for (size_t j = i + 1; j < verts.size(); ++j) {
				Vector3 diff(verts[i].x - verts[j].x, verts[i].y - verts[j].y, verts[i].z - verts[j].z);
				float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
				if (distSq > bestDistSq) {
					bestDistSq = distSq;
					bestI = (int)i;
					bestJ = (int)j;
				}
			}
		}

		Vector3 pole1 = verts[bestI];
		Vector3 pole2 = verts[bestJ];
		Vector3 axis(pole2.x - pole1.x, pole2.y - pole1.y, pole2.z - pole1.z);
		float axisLen = sqrtf(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);

		if (axisLen > radiusVal * 2.0f) {
			Vector3 axisDir(axis.x / axisLen, axis.y / axisLen, axis.z / axisLen);
			Vector3 p1(pole1.x + axisDir.x * radiusVal, pole1.y + axisDir.y * radiusVal, pole1.z + axisDir.z * radiusVal);
			Vector3 p2(pole2.x - axisDir.x * radiusVal, pole2.y - axisDir.y * radiusVal, pole2.z - axisDir.z * radiusVal);
			capsule->SetFirstPoint(p1);
			capsule->SetSecondPoint(p2);
		}
		else {
			MGlobal::displayWarning("NifCollisionExporter: bhkCapsuleShape mesh is too short relative to its radius to recover firstPoint/secondPoint reliably - using pole vertices directly.");
			capsule->SetFirstPoint(pole1);
			capsule->SetSecondPoint(pole2);
		}
	}
	else {
		MGlobal::displayWarning("NifCollisionExporter: bhkCapsuleShape has no mesh to recover firstPoint/secondPoint from - exporting degenerate capsule (both points at origin).");
		capsule->SetFirstPoint(Vector3(0.0f, 0.0f, 0.0f));
		capsule->SetSecondPoint(Vector3(0.0f, 0.0f, 0.0f));
	}

	capsule->SetRadius(radiusVal);
	// Radius1/Radius2 are documented as "seems to match the first/second
	// capsule radius" - keep them consistent with the main radius.
	capsule->SetRadius1(radiusVal);
	capsule->SetRadius2(radiusVal);

	return StaticCast<bhkShape>(capsule);
}

bhkShapeRef NifCollisionExporter::ExportConvexVerticesShape(MObject shapeTransform) {
	Ref<bhkConvexVerticesShape> cv = new bhkConvexVerticesShape();

	vector<Vector3> verts;
	vector<Triangle> tris;
	bool hasMesh = this->ReadMeshFromTransform(shapeTransform, verts, tris);

	if (!hasMesh || verts.empty()) {
		MGlobal::displayWarning("NifCollisionExporter: bhkConvexVerticesShape transform has no mesh - exporting with no vertices.");
		return StaticCast<bhkShape>(cv);
	}

	cv->SetVertices(verts);

	// bhkConvexVerticesShape's "normals" field is actually a set of half-space
	// planes that define the convex hull geometrically (not just per-vertex
	// shading normals) - see bhkConvexVerticesShape.h's field comments. We
	// don't have a stored hull triangulation to read back here (the importer
	// only built one transiently to generate the Maya mesh), so recompute it
	// from the mesh's own vertices via the same ComputeConvexHull algorithm
	// used on import, then derive one half-space plane per triangle face.
	//
	// NOTE: if the user edited the mesh in Maya in a way that makes it
	// non-convex, this recomputed hull may not match the visible mesh shape -
	// same trust-the-scene caveat noted in the header comment for this class.
	vector<Triangle> hullTris = NifCollisionImporter::ComputeConvexHull(verts);
	if (!hullTris.empty()) {
		vector<Vector4> normalsAndDist;
		normalsAndDist.reserve(hullTris.size());
		for (size_t i = 0; i < hullTris.size(); ++i) {
			const Vector3& a = verts[hullTris[i].v1];
			const Vector3& b = verts[hullTris[i].v2];
			const Vector3& c = verts[hullTris[i].v3];
			Vector3 edge1(b.x - a.x, b.y - a.y, b.z - a.z);
			Vector3 edge2(c.x - a.x, c.y - a.y, c.z - a.z);
			Vector3 normal(
				edge1.y * edge2.z - edge1.z * edge2.y,
				edge1.z * edge2.x - edge1.x * edge2.z,
				edge1.x * edge2.y - edge1.y * edge2.x);
			float len = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			if (len > 1e-8f) {
				normal.x /= len; normal.y /= len; normal.z /= len;
			}
			float dist = -(normal.x * a.x + normal.y * a.y + normal.z * a.z);
			normalsAndDist.push_back(Vector4(normal.x, normal.y, normal.z, dist));
		}
		cv->SetNormalsAndDist(normalsAndDist);
	}
	else {
		MGlobal::displayWarning("NifCollisionExporter: bhkConvexVerticesShape's mesh produced a degenerate convex hull when recomputing half-spaces - exporting vertices with no normals/half-spaces, which may not be physically valid in-game.");
	}

	MObject shapeAttrsNode = this->FindShapeAttrsNode(shapeTransform);
	if (!shapeAttrsNode.isNull()) {
		short materialVal;
		float radiusVal;
		MPlug(shapeAttrsNode, NifBhkShape::material).getValue(materialVal);
		MPlug(shapeAttrsNode, NifBhkShape::radius).getValue(radiusVal);
		cv->SetMaterial((HavokMaterial)materialVal);
		cv->SetRadius(radiusVal);
	}

	return StaticCast<bhkShape>(cv);
}

bhkShapeRef NifCollisionExporter::ExportPackedTriStripsShape(MObject shapeTransform) {
	Ref<bhkPackedNiTriStripsShape> packed = new bhkPackedNiTriStripsShape();

	vector<Vector3> verts;
	vector<Triangle> tris;
	bool hasMesh = this->ReadMeshFromTransform(shapeTransform, verts, tris);

	if (!hasMesh) {
		MGlobal::displayWarning("NifCollisionExporter: bhkPackedTriStrips transform has no mesh - exporting with no data.");
		return StaticCast<bhkShape>(packed);
	}

	Ref<hkPackedNiTriStripsData> data = new hkPackedNiTriStripsData();
	data->SetVertices(verts);
	data->SetNumFaces((int)tris.size());
	data->SetTriangles(tris);

	// CONFIRMED via bhkPackedNiTriStripsShape.h: SetData(hkPackedNiTriStripsData*
	// n) takes a raw pointer, not a Ref.
	packed->SetData(data.operator->());

	return StaticCast<bhkShape>(packed);
}

bhkShapeRef NifCollisionExporter::ExportListShape(MObject shapeTransform, SkyrimLayer layer, MObject rigidBodyAttrsNode) {
	Ref<bhkListShape> list = new bhkListShape();

	MFnDagNode dagFn(shapeTransform);
	vector<Ref<bhkShape>> subShapes;

	// bhkListShape's own children are sub-shape transforms directly (see
	// ImportListShape - it calls ImportShape on each sub-shape with
	// listTransform as their parent), so iterate children the same way
	// FindShapeChild does, but collect ALL matches instead of just the first.
	for (unsigned int i = 0; i < dagFn.childCount(); ++i) {
		MObject child = dagFn.child(i);
		if (!child.hasFn(MFn::kTransform)) {
			continue;
		}
		MFnDagNode childDagFn(child);
		MString childName = childDagFn.name();

		// geckwiki's documented restriction on bhkListShape (also stated
		// verbatim in bhkListShape.h's class comment): never put a
		// bhkPackedNiTriStripsShape inside a list shape's sub-shapes.
		if (childName == "bhkPackedTriStrips") {
			MGlobal::displayWarning("NifCollisionExporter: found a \"bhkPackedTriStrips\" child under a bhkListShape - this is documented as unsupported (geckwiki/bhkListShape.h both warn against it). Skipping this sub-shape.");
			continue;
		}

		if (childName == "bhkBoxShape" || childName == "bhkSphereShape" ||
			childName == "bhkCapsuleShape" || childName == "bhkConvexVertices" ||
			childName == "bhkListShape") {
			bhkShapeRef subShape = this->ExportShape(child, layer, rigidBodyAttrsNode);
			if (subShape != NULL) {
				subShapes.push_back(subShape);
			}
		}
	}

	list->SetSubShapes(subShapes);

	MObject shapeAttrsNode = this->FindShapeAttrsNode(shapeTransform);
	if (!shapeAttrsNode.isNull()) {
		short materialVal;
		MPlug(shapeAttrsNode, NifBhkShape::material).getValue(materialVal);
		list->SetMaterial((HavokMaterial)materialVal);
	}

	return StaticCast<bhkShape>(list);
}

string NifCollisionExporter::asString(bool verbose /*= false */) const {
	stringstream out;
	out << NifTranslatorFixtureItem::asString(verbose) << endl;
	out << "NifCollisionExporter" << endl;
	return out.str();
}

const Type& NifCollisionExporter::GetType() const {
	return TYPE;
}

const Type NifCollisionExporter::TYPE("NifCollisionExporter", &NifTranslatorFixtureItem::TYPE);