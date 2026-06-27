#include "include/Custom Nodes/NifBhkRigidBody.h"

NifBhkRigidBody::NifBhkRigidBody() {
}

NifBhkRigidBody::~NifBhkRigidBody() {
}

// Only layerColor is actually computed - everything else on this node is a
// plain stored attribute written directly by the importer/exporter via MPlug,
// same as before. layerColor is the one live, graph-driven output: it's
// connected to a collision shape's overrideColorRGB on import (see
// NifCollisionImporter::ImportRigidBody) so the wireframe color updates
// automatically if layer is edited afterward.
MStatus NifBhkRigidBody::compute(const MPlug& plug, MDataBlock& dataBlock) {
	if (plug == layerColor || plug.parent() == layerColor) {
		short layerValue = dataBlock.inputValue(layer).asShort();

		float r, g, b;
		NifBhkRigidBody::LayerToColor((SkyrimLayer)layerValue, r, g, b);

		MDataHandle outHandle = dataBlock.outputValue(layerColor);
		outHandle.set3Float(r, g, b);
		outHandle.setClean();

		dataBlock.setClean(plug);
		return MStatus::kSuccess;
	}

	return MStatus::kUnknownParameter;
}

// Colors for values 0-18 taken directly from the OblivionLayer/Fallout3Layer
// option comments in nif.xml (e.g. "STATIC (red)", "BIPED (green)") - those
// values are numerically identical in SkyrimLayer, and this is also how
// NifSkope colors FNV collision layers. Values 19+ are Skyrim-specific
// layers that don't appear in FNV files but are colored sensibly by
// category (debris, picks, zones) in case this code is ever reused there.
void NifBhkRigidBody::LayerToColor(SkyrimLayer layer, float& r, float& g, float& b) {
	switch (layer) {
	case SKYL_UNIDENTIFIED:    r = 1.0f; g = 1.0f; b = 1.0f; break; // white
	case SKYL_STATIC:          r = 1.0f; g = 0.0f; b = 0.0f; break; // red
	case SKYL_ANIMSTATIC:      r = 1.0f; g = 0.0f; b = 1.0f; break; // magenta
	case SKYL_TRANSPARENT:     r = 1.0f; g = 0.7f; b = 0.75f; break; // light pink
	case SKYL_CLUTTER:         r = 0.6f; g = 0.8f; b = 1.0f; break; // light blue
	case SKYL_WEAPON:          r = 1.0f; g = 0.5f; b = 0.0f; break; // orange
	case SKYL_PROJECTILE:      r = 1.0f; g = 0.7f; b = 0.4f; break; // light orange
	case SKYL_SPELL:           r = 0.0f; g = 1.0f; b = 1.0f; break; // cyan
	case SKYL_BIPED:           r = 0.0f; g = 1.0f; b = 0.0f; break; // green
	case SKYL_TREES:           r = 0.7f; g = 0.5f; b = 0.3f; break; // light brown
	case SKYL_PROPS:           r = 1.0f; g = 0.0f; b = 1.0f; break; // magenta
	case SKYL_WATER:           r = 0.0f; g = 1.0f; b = 1.0f; break; // cyan
	case SKYL_TRIGGER:         r = 0.8f; g = 0.8f; b = 0.8f; break; // light grey
	case SKYL_TERRAIN:         r = 1.0f; g = 1.0f; b = 0.6f; break; // light yellow
	case SKYL_TRAP:            r = 0.8f; g = 0.8f; b = 0.8f; break; // light grey
	case SKYL_NONCOLLIDABLE:   r = 1.0f; g = 1.0f; b = 1.0f; break; // white
	case SKYL_CLOUD_TRAP:      r = 0.6f; g = 0.7f; b = 0.6f; break; // greenish grey
	case SKYL_GROUND:          r = 0.5f; g = 0.5f; b = 0.5f; break; // (none) - neutral grey
	case SKYL_PORTAL:          r = 0.0f; g = 1.0f; b = 0.0f; break; // green
		// Skyrim-only layers beyond this point (19-47) - not used by FNV, but
		// colored by rough category rather than left undifferentiated.
	case SKYL_DEBRIS_SMALL: case SKYL_DEBRIS_LARGE:
		r = 0.6f; g = 0.4f; b = 0.2f; break; // brown, debris-ish
	case SKYL_ACOUSTIC_SPACE: case SKYL_ACTORZONE: case SKYL_PROJECTILEZONE:
		r = 0.8f; g = 0.8f; b = 0.8f; break; // light grey, trigger-like zones
	case SKYL_GASTRAP:         r = 0.6f; g = 0.9f; b = 0.3f; break; // yellowish green
	case SKYL_SHELLCASING:     r = 1.0f; g = 1.0f; b = 1.0f; break; // white
	case SKYL_TRANSPARENT_SMALL: case SKYL_TRANSPARENT_SMALL_ANIM:
		r = 1.0f; g = 0.7f; b = 0.75f; break; // light pink, like SKYL_TRANSPARENT
	case SKYL_INVISIBLE_WALL:  r = 1.0f; g = 1.0f; b = 1.0f; break; // white
	case SKYL_WARD:            r = 0.4f; g = 0.7f; b = 1.0f; break; // light blue, magic-ish
	case SKYL_CHARCONTROLLER:  r = 1.0f; g = 1.0f; b = 0.0f; break; // yellow
	case SKYL_STAIRHELPER:     r = 1.0f; g = 1.0f; b = 1.0f; break; // white
	case SKYL_DEADBIP: case SKYL_BIPED_NO_CC:
		r = 0.0f; g = 0.8f; b = 0.2f; break; // darker green, biped family
	case SKYL_AVOIDBOX:        r = 0.6f; g = 0.6f; b = 0.0f; break; // dark yellow
	case SKYL_COLLISIONBOX:    r = 1.0f; g = 1.0f; b = 1.0f; break; // white
	case SKYL_CAMERASHPERE: case SKYL_DOORDETECTION:
	case SKYL_CAMERAPICK: case SKYL_ITEMPICK: case SKYL_LINEOFSIGHT:
	case SKYL_PATHPICK: case SKYL_CUSTOMPICK1: case SKYL_CUSTOMPICK2:
	case SKYL_CONEPROJECTILE: case SKYL_SPELLEXPLOSION: case SKYL_DROPPINGPICK:
		r = 1.0f; g = 1.0f; b = 1.0f; break; // white, all "(white)" in nif.xml-style docs
	case SKYL_NULL:            r = 1.0f; g = 1.0f; b = 1.0f; break; // white
	default:
		r = 1.0f; g = 1.0f; b = 1.0f; break;
	}
}

void* NifBhkRigidBody::creator() {
	return new NifBhkRigidBody();
}

MStatus NifBhkRigidBody::initialize() {
	MStatus status;

	MFnMessageAttribute target_shape_attribute;
	MFnTypedAttribute string_attribute;
	MFnNumericAttribute numeric_attribute;

	// Link to the mesh holding the collision shape geometry
	target_shape_attribute.setWritable(true);
	target_shape_attribute.setConnectable(true);
	target_shape_attribute.setStorable(true);
	targetShape = target_shape_attribute.create("targetShape", "tS", &status);
	status = MPxNode::addAttribute(targetShape);

	// bhkCollisionObject flags shown as individual checkboxes (matches
	// NifSkope's bhkCOFlags multi-select list). ACTIVE defaults to true since
	// that's the common case (raw value 1 = only bit 0 set).
	//
	// Plain top-level boolean attributes - the 2-column "Bhk Flags" grouping
	// in the Attribute Editor is handled entirely by an AETemplate MEL proc
	// registered at plugin load time (see initializePlugin), not by nesting
	// these in a compound. Maya's native compound widget can't produce a
	// flat checkbox grid (nesting just adds more collapsible sub-headers),
	// so the grouping has to happen in the AE layout, not the data schema.
	bhkFlagActive = numeric_attribute.create("bhkFlagActive", "bhkFA", MFnNumericData::kBoolean, true, &status);
	numeric_attribute.setNiceNameOverride("Active");
	status = MPxNode::addAttribute(bhkFlagActive);

	bhkFlagResetTrans = numeric_attribute.create("bhkFlagResetTrans", "bhkFRT", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Reset Trans");
	status = MPxNode::addAttribute(bhkFlagResetTrans);

	bhkFlagNotify = numeric_attribute.create("bhkFlagNotify", "bhkFN", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Notify");
	status = MPxNode::addAttribute(bhkFlagNotify);

	bhkFlagSetLocal = numeric_attribute.create("bhkFlagSetLocal", "bhkFSL", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Set Local");
	status = MPxNode::addAttribute(bhkFlagSetLocal);

	bhkFlagDbgDisplay = numeric_attribute.create("bhkFlagDbgDisplay", "bhkFDD", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Dbg Display");
	status = MPxNode::addAttribute(bhkFlagDbgDisplay);

	bhkFlagUseVel = numeric_attribute.create("bhkFlagUseVel", "bhkFUV", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Use Vel");
	status = MPxNode::addAttribute(bhkFlagUseVel);

	bhkFlagReset = numeric_attribute.create("bhkFlagReset", "bhkFR", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Reset");
	status = MPxNode::addAttribute(bhkFlagReset);

	bhkFlagSyncOnUpdate = numeric_attribute.create("bhkFlagSyncOnUpdate", "bhkFSU", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Sync On Update");
	status = MPxNode::addAttribute(bhkFlagSyncOnUpdate);

	bhkFlagBlendPos = numeric_attribute.create("bhkFlagBlendPos", "bhkFBP", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Blend Pos");
	status = MPxNode::addAttribute(bhkFlagBlendPos);

	bhkFlagAlwaysBlend = numeric_attribute.create("bhkFlagAlwaysBlend", "bhkFAB", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Always Blend");
	status = MPxNode::addAttribute(bhkFlagAlwaysBlend);

	bhkFlagAnimTargeted = numeric_attribute.create("bhkFlagAnimTargeted", "bhkFAT", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Anim Targeted");
	status = MPxNode::addAttribute(bhkFlagAnimTargeted);

	bhkFlagDismemberedLimb = numeric_attribute.create("bhkFlagDismemberedLimb", "bhkFDL", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setNiceNameOverride("Dismembered Limb");
	status = MPxNode::addAttribute(bhkFlagDismemberedLimb);

	isHavokT = numeric_attribute.create("isHavokT", "isHT", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(isHavokT);

	// Enums shown as real Maya dropdown lists (MFnEnumAttribute), matching
	// what NifSkope shows for these same fields - much safer to edit than
	// free-text strings, since an unrecognized string used to silently fall
	// back to a default value.
	MFnEnumAttribute enum_attribute;

	// Layer: index matches the SkyrimLayer numeric value directly (0-47), and
	// labels use Fallout3Layer (FOL_*) names - see layerToString's note on why.
	enum_attribute.create("layer", "lyr", 1, &status); // default index 1 = FOL_STATIC
	enum_attribute.addField("FOL_UNIDENTIFIED", 0);
	enum_attribute.addField("FOL_STATIC", 1);
	enum_attribute.addField("FOL_ANIM_STATIC", 2);
	enum_attribute.addField("FOL_TRANSPARENT", 3);
	enum_attribute.addField("FOL_CLUTTER", 4);
	enum_attribute.addField("FOL_WEAPON", 5);
	enum_attribute.addField("FOL_PROJECTILE", 6);
	enum_attribute.addField("FOL_SPELL", 7);
	enum_attribute.addField("FOL_BIPED", 8);
	enum_attribute.addField("FOL_TREES", 9);
	enum_attribute.addField("FOL_PROPS", 10);
	enum_attribute.addField("FOL_WATER", 11);
	enum_attribute.addField("FOL_TRIGGER", 12);
	enum_attribute.addField("FOL_TERRAIN", 13);
	enum_attribute.addField("FOL_TRAP", 14);
	enum_attribute.addField("FOL_NONCOLLIDABLE", 15);
	enum_attribute.addField("FOL_CLOUD_TRAP", 16);
	enum_attribute.addField("FOL_GROUND", 17);
	enum_attribute.addField("FOL_PORTAL", 18);
	enum_attribute.addField("FOL_DEBRIS_SMALL", 19);
	enum_attribute.addField("FOL_DEBRIS_LARGE", 20);
	enum_attribute.addField("FOL_ACOUSTIC_SPACE", 21);
	enum_attribute.addField("FOL_ACTORZONE", 22);
	enum_attribute.addField("FOL_PROJECTILEZONE", 23);
	enum_attribute.addField("FOL_GASTRAP", 24);
	enum_attribute.addField("FOL_SHELLCASING", 25);
	enum_attribute.addField("FOL_TRANSPARENT_SMALL", 26);
	enum_attribute.addField("FOL_INVISIBLE_WALL", 27);
	enum_attribute.addField("FOL_TRANSPARENT_SMALL_ANIM", 28);
	enum_attribute.addField("FOL_DEADBIP", 29);
	enum_attribute.addField("FOL_CHARCONTROLLER", 30);
	enum_attribute.addField("FOL_AVOIDBOX", 31);
	enum_attribute.addField("FOL_COLLISIONBOX", 32);
	enum_attribute.addField("FOL_CAMERASPHERE", 33);
	enum_attribute.addField("FOL_DOORDETECTION", 34);
	enum_attribute.addField("FOL_CAMERAPICK", 35);
	enum_attribute.addField("FOL_ITEMPICK", 36);
	enum_attribute.addField("FOL_LINEOFSIGHT", 37);
	enum_attribute.addField("FOL_PATHPICK", 38);
	enum_attribute.addField("FOL_CUSTOMPICK1", 39);
	enum_attribute.addField("FOL_CUSTOMPICK2", 40);
	enum_attribute.addField("FOL_SPELLEXPLOSION", 41);
	enum_attribute.addField("FOL_DROPPINGPICK", 42);
	enum_attribute.addField("FOL_NULL", 43);
	enum_attribute.addField("FOL_UNUSED_44", 44);
	enum_attribute.addField("FOL_UNUSED_45", 45);
	enum_attribute.addField("FOL_UNUSED_46", 46);
	enum_attribute.addField("FOL_UNUSED_47", 47);
	layer = enum_attribute.object();
	enum_attribute.setStorable(true);
	status = MPxNode::addAttribute(layer);

	// Live RGB output driven by layer (see compute()). Output-only: not
	// storable/writable, since it's always derived, never set directly.
	layerColorR = numeric_attribute.create("layerColorR", "lyCR", MFnNumericData::kFloat, 1.0);
	numeric_attribute.setWritable(false);
	numeric_attribute.setStorable(false);
	layerColorG = numeric_attribute.create("layerColorG", "lyCG", MFnNumericData::kFloat, 1.0);
	numeric_attribute.setWritable(false);
	numeric_attribute.setStorable(false);
	layerColorB = numeric_attribute.create("layerColorB", "lyCB", MFnNumericData::kFloat, 1.0);
	numeric_attribute.setWritable(false);
	numeric_attribute.setStorable(false);
	layerColor = numeric_attribute.create("layerColor", "lyCol", layerColorR, layerColorG, layerColorB);
	numeric_attribute.setWritable(false);
	numeric_attribute.setStorable(false);
	status = MPxNode::addAttribute(layerColor);
	status = MPxNode::attributeAffects(layer, layerColor);

	// MotionSystem: index matches niflib's MotionSystem numeric value (0-9),
	// labels use NifSkope's display names where they differ (see
	// motionSystemToString's note on the SPHERE/SPHERE_INERTIA/BOX mismatch).
	enum_attribute.create("motionSystem", "mSys", MO_SYS_FIXED, &status);
	enum_attribute.addField("MO_SYS_INVALID", MO_SYS_INVALID);
	enum_attribute.addField("MO_SYS_DYNAMIC", MO_SYS_DYNAMIC);
	enum_attribute.addField("MO_SYS_SPHERE_INERTIA", MO_SYS_SPHERE);
	enum_attribute.addField("MO_SYS_SPHERE_STABILIZED", MO_SYS_SPHERE_INERTIA);
	enum_attribute.addField("MO_SYS_BOX_INERTIA", MO_SYS_BOX);
	enum_attribute.addField("MO_SYS_BOX_STABILIZED", MO_SYS_BOX_STABILIZED);
	enum_attribute.addField("MO_SYS_KEYFRAMED", MO_SYS_KEYFRAMED);
	enum_attribute.addField("MO_SYS_FIXED", MO_SYS_FIXED);
	enum_attribute.addField("MO_SYS_THIN_BOX", MO_SYS_THIN_BOX);
	enum_attribute.addField("MO_SYS_CHARACTER", MO_SYS_CHARACTER);
	motionSystem = enum_attribute.object();
	enum_attribute.setStorable(true);
	status = MPxNode::addAttribute(motionSystem);

	// MotionQuality: index matches niflib's MotionQuality numeric value (0-9)
	enum_attribute.create("qualityType", "qTyp", MO_QUAL_FIXED, &status);
	enum_attribute.addField("MO_QUAL_INVALID", MO_QUAL_INVALID);
	enum_attribute.addField("MO_QUAL_FIXED", MO_QUAL_FIXED);
	enum_attribute.addField("MO_QUAL_KEYFRAMED", MO_QUAL_KEYFRAMED);
	enum_attribute.addField("MO_QUAL_DEBRIS", MO_QUAL_DEBRIS);
	enum_attribute.addField("MO_QUAL_MOVING", MO_QUAL_MOVING);
	enum_attribute.addField("MO_QUAL_CRITICAL", MO_QUAL_CRITICAL);
	enum_attribute.addField("MO_QUAL_BULLET", MO_QUAL_BULLET);
	enum_attribute.addField("MO_QUAL_USER", MO_QUAL_USER);
	enum_attribute.addField("MO_QUAL_CHARACTER", MO_QUAL_CHARACTER);
	enum_attribute.addField("MO_QUAL_KEYFRAMED_REPORT", MO_QUAL_KEYFRAMED_REPORT);
	qualityType = enum_attribute.object();
	enum_attribute.setStorable(true);
	status = MPxNode::addAttribute(qualityType);

	// DeactivatorType: index matches niflib's DeactivatorType numeric value (0-2)
	enum_attribute.create("deactivatorType", "dTyp", DEACTIVATOR_NEVER, &status);
	enum_attribute.addField("DEACTIVATOR_INVALID", DEACTIVATOR_INVALID);
	enum_attribute.addField("DEACTIVATOR_NEVER", DEACTIVATOR_NEVER);
	enum_attribute.addField("DEACTIVATOR_SPATIAL", DEACTIVATOR_SPATIAL);
	deactivatorType = enum_attribute.object();
	enum_attribute.setStorable(true);
	status = MPxNode::addAttribute(deactivatorType);

	// SolverDeactivation: index matches niflib's SolverDeactivation numeric value (0-5)
	enum_attribute.create("solverDeactivation", "sDct", SOLVER_DEACTIVATION_OFF, &status);
	enum_attribute.addField("SOLVER_DEACTIVATION_INVALID", SOLVER_DEACTIVATION_INVALID);
	enum_attribute.addField("SOLVER_DEACTIVATION_OFF", SOLVER_DEACTIVATION_OFF);
	enum_attribute.addField("SOLVER_DEACTIVATION_LOW", SOLVER_DEACTIVATION_LOW);
	enum_attribute.addField("SOLVER_DEACTIVATION_MEDIUM", SOLVER_DEACTIVATION_MEDIUM);
	enum_attribute.addField("SOLVER_DEACTIVATION_HIGH", SOLVER_DEACTIVATION_HIGH);
	enum_attribute.addField("SOLVER_DEACTIVATION_MAX", SOLVER_DEACTIVATION_MAX);
	solverDeactivation = enum_attribute.object();
	enum_attribute.setStorable(true);
	status = MPxNode::addAttribute(solverDeactivation);

	// hkResponseType: index matches niflib's hkResponseType numeric value (0-3).
	// CONFIRMED via the actual bhkRigidBody.h header: collisionResponse_ is a
	// protected field with no public getter or setter at all in this niflib
	// build, so it can't be read on import - the dropdown stays at its default
	// (RESPONSE_SIMPLE_CONTACT, matching niflib's own constructor default for
	// this field). Editing it manually before export is the only way to set a
	// non-default value.
	enum_attribute.create("collisionResponse", "colR", RESPONSE_SIMPLE_CONTACT, &status);
	enum_attribute.addField("RESPONSE_INVALID", RESPONSE_INVALID);
	enum_attribute.addField("RESPONSE_SIMPLE_CONTACT", RESPONSE_SIMPLE_CONTACT);
	enum_attribute.addField("RESPONSE_REPORTING", RESPONSE_REPORTING);
	enum_attribute.addField("RESPONSE_NONE", RESPONSE_NONE);
	collisionResponse = enum_attribute.object();
	enum_attribute.setStorable(true);
	status = MPxNode::addAttribute(collisionResponse);

	mass = numeric_attribute.create("mass", "mss", MFnNumericData::kFloat, 1.0, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(mass);

	friction = numeric_attribute.create("friction", "frc", MFnNumericData::kFloat, 0.3, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(friction);

	restitution = numeric_attribute.create("restitution", "rst", MFnNumericData::kFloat, 0.3, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(restitution);

	linearDamping = numeric_attribute.create("linearDamping", "lDmp", MFnNumericData::kFloat, 0.1, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(linearDamping);

	angularDamping = numeric_attribute.create("angularDamping", "aDmp", MFnNumericData::kFloat, 0.05, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(angularDamping);

	maxLinearVelocity = numeric_attribute.create("maxLinearVelocity", "mLVl", MFnNumericData::kFloat, 104.4, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(maxLinearVelocity);

	maxAngularVelocity = numeric_attribute.create("maxAngularVelocity", "mAVl", MFnNumericData::kFloat, 31.57, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(maxAngularVelocity);

	penetrationDepth = numeric_attribute.create("penetrationDepth", "penD", MFnNumericData::kFloat, 0.15, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(penetrationDepth);

	// Translation/rotation - only meaningful when isHavokT is true, but always
	// stored so a round trip through Maya never silently drops the data.
	MObject havokTransX = numeric_attribute.create("havokTranslationX", "hTX", MFnNumericData::kFloat, 0.0);
	MObject havokTransY = numeric_attribute.create("havokTranslationY", "hTY", MFnNumericData::kFloat, 0.0);
	MObject havokTransZ = numeric_attribute.create("havokTranslationZ", "hTZ", MFnNumericData::kFloat, 0.0);
	havokTranslation = numeric_attribute.create("havokTranslation", "hTr", havokTransX, havokTransY, havokTransZ, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(havokTranslation);

	MObject havokRotX = numeric_attribute.create("havokRotationX", "hRX", MFnNumericData::kFloat, 0.0);
	MObject havokRotY = numeric_attribute.create("havokRotationY", "hRY", MFnNumericData::kFloat, 0.0);
	MObject havokRotZ = numeric_attribute.create("havokRotationZ", "hRZ", MFnNumericData::kFloat, 0.0);
	havokRotation = numeric_attribute.create("havokRotation", "hRo", havokRotX, havokRotY, havokRotZ, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(havokRotation);

	// W component stored separately - compounds only support 3 children per create() call
	havokRotationW = numeric_attribute.create("havokRotationW", "hRW", MFnNumericData::kFloat, 1.0, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(havokRotationW);

	return MStatus::kSuccess;
}

// --- SkyrimLayer (C++ type) <-> Fallout3Layer-style strings -----------------
// niflib reads the on-disk layer byte into bhkWorldObject::skyrimLayer (not
// the legacy ::layer/OblivionLayer field) for every file with version >=
// VER_20_2_0_7, which includes all FNV files (version 20.2.0.7) - confirmed
// via bhkWorldObject.cpp::Read(). The underlying C++ enum is SkyrimLayer (it's
// the only field niflib actually populates), but the strings shown/accepted
// here use FOL_* names from nif.xml's Fallout3Layer definition, matching what
// NifSkope displays for FNV files. Values 0-18 are numerically identical
// between SkyrimLayer and Fallout3Layer. Fallout3Layer stops at FOL_NULL=43;
// values 44-47 only exist in SkyrimLayer and never appear in real FNV content,
// but are still given FOL-style names here for round-trip safety. Falls back
// to FOL_STATIC for any out-of-range or unrecognized value.
MString NifBhkRigidBody::layerToString(SkyrimLayer layer_value) {
	switch (layer_value) {
	case SKYL_UNIDENTIFIED:         return MString("FOL_UNIDENTIFIED");
	case SKYL_STATIC:               return MString("FOL_STATIC");
	case SKYL_ANIMSTATIC:           return MString("FOL_ANIM_STATIC");
	case SKYL_TRANSPARENT:          return MString("FOL_TRANSPARENT");
	case SKYL_CLUTTER:              return MString("FOL_CLUTTER");
	case SKYL_WEAPON:               return MString("FOL_WEAPON");
	case SKYL_PROJECTILE:           return MString("FOL_PROJECTILE");
	case SKYL_SPELL:                return MString("FOL_SPELL");
	case SKYL_BIPED:                return MString("FOL_BIPED");
	case SKYL_TREES:                return MString("FOL_TREES");
	case SKYL_PROPS:                return MString("FOL_PROPS");
	case SKYL_WATER:                return MString("FOL_WATER");
	case SKYL_TRIGGER:              return MString("FOL_TRIGGER");
	case SKYL_TERRAIN:              return MString("FOL_TERRAIN");
	case SKYL_TRAP:                 return MString("FOL_TRAP");
	case SKYL_NONCOLLIDABLE:        return MString("FOL_NONCOLLIDABLE");
	case SKYL_CLOUD_TRAP:           return MString("FOL_CLOUD_TRAP");
	case SKYL_GROUND:               return MString("FOL_GROUND");
	case SKYL_PORTAL:               return MString("FOL_PORTAL");
	case SKYL_DEBRIS_SMALL:         return MString("FOL_DEBRIS_SMALL");
	case SKYL_DEBRIS_LARGE:         return MString("FOL_DEBRIS_LARGE");
	case SKYL_ACOUSTIC_SPACE:       return MString("FOL_ACOUSTIC_SPACE");
	case SKYL_ACTORZONE:            return MString("FOL_ACTORZONE");
	case SKYL_PROJECTILEZONE:       return MString("FOL_PROJECTILEZONE");
	case SKYL_GASTRAP:              return MString("FOL_GASTRAP");
	case SKYL_SHELLCASING:          return MString("FOL_SHELLCASING");
	case SKYL_TRANSPARENT_SMALL:    return MString("FOL_TRANSPARENT_SMALL");
	case SKYL_INVISIBLE_WALL:       return MString("FOL_INVISIBLE_WALL");
	case SKYL_TRANSPARENT_SMALL_ANIM: return MString("FOL_TRANSPARENT_SMALL_ANIM");
	case SKYL_WARD:                 return MString("FOL_DEADBIP"); // value 29: SkyrimLayer=WARD, Fallout3Layer=DEADBIP - same number, different meaning per-game
	case SKYL_CHARCONTROLLER:       return MString("FOL_CHARCONTROLLER");
	case SKYL_STAIRHELPER:          return MString("FOL_AVOIDBOX"); // value 31: SkyrimLayer=STAIRHELPER, Fallout3Layer=AVOIDBOX
	case SKYL_DEADBIP:              return MString("FOL_COLLISIONBOX"); // value 32: SkyrimLayer=DEADBIP, Fallout3Layer=COLLISIONBOX
	case SKYL_BIPED_NO_CC:          return MString("FOL_CAMERASPHERE"); // value 33: SkyrimLayer=BIPED_NO_CC, Fallout3Layer=CAMERASPHERE
	case SKYL_AVOIDBOX:             return MString("FOL_DOORDETECTION"); // value 34: SkyrimLayer=AVOIDBOX, Fallout3Layer=DOORDETECTION
	case SKYL_COLLISIONBOX:         return MString("FOL_CAMERAPICK"); // value 35
	case SKYL_CAMERASHPERE:         return MString("FOL_ITEMPICK"); // value 36
	case SKYL_DOORDETECTION:        return MString("FOL_LINEOFSIGHT"); // value 37
	case SKYL_CONEPROJECTILE:       return MString("FOL_PATHPICK"); // value 38
	case SKYL_CAMERAPICK:           return MString("FOL_CUSTOMPICK1"); // value 39
	case SKYL_ITEMPICK:             return MString("FOL_CUSTOMPICK2"); // value 40
	case SKYL_LINEOFSIGHT:          return MString("FOL_SPELLEXPLOSION"); // value 41
	case SKYL_PATHPICK:             return MString("FOL_DROPPINGPICK"); // value 42
	case SKYL_CUSTOMPICK1:          return MString("FOL_NULL"); // value 43
		// Values 44-47 only exist in SkyrimLayer, never in real FNV files -
		// Fallout3Layer has no equivalent (it stops at FOL_NULL=43).
	case SKYL_CUSTOMPICK2:          return MString("FOL_UNUSED_44");
	case SKYL_SPELLEXPLOSION:       return MString("FOL_UNUSED_45");
	case SKYL_DROPPINGPICK:         return MString("FOL_UNUSED_46");
	case SKYL_NULL:                 return MString("FOL_UNUSED_47");
	default:                        return MString("FOL_STATIC");
	}
}

SkyrimLayer NifBhkRigidBody::stringToLayer(const MString& str) {
	if (str == "FOL_UNIDENTIFIED")        return SKYL_UNIDENTIFIED;
	if (str == "FOL_STATIC")              return SKYL_STATIC;
	if (str == "FOL_ANIM_STATIC")         return SKYL_ANIMSTATIC;
	if (str == "FOL_TRANSPARENT")         return SKYL_TRANSPARENT;
	if (str == "FOL_CLUTTER")             return SKYL_CLUTTER;
	if (str == "FOL_WEAPON")              return SKYL_WEAPON;
	if (str == "FOL_PROJECTILE")          return SKYL_PROJECTILE;
	if (str == "FOL_SPELL")               return SKYL_SPELL;
	if (str == "FOL_BIPED")               return SKYL_BIPED;
	if (str == "FOL_TREES")               return SKYL_TREES;
	if (str == "FOL_PROPS")               return SKYL_PROPS;
	if (str == "FOL_WATER")               return SKYL_WATER;
	if (str == "FOL_TRIGGER")             return SKYL_TRIGGER;
	if (str == "FOL_TERRAIN")             return SKYL_TERRAIN;
	if (str == "FOL_TRAP")                return SKYL_TRAP;
	if (str == "FOL_NONCOLLIDABLE")       return SKYL_NONCOLLIDABLE;
	if (str == "FOL_CLOUD_TRAP")          return SKYL_CLOUD_TRAP;
	if (str == "FOL_GROUND")              return SKYL_GROUND;
	if (str == "FOL_PORTAL")              return SKYL_PORTAL;
	if (str == "FOL_DEBRIS_SMALL")        return SKYL_DEBRIS_SMALL;
	if (str == "FOL_DEBRIS_LARGE")        return SKYL_DEBRIS_LARGE;
	if (str == "FOL_ACOUSTIC_SPACE")      return SKYL_ACOUSTIC_SPACE;
	if (str == "FOL_ACTORZONE")           return SKYL_ACTORZONE;
	if (str == "FOL_PROJECTILEZONE")      return SKYL_PROJECTILEZONE;
	if (str == "FOL_GASTRAP")             return SKYL_GASTRAP;
	if (str == "FOL_SHELLCASING")         return SKYL_SHELLCASING;
	if (str == "FOL_TRANSPARENT_SMALL")   return SKYL_TRANSPARENT_SMALL;
	if (str == "FOL_INVISIBLE_WALL")      return SKYL_INVISIBLE_WALL;
	if (str == "FOL_TRANSPARENT_SMALL_ANIM") return SKYL_TRANSPARENT_SMALL_ANIM;
	if (str == "FOL_DEADBIP")             return SKYL_WARD;             // value 29
	if (str == "FOL_CHARCONTROLLER")      return SKYL_CHARCONTROLLER;
	if (str == "FOL_AVOIDBOX")            return SKYL_STAIRHELPER;      // value 31
	if (str == "FOL_COLLISIONBOX")        return SKYL_DEADBIP;          // value 32
	if (str == "FOL_CAMERASPHERE")        return SKYL_BIPED_NO_CC;      // value 33
	if (str == "FOL_DOORDETECTION")       return SKYL_AVOIDBOX;         // value 34
	if (str == "FOL_CAMERAPICK")          return SKYL_COLLISIONBOX;     // value 35
	if (str == "FOL_ITEMPICK")            return SKYL_CAMERASHPERE;     // value 36
	if (str == "FOL_LINEOFSIGHT")         return SKYL_DOORDETECTION;    // value 37
	if (str == "FOL_PATHPICK")            return SKYL_CONEPROJECTILE;   // value 38
	if (str == "FOL_CUSTOMPICK1")         return SKYL_CAMERAPICK;       // value 39
	if (str == "FOL_CUSTOMPICK2")         return SKYL_ITEMPICK;         // value 40
	if (str == "FOL_SPELLEXPLOSION")      return SKYL_LINEOFSIGHT;      // value 41
	if (str == "FOL_DROPPINGPICK")        return SKYL_PATHPICK;         // value 42
	if (str == "FOL_NULL")                return SKYL_CUSTOMPICK1;      // value 43
	if (str == "FOL_UNUSED_44")           return SKYL_CUSTOMPICK2;
	if (str == "FOL_UNUSED_45")           return SKYL_SPELLEXPLOSION;
	if (str == "FOL_UNUSED_46")           return SKYL_DROPPINGPICK;
	if (str == "FOL_UNUSED_47")           return SKYL_NULL;
	return SKYL_STATIC;
}

// --- MotionSystem <-> string --------------------------------------------------
// NOTE: string names here match what NifSkope actually displays (confirmed via
// screenshot), not niflib's own enum constant names. niflib's C++ enum values
// are numerically correct (0-9) but its names for values 2/3/4 are
// SPHERE/SPHERE_INERTIA/BOX, while NifSkope (and the underlying Havok SDK
// terminology) calls the same three values SPHERE_INERTIA/SPHERE_STABILIZED/
// BOX_INERTIA respectively. Same pattern as the Fallout3Layer vs SkyrimLayer
// naming mismatch - the byte in the file is identical, only the display name
// differs depending on which tool's enum table you read it through.
MString NifBhkRigidBody::motionSystemToString(MotionSystem ms) {
	switch (ms) {
	case MO_SYS_INVALID:        return MString("MO_SYS_INVALID");
	case MO_SYS_DYNAMIC:        return MString("MO_SYS_DYNAMIC");
	case MO_SYS_SPHERE:         return MString("MO_SYS_SPHERE_INERTIA");   // niflib value 2
	case MO_SYS_SPHERE_INERTIA: return MString("MO_SYS_SPHERE_STABILIZED"); // niflib value 3
	case MO_SYS_BOX:            return MString("MO_SYS_BOX_INERTIA");      // niflib value 4
	case MO_SYS_BOX_STABILIZED: return MString("MO_SYS_BOX_STABILIZED");
	case MO_SYS_KEYFRAMED:      return MString("MO_SYS_KEYFRAMED");
	case MO_SYS_FIXED:          return MString("MO_SYS_FIXED");
	case MO_SYS_THIN_BOX:       return MString("MO_SYS_THIN_BOX");
	case MO_SYS_CHARACTER:      return MString("MO_SYS_CHARACTER");
	default:                    return MString("MO_SYS_FIXED");
	}
}

MotionSystem NifBhkRigidBody::stringToMotionSystem(const MString& str) {
	if (str == "MO_SYS_INVALID")          return MO_SYS_INVALID;
	if (str == "MO_SYS_DYNAMIC")          return MO_SYS_DYNAMIC;
	if (str == "MO_SYS_SPHERE_INERTIA")   return MO_SYS_SPHERE;          // niflib value 2
	if (str == "MO_SYS_SPHERE_STABILIZED") return MO_SYS_SPHERE_INERTIA; // niflib value 3
	if (str == "MO_SYS_BOX_INERTIA")      return MO_SYS_BOX;             // niflib value 4
	if (str == "MO_SYS_BOX_STABILIZED")   return MO_SYS_BOX_STABILIZED;
	if (str == "MO_SYS_KEYFRAMED")        return MO_SYS_KEYFRAMED;
	if (str == "MO_SYS_FIXED")            return MO_SYS_FIXED;
	if (str == "MO_SYS_THIN_BOX")         return MO_SYS_THIN_BOX;
	if (str == "MO_SYS_CHARACTER")        return MO_SYS_CHARACTER;
	return MO_SYS_FIXED;
}

// --- MotionQuality <-> string --------------------------------------------------
MString NifBhkRigidBody::qualityTypeToString(MotionQuality qt) {
	switch (qt) {
	case MO_QUAL_INVALID:          return MString("MO_QUAL_INVALID");
	case MO_QUAL_FIXED:            return MString("MO_QUAL_FIXED");
	case MO_QUAL_KEYFRAMED:        return MString("MO_QUAL_KEYFRAMED");
	case MO_QUAL_DEBRIS:           return MString("MO_QUAL_DEBRIS");
	case MO_QUAL_MOVING:           return MString("MO_QUAL_MOVING");
	case MO_QUAL_CRITICAL:         return MString("MO_QUAL_CRITICAL");
	case MO_QUAL_BULLET:           return MString("MO_QUAL_BULLET");
	case MO_QUAL_USER:             return MString("MO_QUAL_USER");
	case MO_QUAL_CHARACTER:        return MString("MO_QUAL_CHARACTER");
	case MO_QUAL_KEYFRAMED_REPORT: return MString("MO_QUAL_KEYFRAMED_REPORT");
	default:                       return MString("MO_QUAL_FIXED");
	}
}

MotionQuality NifBhkRigidBody::stringToQualityType(const MString& str) {
	if (str == "MO_QUAL_INVALID")          return MO_QUAL_INVALID;
	if (str == "MO_QUAL_FIXED")            return MO_QUAL_FIXED;
	if (str == "MO_QUAL_KEYFRAMED")        return MO_QUAL_KEYFRAMED;
	if (str == "MO_QUAL_DEBRIS")           return MO_QUAL_DEBRIS;
	if (str == "MO_QUAL_MOVING")           return MO_QUAL_MOVING;
	if (str == "MO_QUAL_CRITICAL")         return MO_QUAL_CRITICAL;
	if (str == "MO_QUAL_BULLET")           return MO_QUAL_BULLET;
	if (str == "MO_QUAL_USER")             return MO_QUAL_USER;
	if (str == "MO_QUAL_CHARACTER")        return MO_QUAL_CHARACTER;
	if (str == "MO_QUAL_KEYFRAMED_REPORT") return MO_QUAL_KEYFRAMED_REPORT;
	return MO_QUAL_FIXED;
}

// --- hkResponseType <-> string ------------------------------------------------
MString NifBhkRigidBody::responseTypeToString(hkResponseType rt) {
	switch (rt) {
	case RESPONSE_INVALID:         return MString("RESPONSE_INVALID");
	case RESPONSE_SIMPLE_CONTACT:  return MString("RESPONSE_SIMPLE_CONTACT");
	case RESPONSE_REPORTING:       return MString("RESPONSE_REPORTING");
	case RESPONSE_NONE:            return MString("RESPONSE_NONE");
	default:                       return MString("RESPONSE_SIMPLE_CONTACT");
	}
}

hkResponseType NifBhkRigidBody::stringToResponseType(const MString& str) {
	if (str == "RESPONSE_INVALID")        return RESPONSE_INVALID;
	if (str == "RESPONSE_SIMPLE_CONTACT") return RESPONSE_SIMPLE_CONTACT;
	if (str == "RESPONSE_REPORTING")      return RESPONSE_REPORTING;
	if (str == "RESPONSE_NONE")           return RESPONSE_NONE;
	return RESPONSE_SIMPLE_CONTACT;
}

// --- DeactivatorType <-> string ------------------------------------------------
MString NifBhkRigidBody::deactivatorTypeToString(DeactivatorType dt) {
	switch (dt) {
	case DEACTIVATOR_INVALID: return MString("DEACTIVATOR_INVALID");
	case DEACTIVATOR_NEVER:   return MString("DEACTIVATOR_NEVER");
	case DEACTIVATOR_SPATIAL: return MString("DEACTIVATOR_SPATIAL");
	default:                  return MString("DEACTIVATOR_NEVER");
	}
}

DeactivatorType NifBhkRigidBody::stringToDeactivatorType(const MString& str) {
	if (str == "DEACTIVATOR_INVALID") return DEACTIVATOR_INVALID;
	if (str == "DEACTIVATOR_NEVER")   return DEACTIVATOR_NEVER;
	if (str == "DEACTIVATOR_SPATIAL") return DEACTIVATOR_SPATIAL;
	return DEACTIVATOR_NEVER;
}

// --- SolverDeactivation <-> string ---------------------------------------------
MString NifBhkRigidBody::solverDeactivationToString(SolverDeactivation sd) {
	switch (sd) {
	case SOLVER_DEACTIVATION_INVALID: return MString("SOLVER_DEACTIVATION_INVALID");
	case SOLVER_DEACTIVATION_OFF:     return MString("SOLVER_DEACTIVATION_OFF");
	case SOLVER_DEACTIVATION_LOW:     return MString("SOLVER_DEACTIVATION_LOW");
	case SOLVER_DEACTIVATION_MEDIUM:  return MString("SOLVER_DEACTIVATION_MEDIUM");
	case SOLVER_DEACTIVATION_HIGH:    return MString("SOLVER_DEACTIVATION_HIGH");
	case SOLVER_DEACTIVATION_MAX:     return MString("SOLVER_DEACTIVATION_MAX");
	default:                          return MString("SOLVER_DEACTIVATION_OFF");
	}
}

SolverDeactivation NifBhkRigidBody::stringToSolverDeactivation(const MString& str) {
	if (str == "SOLVER_DEACTIVATION_INVALID") return SOLVER_DEACTIVATION_INVALID;
	if (str == "SOLVER_DEACTIVATION_OFF")     return SOLVER_DEACTIVATION_OFF;
	if (str == "SOLVER_DEACTIVATION_LOW")     return SOLVER_DEACTIVATION_LOW;
	if (str == "SOLVER_DEACTIVATION_MEDIUM")  return SOLVER_DEACTIVATION_MEDIUM;
	if (str == "SOLVER_DEACTIVATION_HIGH")    return SOLVER_DEACTIVATION_HIGH;
	if (str == "SOLVER_DEACTIVATION_MAX")     return SOLVER_DEACTIVATION_MAX;
	return SOLVER_DEACTIVATION_OFF;
}

// --- bhkCOFlags <-> 12 individual booleans -----------------------------------
// Bit order confirmed against a NifSkope bhkCOFlags screenshot.
unsigned short NifBhkRigidBody::PackBhkFlags(MObject attrNode) {
	unsigned short flags = 0;
	bool v;

	MPlug(attrNode, bhkFlagActive).getValue(v);          if (v) flags |= (1 << 0);
	MPlug(attrNode, bhkFlagResetTrans).getValue(v);      if (v) flags |= (1 << 1);
	MPlug(attrNode, bhkFlagNotify).getValue(v);          if (v) flags |= (1 << 2);
	MPlug(attrNode, bhkFlagSetLocal).getValue(v);        if (v) flags |= (1 << 3);
	MPlug(attrNode, bhkFlagDbgDisplay).getValue(v);      if (v) flags |= (1 << 4);
	MPlug(attrNode, bhkFlagUseVel).getValue(v);          if (v) flags |= (1 << 5);
	MPlug(attrNode, bhkFlagReset).getValue(v);           if (v) flags |= (1 << 6);
	MPlug(attrNode, bhkFlagSyncOnUpdate).getValue(v);    if (v) flags |= (1 << 7);
	MPlug(attrNode, bhkFlagBlendPos).getValue(v);        if (v) flags |= (1 << 8);
	MPlug(attrNode, bhkFlagAlwaysBlend).getValue(v);     if (v) flags |= (1 << 9);
	MPlug(attrNode, bhkFlagAnimTargeted).getValue(v);    if (v) flags |= (1 << 10);
	MPlug(attrNode, bhkFlagDismemberedLimb).getValue(v); if (v) flags |= (1 << 11);

	return flags;
}

void NifBhkRigidBody::UnpackBhkFlags(MObject attrNode, unsigned short flags) {
	MPlug(attrNode, bhkFlagActive).setValue((flags & (1 << 0)) != 0);
	MPlug(attrNode, bhkFlagResetTrans).setValue((flags & (1 << 1)) != 0);
	MPlug(attrNode, bhkFlagNotify).setValue((flags & (1 << 2)) != 0);
	MPlug(attrNode, bhkFlagSetLocal).setValue((flags & (1 << 3)) != 0);
	MPlug(attrNode, bhkFlagDbgDisplay).setValue((flags & (1 << 4)) != 0);
	MPlug(attrNode, bhkFlagUseVel).setValue((flags & (1 << 5)) != 0);
	MPlug(attrNode, bhkFlagReset).setValue((flags & (1 << 6)) != 0);
	MPlug(attrNode, bhkFlagSyncOnUpdate).setValue((flags & (1 << 7)) != 0);
	MPlug(attrNode, bhkFlagBlendPos).setValue((flags & (1 << 8)) != 0);
	MPlug(attrNode, bhkFlagAlwaysBlend).setValue((flags & (1 << 9)) != 0);
	MPlug(attrNode, bhkFlagAnimTargeted).setValue((flags & (1 << 10)) != 0);
	MPlug(attrNode, bhkFlagDismemberedLimb).setValue((flags & (1 << 11)) != 0);
}

MObject NifBhkRigidBody::targetShape;
MObject NifBhkRigidBody::bhkFlagActive;
MObject NifBhkRigidBody::bhkFlagResetTrans;
MObject NifBhkRigidBody::bhkFlagNotify;
MObject NifBhkRigidBody::bhkFlagSetLocal;
MObject NifBhkRigidBody::bhkFlagDbgDisplay;
MObject NifBhkRigidBody::bhkFlagUseVel;
MObject NifBhkRigidBody::bhkFlagReset;
MObject NifBhkRigidBody::bhkFlagSyncOnUpdate;
MObject NifBhkRigidBody::bhkFlagBlendPos;
MObject NifBhkRigidBody::bhkFlagAlwaysBlend;
MObject NifBhkRigidBody::bhkFlagAnimTargeted;
MObject NifBhkRigidBody::bhkFlagDismemberedLimb;
MObject NifBhkRigidBody::isHavokT;
MObject NifBhkRigidBody::layer;
MObject NifBhkRigidBody::layerColor;
MObject NifBhkRigidBody::layerColorR;
MObject NifBhkRigidBody::layerColorG;
MObject NifBhkRigidBody::layerColorB;
MObject NifBhkRigidBody::motionSystem;
MObject NifBhkRigidBody::qualityType;
MObject NifBhkRigidBody::deactivatorType;
MObject NifBhkRigidBody::solverDeactivation;
MObject NifBhkRigidBody::collisionResponse;
MObject NifBhkRigidBody::mass;
MObject NifBhkRigidBody::friction;
MObject NifBhkRigidBody::restitution;
MObject NifBhkRigidBody::linearDamping;
MObject NifBhkRigidBody::angularDamping;
MObject NifBhkRigidBody::maxLinearVelocity;
MObject NifBhkRigidBody::maxAngularVelocity;
MObject NifBhkRigidBody::penetrationDepth;
MObject NifBhkRigidBody::havokTranslation;
MObject NifBhkRigidBody::havokRotation;
MObject NifBhkRigidBody::havokRotationW;

// Unique type ID - must not collide with NifDismemberPartition (0x7ff11) or
// any other custom node ID registered in the plugin. Verify against the full
// list of registered IDs in initializePlugin before building.
MTypeId NifBhkRigidBody::id(0x7ff12);