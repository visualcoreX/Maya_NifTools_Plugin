#include "include/Custom Nodes/NifBhkShape.h"

NifBhkShape::NifBhkShape() {
}

NifBhkShape::~NifBhkShape() {
}

// No DG computation needed - this node is just an attribute container.
// All reading/writing happens directly in the importer/exporter via MPlug.
MStatus NifBhkShape::compute(const MPlug& plug, MDataBlock& dataBlock) {
	return MStatus::kSuccess;
}

void* NifBhkShape::creator() {
	return new NifBhkShape();
}

MStatus NifBhkShape::initialize() {
	MStatus status;

	MFnEnumAttribute enum_attribute;
	MFnNumericAttribute numeric_attribute;

	// NOTE: Fallout3HavokMaterial is not a real C++ type in this niflib
	// build (it only exists in the nif.xml schema) - GetMaterial() returns
	// HavokMaterial, which only defines values 0-31. The full 0-127 range
	// below is taken directly from nif.xml's Fallout3HavokMaterial enum,
	// using raw integers since there are no matching C++ constants past 31.
	// Pattern: 32 base materials x4 suffix groups (none/_PLATFORM/_STAIRS/
	// _STAIRS_PLATFORM). Confirmed against a NifSkope screenshot (value 26
	// = FO_HAV_MAT_PISTOL for a pistol's bhkConvexVerticesShape).
	enum_attribute.create("material", "mtl", 0, &status);
	enum_attribute.addField("FO_HAV_MAT_STONE", 0);
	enum_attribute.addField("FO_HAV_MAT_CLOTH", 1);
	enum_attribute.addField("FO_HAV_MAT_DIRT", 2);
	enum_attribute.addField("FO_HAV_MAT_GLASS", 3);
	enum_attribute.addField("FO_HAV_MAT_GRASS", 4);
	enum_attribute.addField("FO_HAV_MAT_METAL", 5);
	enum_attribute.addField("FO_HAV_MAT_ORGANIC", 6);
	enum_attribute.addField("FO_HAV_MAT_SKIN", 7);
	enum_attribute.addField("FO_HAV_MAT_WATER", 8);
	enum_attribute.addField("FO_HAV_MAT_WOOD", 9);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_STONE", 10);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_METAL", 11);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_WOOD", 12);
	enum_attribute.addField("FO_HAV_MAT_CHAIN", 13);
	enum_attribute.addField("FO_HAV_MAT_BOTTLECAP", 14);
	enum_attribute.addField("FO_HAV_MAT_ELEVATOR", 15);
	enum_attribute.addField("FO_HAV_MAT_HOLLOW_METAL", 16);
	enum_attribute.addField("FO_HAV_MAT_SHEET_METAL", 17);
	enum_attribute.addField("FO_HAV_MAT_SAND", 18);
	enum_attribute.addField("FO_HAV_MAT_BROKEN_CONCRETE", 19);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_BODY", 20);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_PART_SOLID", 21);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_PART_HOLLOW", 22);
	enum_attribute.addField("FO_HAV_MAT_BARREL", 23);
	enum_attribute.addField("FO_HAV_MAT_BOTTLE", 24);
	enum_attribute.addField("FO_HAV_MAT_SODA_CAN", 25);
	enum_attribute.addField("FO_HAV_MAT_PISTOL", 26);
	enum_attribute.addField("FO_HAV_MAT_RIFLE", 27);
	enum_attribute.addField("FO_HAV_MAT_SHOPPING_CART", 28);
	enum_attribute.addField("FO_HAV_MAT_LUNCHBOX", 29);
	enum_attribute.addField("FO_HAV_MAT_BABY_RATTLE", 30);
	enum_attribute.addField("FO_HAV_MAT_RUBBER_BALL", 31);
	enum_attribute.addField("FO_HAV_MAT_STONE_PLATFORM", 32);
	enum_attribute.addField("FO_HAV_MAT_CLOTH_PLATFORM", 33);
	enum_attribute.addField("FO_HAV_MAT_DIRT_PLATFORM", 34);
	enum_attribute.addField("FO_HAV_MAT_GLASS_PLATFORM", 35);
	enum_attribute.addField("FO_HAV_MAT_GRASS_PLATFORM", 36);
	enum_attribute.addField("FO_HAV_MAT_METAL_PLATFORM", 37);
	enum_attribute.addField("FO_HAV_MAT_ORGANIC_PLATFORM", 38);
	enum_attribute.addField("FO_HAV_MAT_SKIN_PLATFORM", 39);
	enum_attribute.addField("FO_HAV_MAT_WATER_PLATFORM", 40);
	enum_attribute.addField("FO_HAV_MAT_WOOD_PLATFORM", 41);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_STONE_PLATFORM", 42);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_METAL_PLATFORM", 43);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_WOOD_PLATFORM", 44);
	enum_attribute.addField("FO_HAV_MAT_CHAIN_PLATFORM", 45);
	enum_attribute.addField("FO_HAV_MAT_BOTTLECAP_PLATFORM", 46);
	enum_attribute.addField("FO_HAV_MAT_ELEVATOR_PLATFORM", 47);
	enum_attribute.addField("FO_HAV_MAT_HOLLOW_METAL_PLATFORM", 48);
	enum_attribute.addField("FO_HAV_MAT_SHEET_METAL_PLATFORM", 49);
	enum_attribute.addField("FO_HAV_MAT_SAND_PLATFORM", 50);
	enum_attribute.addField("FO_HAV_MAT_BROKEN_CONCRETE_PLATFORM", 51);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_BODY_PLATFORM", 52);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_PART_SOLID_PLATFORM", 53);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_PART_HOLLOW_PLATFORM", 54);
	enum_attribute.addField("FO_HAV_MAT_BARREL_PLATFORM", 55);
	enum_attribute.addField("FO_HAV_MAT_BOTTLE_PLATFORM", 56);
	enum_attribute.addField("FO_HAV_MAT_SODA_CAN_PLATFORM", 57);
	enum_attribute.addField("FO_HAV_MAT_PISTOL_PLATFORM", 58);
	enum_attribute.addField("FO_HAV_MAT_RIFLE_PLATFORM", 59);
	enum_attribute.addField("FO_HAV_MAT_SHOPPING_CART_PLATFORM", 60);
	enum_attribute.addField("FO_HAV_MAT_LUNCHBOX_PLATFORM", 61);
	enum_attribute.addField("FO_HAV_MAT_BABY_RATTLE_PLATFORM", 62);
	enum_attribute.addField("FO_HAV_MAT_RUBBER_BALL_PLATFORM", 63);
	enum_attribute.addField("FO_HAV_MAT_STONE_STAIRS", 64);
	enum_attribute.addField("FO_HAV_MAT_CLOTH_STAIRS", 65);
	enum_attribute.addField("FO_HAV_MAT_DIRT_STAIRS", 66);
	enum_attribute.addField("FO_HAV_MAT_GLASS_STAIRS", 67);
	enum_attribute.addField("FO_HAV_MAT_GRASS_STAIRS", 68);
	enum_attribute.addField("FO_HAV_MAT_METAL_STAIRS", 69);
	enum_attribute.addField("FO_HAV_MAT_ORGANIC_STAIRS", 70);
	enum_attribute.addField("FO_HAV_MAT_SKIN_STAIRS", 71);
	enum_attribute.addField("FO_HAV_MAT_WATER_STAIRS", 72);
	enum_attribute.addField("FO_HAV_MAT_WOOD_STAIRS", 73);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_STONE_STAIRS", 74);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_METAL_STAIRS", 75);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_WOOD_STAIRS", 76);
	enum_attribute.addField("FO_HAV_MAT_CHAIN_STAIRS", 77);
	enum_attribute.addField("FO_HAV_MAT_BOTTLECAP_STAIRS", 78);
	enum_attribute.addField("FO_HAV_MAT_ELEVATOR_STAIRS", 79);
	enum_attribute.addField("FO_HAV_MAT_HOLLOW_METAL_STAIRS", 80);
	enum_attribute.addField("FO_HAV_MAT_SHEET_METAL_STAIRS", 81);
	enum_attribute.addField("FO_HAV_MAT_SAND_STAIRS", 82);
	enum_attribute.addField("FO_HAV_MAT_BROKEN_CONCRETE_STAIRS", 83);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_BODY_STAIRS", 84);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_PART_SOLID_STAIRS", 85);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_PART_HOLLOW_STAIRS", 86);
	enum_attribute.addField("FO_HAV_MAT_BARREL_STAIRS", 87);
	enum_attribute.addField("FO_HAV_MAT_BOTTLE_STAIRS", 88);
	enum_attribute.addField("FO_HAV_MAT_SODA_CAN_STAIRS", 89);
	enum_attribute.addField("FO_HAV_MAT_PISTOL_STAIRS", 90);
	enum_attribute.addField("FO_HAV_MAT_RIFLE_STAIRS", 91);
	enum_attribute.addField("FO_HAV_MAT_SHOPPING_CART_STAIRS", 92);
	enum_attribute.addField("FO_HAV_MAT_LUNCHBOX_STAIRS", 93);
	enum_attribute.addField("FO_HAV_MAT_BABY_RATTLE_STAIRS", 94);
	enum_attribute.addField("FO_HAV_MAT_RUBBER_BALL_STAIRS", 95);
	enum_attribute.addField("FO_HAV_MAT_STONE_STAIRS_PLATFORM", 96);
	enum_attribute.addField("FO_HAV_MAT_CLOTH_STAIRS_PLATFORM", 97);
	enum_attribute.addField("FO_HAV_MAT_DIRT_STAIRS_PLATFORM", 98);
	enum_attribute.addField("FO_HAV_MAT_GLASS_STAIRS_PLATFORM", 99);
	enum_attribute.addField("FO_HAV_MAT_GRASS_STAIRS_PLATFORM", 100);
	enum_attribute.addField("FO_HAV_MAT_METAL_STAIRS_PLATFORM", 101);
	enum_attribute.addField("FO_HAV_MAT_ORGANIC_STAIRS_PLATFORM", 102);
	enum_attribute.addField("FO_HAV_MAT_SKIN_STAIRS_PLATFORM", 103);
	enum_attribute.addField("FO_HAV_MAT_WATER_STAIRS_PLATFORM", 104);
	enum_attribute.addField("FO_HAV_MAT_WOOD_STAIRS_PLATFORM", 105);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_STONE_STAIRS_PLATFORM", 106);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_METAL_STAIRS_PLATFORM", 107);
	enum_attribute.addField("FO_HAV_MAT_HEAVY_WOOD_STAIRS_PLATFORM", 108);
	enum_attribute.addField("FO_HAV_MAT_CHAIN_STAIRS_PLATFORM", 109);
	enum_attribute.addField("FO_HAV_MAT_BOTTLECAP_STAIRS_PLATFORM", 110);
	enum_attribute.addField("FO_HAV_MAT_ELEVATOR_STAIRS_PLATFORM", 111);
	enum_attribute.addField("FO_HAV_MAT_HOLLOW_METAL_STAIRS_PLATFORM", 112);
	enum_attribute.addField("FO_HAV_MAT_SHEET_METAL_STAIRS_PLATFORM", 113);
	enum_attribute.addField("FO_HAV_MAT_SAND_STAIRS_PLATFORM", 114);
	enum_attribute.addField("FO_HAV_MAT_BROKEN_CONCRETE_STAIRS_PLATFORM", 115);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_BODY_STAIRS_PLATFORM", 116);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_PART_SOLID_STAIRS_PLATFORM", 117);
	enum_attribute.addField("FO_HAV_MAT_VEHICLE_PART_HOLLOW_STAIRS_PLATFORM", 118);
	enum_attribute.addField("FO_HAV_MAT_BARREL_STAIRS_PLATFORM", 119);
	enum_attribute.addField("FO_HAV_MAT_BOTTLE_STAIRS_PLATFORM", 120);
	enum_attribute.addField("FO_HAV_MAT_SODA_CAN_STAIRS_PLATFORM", 121);
	enum_attribute.addField("FO_HAV_MAT_PISTOL_STAIRS_PLATFORM", 122);
	enum_attribute.addField("FO_HAV_MAT_RIFLE_STAIRS_PLATFORM", 123);
	enum_attribute.addField("FO_HAV_MAT_SHOPPING_CART_STAIRS_PLATFORM", 124);
	enum_attribute.addField("FO_HAV_MAT_LUNCHBOX_STAIRS_PLATFORM", 125);
	enum_attribute.addField("FO_HAV_MAT_BABY_RATTLE_STAIRS_PLATFORM", 126);
	enum_attribute.addField("FO_HAV_MAT_RUBBER_BALL_STAIRS_PLATFORM", 127);
	material = enum_attribute.object();
	enum_attribute.setStorable(true);
	status = MPxNode::addAttribute(material);

	materialOverriddenByList = numeric_attribute.create("materialOverriddenByList", "movl", MFnNumericData::kBoolean, false, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(materialOverriddenByList);

	// Radius of the bounding sphere, stored as the raw Havok-unit value
	// (confirmed unscaled against a NifSkope screenshot - this is an internal
	// Havok quick-rejection value, not a visual geometry dimension).
	radius = numeric_attribute.create("radius", "rad", MFnNumericData::kFloat, 0.0, &status);
	numeric_attribute.setStorable(true);
	status = MPxNode::addAttribute(radius);

	return MStatus::kSuccess;
}

MObject NifBhkShape::material;
MObject NifBhkShape::materialOverriddenByList;
MObject NifBhkShape::radius;

// Unique type ID - must not collide with NifDismemberPartition (0x7ff11),
// NifBhkRigidBody (0x7ff12), or any other custom node ID registered in the
// plugin. Verify against the full list of registered IDs in initializePlugin
// before building.
MTypeId NifBhkShape::id(0x7ff13);