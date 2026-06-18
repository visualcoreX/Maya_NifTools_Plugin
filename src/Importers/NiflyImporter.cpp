// NiflyImporter.cpp - Standalone nifly-based importer
// This file MUST NOT include any niflib headers to avoid symbol conflicts

// Include our header FIRST
#include "include/Importers/NiflyImporter.h"

// Nifly includes
#include "NifFile.hpp"
#include "Geometry.hpp"
#include "Nodes.hpp"
#include "Shaders.hpp"

// Maya includes
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MFnPhongShader.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnSet.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MDGModifier.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MSelectionList.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MDagPath.h>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <algorithm>
#include <functional>
#include <unordered_set>

#include "include/Custom Nodes/NifDismemberPartition.h"

namespace {
	void AppendLog(const std::string& message) {
		const char* logPath = "C:\\Users\\sauron\\Documents\\maya\\2025\\scripts\\nifTranslator_debug.log";
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			log << message << std::endl;
		}
		// Also output to Maya script editor
		MGlobal::displayInfo(MString(message.c_str()));
	}

	MTransformationMatrix MakeMayaTransform(const nifly::MatTransform& xform, float importScale) {
		double mat[4][4] = {
			{ xform.rotation[0][0], xform.rotation[0][1], xform.rotation[0][2], 0.0 },
			{ xform.rotation[1][0], xform.rotation[1][1], xform.rotation[1][2], 0.0 },
			{ xform.rotation[2][0], xform.rotation[2][1], xform.rotation[2][2], 0.0 },
			{ 0.0, 0.0, 0.0, 1.0 }
		};
		MTransformationMatrix tm;
		tm = MMatrix(mat);
		tm.setTranslation(MVector(
			xform.translation.x * importScale,
			xform.translation.y * importScale,
			xform.translation.z * importScale
		), MSpace::kTransform);
		double scaleArr[3] = { xform.scale, xform.scale, xform.scale };
		tm.setScale(scaleArr, MSpace::kTransform);
		return tm;
	}
}

NiflyImporter::NiflyImporter() {}

NiflyImporter::~NiflyImporter() {}

void NiflyImporter::SetOptions(const NiflyImportOptions& options) {
	importOptions = options;
}

bool NiflyImporter::ShouldUseNifly(const std::string& filename) {
	// Try to load just the header to check version
	try {
		nifly::NifFile testFile;
		nifly::NifLoadOptions options;
		if (testFile.Load(std::filesystem::path(filename), options) != 0) {
			return false;
		}
		
		const nifly::NiVersion& ver = testFile.GetHeader().GetVersion();
		// Use nifly for SSE (stream 100), FO4 (stream 130), Skyrim LE (stream 83)
		return ver.Stream() >= 83;
	} catch (...) {
		return false;
	}
}

MStatus NiflyImporter::Import(const MFileObject& file) {
	std::string filename = file.resolvedFullName().asChar();
	LogMessage("[NiflyImporter] Loading: " + filename);
	
	// Store the import file directory for texture resolution
	importFileDir = file.rawPath().asChar();
	
	try {
		nifFile = std::make_unique<nifly::NifFile>();
		nifly::NifLoadOptions options;
		
		int result = nifFile->Load(std::filesystem::path(filename), options);
		if (result != 0) {
			lastError = "Failed to load NIF file (error " + std::to_string(result) + ")";
			LogError(lastError);
			MGlobal::displayError(MString(lastError.c_str()));
			return MStatus::kFailure;
		}
		
		if (!nifFile->IsValid()) {
			lastError = "NIF file is not valid";
			LogError(lastError);
			MGlobal::displayError(MString(lastError.c_str()));
			return MStatus::kFailure;
		}
		
		// Log some info about the file
		const nifly::NiVersion& ver = nifFile->GetHeader().GetVersion();
		std::ostringstream oss;
		oss << "[NiflyImporter] Version: " << ver.File() << " User: " << ver.User() << " Stream: " << ver.Stream();
		LogMessage(oss.str());
		
		return ImportShapes();
		
	} catch (const std::exception& e) {
		lastError = std::string("Exception during import: ") + e.what();
		LogError(lastError);
		MGlobal::displayError(MString(lastError.c_str()));
		return MStatus::kFailure;
	}
}

MStatus NiflyImporter::ImportShapes() {
	auto shapes = nifFile->GetShapes();
	LogMessage("[NiflyImporter] Found " + std::to_string(shapes.size()) + " shapes");
	
	if (shapes.empty()) {
		LogMessage("[NiflyImporter] No shapes to import");
		return MStatus::kSuccess;
	}
	
	// Get root node for parent transform
	MObject parentTransform = MObject::kNullObj;
	nifly::NiNode* rootNode = nifFile->GetRootNode();
	if (rootNode) {
		// Create a root transform
		MFnTransform transformFn;
		parentTransform = transformFn.create();
		
		std::string rootName = rootNode->name.get();
		if (rootName.empty()) rootName = "nif_root";
		transformFn.setName(MakeMayaName(rootName));
		
		// Apply root transform
		nifly::MatTransform rootXform = rootNode->GetTransformToParent();
		float scale = importOptions.importScale;
		transformFn.setTranslation(MVector(
			rootXform.translation.x * scale,
			rootXform.translation.y * scale,
			rootXform.translation.z * scale
		), MSpace::kTransform);
	}
	rootTransform = parentTransform;

	// Decide whether to use a reference skeleton for bones not in the NIF
	std::unordered_set<std::string> boneSet;
	for (auto* shape : shapes) {
		if (!shape) {
			continue;
		}
		std::vector<std::string> boneNames;
		nifFile->GetShapeBoneList(shape, boneNames);
		for (const auto& name : boneNames) {
			if (!name.empty()) {
				boneSet.insert(name);
			}
		}
	}

	std::vector<std::string> boneNames(boneSet.begin(), boneSet.end());
	useReferenceSkeleton = !boneNames.empty();
	if (useReferenceSkeleton) {
		LoadReferenceSkeleton();
		useReferenceSkeleton = (referenceSkeleton && referenceSkeleton->IsValid());
		if (useReferenceSkeleton) {
			LogMessage("[NiflyImporter] Using reference skeleton for joint hierarchy");
		} else {
			LogMessage("[NiflyImporter] Reference skeleton not found; using NIF nodes");
		}
	}

	// Build full joint hierarchy before applying skinning
	BuildJointHierarchy();
	
	for (nifly::NiShape* shape : shapes) {
		MStatus status = ImportShape(shape, parentTransform);
		if (status != MStatus::kSuccess) {
			LogMessage("[NiflyImporter] Warning: Failed to import shape");
		}
	}
	
	return MStatus::kSuccess;
}

MStatus NiflyImporter::ImportShape(nifly::NiShape* shape, const MObject& parentTransform) {
	if (!shape) return MStatus::kFailure;
	
	std::string shapeName = shape->name.get();
	LogMessage("[NiflyImporter] Importing shape: " + shapeName);
	
	NiflyShapeData shapeData;
	if (!ExtractShapeData(shape, shapeData)) {
		LogError("[NiflyImporter] Failed to extract shape data for: " + shapeName);
		return MStatus::kFailure;
	}
	
	NiflyTransform transform;
	ExtractTransform(shape, transform);
	
	return CreateMayaMesh(shapeData, transform, parentTransform, shape);
}

bool NiflyImporter::ExtractShapeData(nifly::NiShape* shape, NiflyShapeData& outData) {
	if (!shape) return false;
	
	outData.name = shape->name.get();
	if (outData.name.empty()) outData.name = "unnamed_shape";
	
	// Get vertices
	const std::vector<nifly::Vector3>* verts = nifFile->GetVertsForShape(shape);
	if (!verts || verts->empty()) {
		LogError("[NiflyImporter] Shape has no vertices: " + outData.name);
		return false;
	}
	
	outData.vertices.reserve(verts->size() * 3);
	for (const nifly::Vector3& v : *verts) {
		outData.vertices.push_back(v.x);
		outData.vertices.push_back(v.y);
		outData.vertices.push_back(v.z);
	}
	LogMessage("[NiflyImporter] Extracted " + std::to_string(verts->size()) + " vertices");
	
	// Get triangles
	std::vector<nifly::Triangle> tris;
	if (!shape->GetTriangles(tris) || tris.empty()) {
		LogError("[NiflyImporter] Shape has no triangles: " + outData.name);
		return false;
	}
	
	outData.triangles.reserve(tris.size() * 3);
	for (const nifly::Triangle& t : tris) {
		outData.triangles.push_back(t.p1);
		outData.triangles.push_back(t.p2);
		outData.triangles.push_back(t.p3);
	}
	LogMessage("[NiflyImporter] Extracted " + std::to_string(tris.size()) + " triangles");
	
	// Get normals
	const std::vector<nifly::Vector3>* norms = nifFile->GetNormalsForShape(shape);
	if (norms && !norms->empty()) {
		outData.normals.reserve(norms->size() * 3);
		for (const nifly::Vector3& n : *norms) {
			outData.normals.push_back(n.x);
			outData.normals.push_back(n.y);
			outData.normals.push_back(n.z);
		}
	}
	
	// Get UVs
	const std::vector<nifly::Vector2>* uvs = nifFile->GetUvsForShape(shape);
	if (uvs && !uvs->empty()) {
		outData.uvs.reserve(uvs->size() * 2);
		for (const nifly::Vector2& uv : *uvs) {
			outData.uvs.push_back(uv.u);
			outData.uvs.push_back(1.0f - uv.v); // Flip V for Maya
		}
	}
	
	// Get vertex colors
	const std::vector<nifly::Color4>* colors = nifFile->GetColorsForShape(shape);
	if (colors && !colors->empty()) {
		outData.colors.reserve(colors->size() * 4);
		for (const nifly::Color4& c : *colors) {
			outData.colors.push_back(c.r);
			outData.colors.push_back(c.g);
			outData.colors.push_back(c.b);
			outData.colors.push_back(c.a);
		}
	}
	
	// Get shader/material info
	nifly::NiShader* shader = nifFile->GetShader(shape);
	if (shader) {
		LogMessage("[NiflyImporter] Shader found for shape: " + outData.name);
		// Try to get BSLightingShaderProperty
		nifly::BSLightingShaderProperty* lsShader = dynamic_cast<nifly::BSLightingShaderProperty*>(shader);
		if (lsShader) {
			outData.specularStrength = lsShader->GetSpecularStrength();
			outData.glossiness = lsShader->GetGlossiness();
			outData.alpha = lsShader->GetAlpha();
			
			// Get textures from the texture set
			nifly::BSShaderTextureSet* texSet = nifFile->GetHeader().GetBlock<nifly::BSShaderTextureSet>(lsShader->textureSetRef);
			if (texSet && texSet->textures.size() > 0) {
				outData.diffuseTexture = texSet->textures[0].get();
				if (texSet->textures.size() > 1) {
					outData.normalTexture = texSet->textures[1].get();
				}
				if (texSet->textures.size() > 2) {
					outData.glowTexture = texSet->textures[2].get();
				}
				LogMessage("[NiflyImporter] Textures: diffuse=" + outData.diffuseTexture +
						   " normal=" + outData.normalTexture +
						   " glow=" + outData.glowTexture);
			} else {
				LogMessage("[NiflyImporter] No texture set or empty textures for shape: " + outData.name);
			}
		} else {
			LogMessage("[NiflyImporter] Shader is not BSLightingShaderProperty for shape: " + outData.name);
		}
	} else {
		LogMessage("[NiflyImporter] No shader found for shape: " + outData.name);
	}
	
	// Check for alpha property
	nifly::NiAlphaProperty* alpha = nifFile->GetAlphaProperty(shape);
	if (alpha) {
		outData.hasAlpha = true;
	}
	
	return true;
}

bool NiflyImporter::ExtractTransform(nifly::NiShape* shape, NiflyTransform& outTransform) {
	if (!shape) return false;
	
	nifly::MatTransform xform = shape->GetTransformToParent();
	
	outTransform.translation[0] = xform.translation.x;
	outTransform.translation[1] = xform.translation.y;
	outTransform.translation[2] = xform.translation.z;
	
	// Row-major 3x3 rotation matrix
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			outTransform.rotation[i * 3 + j] = xform.rotation[i][j];
		}
	}
	
	outTransform.scale = xform.scale;
	
	return true;
}

MStatus NiflyImporter::CreateMayaMesh(const NiflyShapeData& shapeData, const NiflyTransform& transform, const MObject& parentTransform, nifly::NiShape* shape) {
	MStatus status;
	
	size_t numVerts = shapeData.vertices.size() / 3;
	size_t numTris = shapeData.triangles.size() / 3;
	
	if (numVerts == 0 || numTris == 0) {
		LogError("[NiflyImporter] Empty mesh data for: " + shapeData.name);
		return MStatus::kFailure;
	}
	
	// Create vertex array with scaling
	float scale = importOptions.importScale;
	MFloatPointArray vertexArray(static_cast<unsigned int>(numVerts));
	for (size_t i = 0; i < numVerts; i++) {
		float x = shapeData.vertices[i * 3 + 0] * scale;
		float y = shapeData.vertices[i * 3 + 1] * scale;
		float z = shapeData.vertices[i * 3 + 2] * scale;
		vertexArray.set(static_cast<unsigned int>(i), x, y, z);
	}
	
	// Create polygon counts (all triangles = 3 vertices each)
	MIntArray polygonCounts(static_cast<unsigned int>(numTris), 3);
	
	// Create polygon connects (triangle indices)
	MIntArray polygonConnects(static_cast<unsigned int>(numTris * 3));
	for (size_t i = 0; i < numTris * 3; i++) {
		unsigned int idx = shapeData.triangles[i];
		// Clamp to valid range
		if (idx >= numVerts) idx = 0;
		polygonConnects.set(static_cast<int>(idx), static_cast<unsigned int>(i));
	}
	
	// Create UV arrays if present
	MFloatArray uArray, vArray;
	if (!shapeData.uvs.empty()) {
		size_t numUVs = shapeData.uvs.size() / 2;
		uArray.setLength(static_cast<unsigned int>(numUVs));
		vArray.setLength(static_cast<unsigned int>(numUVs));
		for (size_t i = 0; i < numUVs; i++) {
			uArray.set(shapeData.uvs[i * 2], static_cast<unsigned int>(i));
			vArray.set(shapeData.uvs[i * 2 + 1], static_cast<unsigned int>(i));
		}
	}
	
	// Create the mesh
	MFnMesh meshFn;
	MObject meshObj = meshFn.create(
		static_cast<int>(numVerts),
		static_cast<int>(numTris),
		vertexArray,
		polygonCounts,
		polygonConnects,
		parentTransform.isNull() ? MObject::kNullObj : parentTransform,
		&status
	);
	
	if (status != MStatus::kSuccess || meshObj.isNull()) {
		LogError("[NiflyImporter] Failed to create mesh: " + shapeData.name);
		return MStatus::kFailure;
	}
	
	// Set the mesh name
	MString mayaName = MakeMayaName(shapeData.name);
	meshFn.setName(mayaName + "Shape");
	
	// Get the transform and name it
	MFnDagNode dagFn(meshObj);
	MObject transformObj = dagFn.parent(0);
	if (!transformObj.isNull()) {
		MFnDagNode transformDagFn(transformObj);
		transformDagFn.setName(mayaName);

		// Apply shape transform (skinned meshes use global-to-skin derived transform)
		MFnTransform transformFnObj(transformObj);
		bool hasSkin = false;
		if (shape != nullptr) {
			std::vector<std::string> boneNames;
			hasSkin = (nifFile->GetShapeBoneList(shape, boneNames) > 0 && !boneNames.empty());
		}

		if (hasSkin && shape != nullptr) {
			nifly::MatTransform shapeXf = shape->GetTransformToParent();
			nifly::MatTransform calcXf;
			bool hasCalc = nifFile->CalcShapeTransformGlobalToSkin(shape, calcXf);
			if (!hasCalc) {
				calcXf.Clear();
			}

			nifly::MatTransform globalToSkin;
			bool hasGlobal = nifFile->GetShapeTransformGlobalToSkin(shape, globalToSkin);

			nifly::MatTransform objXf;
			if (hasGlobal) {
				objXf = shapeXf.ComposeTransforms(globalToSkin).ComposeTransforms(calcXf).InverseTransform();
			} else if (hasCalc) {
				objXf = calcXf.InverseTransform();
			} else {
				objXf = shapeXf;
			}

			transformFnObj.set(MakeMayaTransform(objXf, scale));
		} else {
			nifly::MatTransform meshXf;
			meshXf.translation = nifly::Vector3(transform.translation[0], transform.translation[1], transform.translation[2]);
			meshXf.rotation = nifly::Matrix3(
				transform.rotation[0], transform.rotation[1], transform.rotation[2],
				transform.rotation[3], transform.rotation[4], transform.rotation[5],
				transform.rotation[6], transform.rotation[7], transform.rotation[8]
			);
			meshXf.scale = transform.scale;
			transformFnObj.set(MakeMayaTransform(meshXf, scale));
		}
	}
	
	// Assign UVs
	if (!shapeData.uvs.empty() && uArray.length() > 0) {
		MString uvSetName = meshFn.currentUVSetName();
		status = meshFn.setUVs(uArray, vArray, &uvSetName);
		if (status == MStatus::kSuccess) {
			// Assign UVs to faces
			MIntArray uvIds(static_cast<unsigned int>(numTris * 3));
			for (size_t i = 0; i < numTris * 3; i++) {
				unsigned int idx = shapeData.triangles[i];
				if (idx >= uArray.length()) idx = 0;
				uvIds.set(static_cast<int>(idx), static_cast<unsigned int>(i));
			}
			meshFn.assignUVs(polygonCounts, uvIds, &uvSetName);
		}
	}
	
	// Create and assign material
	MObject materialObj;
	status = CreateMayaMaterial(shapeData, materialObj);
	if (status == MStatus::kSuccess && !materialObj.isNull()) {
		// Create a shading group and assign the mesh (no restrictions)
		MFnSet setFn;
		MSelectionList selList;
		MDagPath meshPath;
		MDagPath::getAPathTo(meshObj, meshPath);
		selList.add(meshPath);

		MObject shadingGroup = setFn.create(selList, MFnSet::kRenderableOnly, false, &status);
		if (status == MStatus::kSuccess) {
			setFn.setName("shadingGroup");

			// Connect shader to shading group (replace default connection)
			MDGModifier dgMod;
			MFnDependencyNode shaderFn(materialObj);
			MPlug surfaceShader = setFn.findPlug("surfaceShader", true);
			MPlugArray plugArray;
			surfaceShader.connectedTo(plugArray, true, true);
			if (plugArray.length() > 0) {
				dgMod.disconnect(plugArray[0], surfaceShader);
			}

			MPlug outColor = shaderFn.findPlug("outColor", true);
			dgMod.connect(outColor, surfaceShader);
			dgMod.doIt();
		}
	}

	// Apply skinning and partitions if present
	if (shape != nullptr) {
		ApplySkinning(shape, meshObj);
		ApplyDismemberPartitions(shape, meshObj);
	}
	
	LogMessage("[NiflyImporter] Created mesh: " + std::string(mayaName.asChar()) + 
			   " with " + std::to_string(numVerts) + " verts and " + std::to_string(numTris) + " tris");
	
	return MStatus::kSuccess;
}

MStatus NiflyImporter::CreateMayaMaterial(const NiflyShapeData& shapeData, MObject& outMaterial) {
	MStatus status;
	
	MFnPhongShader shaderFn;
	outMaterial = shaderFn.create(&status);
	if (status != MStatus::kSuccess) {
		return status;
	}
	
	// Set shader name based on shape
	MString shaderName = MakeMayaName(shapeData.name) + "_mat";
	shaderFn.setName(shaderName);
	
	// Set shader properties
	shaderFn.findPlug("reflectivity", true).setFloat(shapeData.specularStrength);
	shaderFn.setCosPower(shapeData.glossiness);
	
	if (shapeData.hasAlpha) {
		shaderFn.setTransparency(MColor(1.0f - shapeData.alpha, 1.0f - shapeData.alpha, 1.0f - shapeData.alpha));
	}
	
	// Create and connect diffuse texture if present
	if (!shapeData.diffuseTexture.empty()) {
		MString texturePath = ResolveTexturePath(shapeData.diffuseTexture);
		LogMessage("[NiflyImporter] Resolved diffuse texture: " + std::string(texturePath.asChar()));
		
		MFnDependencyNode fileNode;
		MObject fileObj = fileNode.create("file", shaderName + "_diffuse");
		fileNode.findPlug("ftn", true).setValue(texturePath);
		
		// Add to texture list
	MItDependencyNodes nodeIt(MFn::kTextureList);
	MFnDependencyNode texListFn(nodeIt.thisNode());
		MPlug textures = texListFn.findPlug("textures", true);
		
		int next = 0;
		while (textures.elementByLogicalIndex(next).isConnected()) next++;
		
		MDGModifier dgMod;
		dgMod.connect(fileNode.findPlug("message", true), textures.elementByLogicalIndex(next));
		dgMod.connect(fileNode.findPlug("outColor", true), shaderFn.findPlug("color", true));
		
		if (shapeData.hasAlpha) {
			dgMod.connect(fileNode.findPlug("outTransparency", true), shaderFn.findPlug("transparency", true));
		}
		
		// Create place2dTexture and connect
		MFnDependencyNode place2dNode;
		MObject place2dObj = place2dNode.create("place2dTexture", shaderName + "_place2d");
		
		dgMod.connect(place2dNode.findPlug("coverage", true), fileNode.findPlug("coverage", true));
		dgMod.connect(place2dNode.findPlug("outUV", true), fileNode.findPlug("uvCoord", true));
		dgMod.connect(place2dNode.findPlug("outUvFilterSize", true), fileNode.findPlug("uvFilterSize", true));
		dgMod.connect(place2dNode.findPlug("repeatUV", true), fileNode.findPlug("repeatUV", true));
		dgMod.connect(place2dNode.findPlug("wrapU", true), fileNode.findPlug("wrapU", true));
		dgMod.connect(place2dNode.findPlug("wrapV", true), fileNode.findPlug("wrapV", true));
		
		dgMod.doIt();
	}
	
	// Create and connect normal map if present
	if (!shapeData.normalTexture.empty()) {
		MString texturePath = ResolveTexturePath(shapeData.normalTexture);
		LogMessage("[NiflyImporter] Resolved normal texture: " + std::string(texturePath.asChar()));
		
		MFnDependencyNode fileNode;
		MObject fileObj = fileNode.create("file", shaderName + "_normal");
		fileNode.findPlug("ftn", true).setValue(texturePath);
		
		// Create bump2d node
		MFnDependencyNode bump2dNode;
		MObject bump2dObj = bump2dNode.create("bump2d", shaderName + "_bump");
		bump2dNode.findPlug("bumpInterp", true).setInt(1); // Use tangent space normals
		
		MDGModifier dgMod;
		dgMod.connect(fileNode.findPlug("outAlpha", true), bump2dNode.findPlug("bumpValue", true));
		dgMod.connect(bump2dNode.findPlug("outNormal", true), shaderFn.findPlug("normalCamera", true));
		dgMod.doIt();
	}
	
	return MStatus::kSuccess;
}

MObject NiflyImporter::GetOrCreateJoint(const std::string& boneName) {
	auto it = jointMap.find(boneName);
	if (it != jointMap.end()) {
		return it->second;
	}

	const nifly::NifFile* boneSource = GetBoneSource();
	nifly::NiNode* boneNode = boneSource ? boneSource->FindBlockByName<nifly::NiNode>(boneName) : nullptr;
	if (!boneNode && boneSource != nifFile.get()) {
		boneNode = nifFile->FindBlockByName<nifly::NiNode>(boneName);
	}
	if (!boneNode) {
		LogError("[NiflyImporter] Bone not found in NIF: " + boneName);
		return MObject::kNullObj;
	}

	nifly::NiNode* parentNode = boneSource ? boneSource->GetParentNode(boneNode) : nullptr;
	MObject parentObj = MObject::kNullObj;
	if (parentNode) {
		parentObj = GetOrCreateJoint(parentNode->name.get());
	}
	if (parentObj.isNull() && !rootTransform.isNull()) {
		parentObj = rootTransform;
	}

	MFnIkJoint jointFn;
	MObject jointObj = jointFn.create(parentObj);
	jointFn.setName(MakeMayaName(boneName));

	nifly::MatTransform xform;
	if (boneSource && boneSource->GetNodeTransformToParent(boneName, xform)) {
		MTransformationMatrix tm = MakeMayaTransform(xform, importOptions.importScale);
		jointFn.set(tm);
	}

	jointMap[boneName] = jointObj;
	return jointObj;
}

bool NiflyImporter::ShouldUseReferenceSkeleton(const std::vector<std::string>& boneNames) const {
	if (boneNames.empty()) {
		return false;
	}
	size_t missing = 0;
	for (const auto& name : boneNames) {
		if (name.empty()) {
			continue;
		}
		if (!nifFile->FindBlockByName<nifly::NiNode>(name)) {
			++missing;
		}
	}
	return missing > 0;
}

const nifly::NifFile* NiflyImporter::GetBoneSource() const {
	if (useReferenceSkeleton && referenceSkeleton && referenceSkeleton->IsValid()) {
		return referenceSkeleton.get();
	}
	return nifFile.get();
}

void NiflyImporter::LoadReferenceSkeleton() {
	if (referenceSkeleton || importFileDir.empty()) {
		return;
	}

	std::filesystem::path nifPath(importFileDir);
	std::vector<std::filesystem::path> candidateDirs;
	candidateDirs.push_back(nifPath);

	// Look for a "meshes" folder in the path to build a root search
	std::filesystem::path current = nifPath;
	while (current.has_parent_path()) {
		if (current.filename().string() == "meshes") {
			std::filesystem::path root = current.parent_path();
			candidateDirs.push_back(root / "meshes" / "actors" / "character" / "character assets");
			break;
		}
		current = current.parent_path();
	}

	std::vector<std::string> filenames;
	const std::string lowerPath = nifPath.string();
	const bool preferFemale = (lowerPath.find("female") != std::string::npos);

	if (preferFemale) {
		filenames = {"skeleton_female.nif", "skeleton.nif", "skeletonbeast_female.nif", "skeletonbeast.nif"};
	} else {
		filenames = {"skeleton.nif", "skeleton_female.nif", "skeletonbeast.nif", "skeletonbeast_female.nif"};
	}

	for (const auto& dir : candidateDirs) {
		for (const auto& file : filenames) {
			std::filesystem::path candidate = dir / file;
			if (!std::filesystem::exists(candidate)) {
				continue;
			}
			referenceSkeleton = std::make_unique<nifly::NifFile>();
			nifly::NifLoadOptions options;
			int result = referenceSkeleton->Load(candidate, options);
			if (result == 0 && referenceSkeleton->IsValid()) {
				LogMessage("[NiflyImporter] Loaded reference skeleton: " + candidate.string());
				return;
			}
			referenceSkeleton.reset();
		}
	}
}

void NiflyImporter::BuildJointHierarchy() {
	const nifly::NifFile* boneSource = GetBoneSource();
	if (!boneSource) {
		return;
	}

	auto* rootNode = boneSource->GetRootNode();
	if (!rootNode) {
		return;
	}

	std::unordered_set<std::string> visited;
	size_t created = 0;

	std::function<void(nifly::NiNode*, const MObject&, const std::string&)> visit =
		[&](nifly::NiNode* node, const MObject& parentJoint, const std::string& parentName) {
			if (!node) {
				return;
			}
			const std::string name = node->name.get();
			if (name.empty()) {
				return;
			}

			MObject jointObj = MObject::kNullObj;
			auto it = jointMap.find(name);
			if (it != jointMap.end()) {
				jointObj = it->second;
			} else {
				MFnIkJoint jointFn;
				MObject parentObj = parentJoint.isNull() ? rootTransform : parentJoint;
				jointObj = jointFn.create(parentObj);
				jointFn.setName(MakeMayaName(name));

				nifly::MatTransform xform;
				if (useReferenceSkeleton) {
					nifly::MatTransform nodeGlobal;
					nifly::MatTransform parentGlobal;
					const bool hasNodeGlobal = boneSource->GetNodeTransformToGlobal(name, nodeGlobal);
					const bool hasParentGlobal = (!parentName.empty()) && boneSource->GetNodeTransformToGlobal(parentName, parentGlobal);
					if (hasNodeGlobal) {
						nifly::MatTransform localXf = nodeGlobal;
						if (hasParentGlobal) {
							localXf = parentGlobal.InverseTransform().ComposeTransforms(nodeGlobal);
						}
						jointFn.set(MakeMayaTransform(localXf, importOptions.importScale));
					} else if (boneSource->GetNodeTransformToParent(name, xform)) {
						jointFn.set(MakeMayaTransform(xform, importOptions.importScale));
					}
				} else if (boneSource->GetNodeTransformToParent(name, xform)) {
					jointFn.set(MakeMayaTransform(xform, importOptions.importScale));
				}

				jointMap[name] = jointObj;
				++created;
			}

			visited.insert(name);

			auto children = boneSource->GetChildren<nifly::NiNode>(node);
			for (auto* child : children) {
				visit(child, jointObj, name);
			}
		};

	visit(rootNode, MObject::kNullObj, std::string());

	// Add any loose nodes not reachable from the root as children of rootTransform.
	auto nodes = boneSource->GetNodes();
	for (auto* node : nodes) {
		if (!node) {
			continue;
		}
		const std::string name = node->name.get();
		if (name.empty() || visited.count(name) != 0) {
			continue;
		}
		MFnIkJoint jointFn;
		MObject jointObj = jointFn.create(rootTransform);
		jointFn.setName(MakeMayaName(name));

		nifly::MatTransform xform;
		if (boneSource->GetNodeTransformToGlobal(name, xform)) {
			jointFn.set(MakeMayaTransform(xform, importOptions.importScale));
		} else if (boneSource->GetNodeTransformToParent(name, xform)) {
			jointFn.set(MakeMayaTransform(xform, importOptions.importScale));
		}

		jointMap[name] = jointObj;
		++created;
	}

	LogMessage("[NiflyImporter] Built joint hierarchy: " + std::to_string(created) + " joints");
}

MStatus NiflyImporter::ApplySkinning(nifly::NiShape* shape, const MObject& meshObj) {
	if (!shape || meshObj.isNull()) {
		return MStatus::kFailure;
	}

	std::vector<std::string> boneNames;
	if (nifFile->GetShapeBoneList(shape, boneNames) == 0 || boneNames.empty()) {
		return MStatus::kSuccess;
	}

	std::vector<MDagPath> jointPaths;
	jointPaths.reserve(boneNames.size());
	for (const auto& boneName : boneNames) {
		MObject jointObj = GetOrCreateJoint(boneName);
		if (jointObj.isNull()) {
			LogError("[NiflyImporter] Failed to create joint for bone: " + boneName);
			return MStatus::kFailure;
		}

		MDagPath jointPath;
		MDagPath::getAPathTo(jointObj, jointPath);
		jointPaths.push_back(jointPath);
	}

	MDagPath meshPath;
	MDagPath::getAPathTo(meshObj, meshPath);

	std::string cmd = "skinCluster -tsb ";
	for (const auto& jointPath : jointPaths) {
		cmd.append(jointPath.fullPathName().asChar());
		cmd.append(" ");
	}
	cmd.append(meshPath.fullPathName().asChar());

	MStringArray result;
	MStatus status = MGlobal::executeCommand(cmd.c_str(), result);
	if (status != MStatus::kSuccess || result.length() == 0) {
		LogError("[NiflyImporter] Failed to create skinCluster for mesh: " + std::string(meshPath.partialPathName().asChar()));
		return MStatus::kFailure;
	}

	MSelectionList selList;
	selList.add(result[0]);
	MObject skinOb;
	selList.getDependNode(0, skinOb);
	MFnSkinCluster clusterFn(skinOb, &status);
	if (status != MStatus::kSuccess) {
		LogError("[NiflyImporter] Failed to access skinCluster node");
		return MStatus::kFailure;
	}

	if (!importOptions.importNormalizedWeights) {
		MPlug normalizePlug = clusterFn.findPlug("normalizeWeights", true, &status);
		if (status == MStatus::kSuccess) {
			normalizePlug.setInt(0);
		}
	}

	MFnMesh meshFn(meshObj, &status);
	if (status != MStatus::kSuccess) {
		return MStatus::kFailure;
	}
	const int numVerts = static_cast<int>(meshFn.numVertices());
	if (numVerts <= 0) {
		return MStatus::kFailure;
	}

	MFnSingleIndexedComponent compFn;
	MObject vertices = compFn.create(MFn::kMeshVertComponent);
	MIntArray vertexIndices(numVerts);
	for (int v = 0; v < numVerts; ++v) {
		vertexIndices[v] = v;
	}
	compFn.addElements(vertexIndices);

	std::vector<std::vector<float>> nifWeights(boneNames.size(), std::vector<float>(numVerts, 0.0f));
	for (size_t boneIndex = 0; boneIndex < boneNames.size(); ++boneIndex) {
		std::unordered_map<uint16_t, float> weights;
		nifFile->GetShapeBoneWeights(shape, static_cast<uint32_t>(boneIndex), weights);
		for (const auto& entry : weights) {
			if (entry.first < static_cast<uint16_t>(numVerts)) {
				nifWeights[boneIndex][entry.first] = entry.second;
			}
		}
	}

	MIntArray influenceList(static_cast<unsigned int>(boneNames.size()));
	for (unsigned int i = 0; i < boneNames.size(); ++i) {
		influenceList[i] = static_cast<int>(i);
	}

	MFloatArray weightList(static_cast<unsigned int>(numVerts * boneNames.size()));
	int k = 0;
	for (int v = 0; v < numVerts; ++v) {
		for (size_t b = 0; b < boneNames.size(); ++b) {
			weightList[k++] = nifWeights[b][v];
		}
	}

	clusterFn.setWeights(meshPath, vertices, influenceList, weightList, importOptions.importNormalizedWeights);
	LogMessage("[NiflyImporter] Applied skinning: bones=" + std::to_string(boneNames.size()) + " verts=" + std::to_string(numVerts));
	return MStatus::kSuccess;
}

void NiflyImporter::ApplyDismemberPartitions(nifly::NiShape* shape, const MObject& meshObj) {
	if (!shape || meshObj.isNull()) {
		return;
	}

	nifly::NiVector<nifly::BSDismemberSkinInstance::PartitionInfo> partitionInfo;
	std::vector<int> triParts;
	if (!nifFile->GetShapePartitions(shape, partitionInfo, triParts)) {
		return;
	}
	if (partitionInfo.empty() || triParts.empty()) {
		return;
	}

	MFnMesh meshFn(meshObj);
	meshFn.updateSurface();

	MStringArray longName;
	MStringArray shortName;
	MStringArray formatName;
	longName.append("dismemberValue");
	shortName.append("dV");
	formatName.append("int");

	MDagPath meshPath;
	MDagPath::getAPathTo(meshObj, meshPath);

	int blindDataId = 0;
	for (size_t partIndex = 0; partIndex < partitionInfo.size(); ++partIndex) {
		MSelectionList selectedFaces;
		MStatus status;
		do {
			blindDataId++;
			status = meshFn.createBlindDataType(blindDataId, longName, shortName, formatName);
		} while (status == MStatus::kFailure);

		MItMeshPolygon polygonIt(meshObj);
		selectedFaces.add(meshPath);
		for (size_t t = 0; t < triParts.size() && !polygonIt.isDone(); ++t, polygonIt.next()) {
			if (triParts[t] == static_cast<int>(partIndex)) {
				selectedFaces.add(meshPath, polygonIt.currentItem());
			}
		}

		MString melCommand = "polyBlindData -id ";
		melCommand += blindDataId;
		melCommand += " -associationType \"face\" -ldn \"dismemberValue\" -ind 1";
		MGlobal::clearSelectionList();
		MGlobal::setActiveSelectionList(selectedFaces);
		MGlobal::executeCommand(melCommand);

		meshFn.updateSurface();

		MDGModifier dgModifier;
		MFnDependencyNode dismemberNode;
		dismemberNode.create("nifDismemberPartition");

		MPlug inputMessageDismember = dismemberNode.findPlug("targetFaces");
		MPlug inputMessageShape = dismemberNode.findPlug("targetShape");

		MPlug meshOutMessage = meshFn.findPlug("message");
		dgModifier.connect(meshOutMessage, inputMessageShape);
		dgModifier.doIt();

		const auto& info = partitionInfo[partIndex];
		MStringArray bodyPartsStrings = NifDismemberPartition::bodyPartTypeToStringArray(
			static_cast<BSDismemberBodyPartType>(info.partID));
		MStringArray partsStrings = NifDismemberPartition::partToStringArray(
			static_cast<BSPartFlag>(info.flags));

		melCommand = "setAttr ";
		melCommand += (dismemberNode.name() + ".bodyPartsFlags");
		melCommand += " -type \"stringArray\" ";
		melCommand += bodyPartsStrings.length();
		for (unsigned int z = 0; z < bodyPartsStrings.length(); ++z) {
			melCommand += (" \"" + bodyPartsStrings[z] + "\"");
		}
		MGlobal::executeCommand(melCommand);

		melCommand = "setAttr ";
		melCommand += (dismemberNode.name() + ".partsFlags");
		melCommand += " -type \"stringArray\" ";
		melCommand += partsStrings.length();
		for (unsigned int z = 0; z < partsStrings.length(); ++z) {
			melCommand += (" \"" + partsStrings[z] + "\"");
		}
		MGlobal::executeCommand(melCommand);

		MPlugArray connections;
		MPlug inputMesh = meshFn.findPlug("inMesh");
		MPlug outMesh;
		MPlug typeId;
		MFnDependencyNode nodeOutMesh;

		inputMesh.connectedTo(connections, true, false);
		if (connections.length() > 0) {
			outMesh = connections[0];
			nodeOutMesh.setObject(outMesh.node());
			typeId = nodeOutMesh.findPlug("typeId");
		}

		while (!typeId.isNull() && typeId.asInt() != blindDataId) {
			inputMesh = nodeOutMesh.findPlug("inMesh");
			connections.clear();
			inputMesh.connectedTo(connections, true, false);
			if (connections.length() == 0) {
				break;
			}
			outMesh = connections[0];
			nodeOutMesh.setObject(outMesh.node());
			typeId = nodeOutMesh.findPlug("typeId");
		}

		if (!nodeOutMesh.object().isNull()) {
			MPlug message = nodeOutMesh.findPlug("message");
			dgModifier.connect(message, inputMessageDismember);
			dgModifier.doIt();
		}
	}
}

MString NiflyImporter::ResolveTexturePath(const std::string& texturePath) {
	if (texturePath.empty()) return MString();
	
	std::string path = texturePath;
	// Replace backslashes with forward slashes
	for (char& c : path) {
		if (c == '\\') c = '/';
	}

	// If already absolute, return as-is when it exists
	try {
		std::filesystem::path absPath(path);
		if (absPath.is_absolute() && std::filesystem::exists(absPath)) {
			return MString(absPath.lexically_normal().string().c_str());
		}
	} catch (...) {
		// ignore filesystem errors
	}

	// Strip any leading "./" and normalize texture prefix
	while (path.rfind("./", 0) == 0) {
		path.erase(0, 2);
	}

	// If path already includes textures/, keep the subpath after it
	std::string texturesSubpath = path;
	const std::string texturesPrefix = "textures/";
	auto pos = texturesSubpath.find(texturesPrefix);
	if (pos != std::string::npos) {
		texturesSubpath = texturesSubpath.substr(pos + texturesPrefix.size());
	}
	
	// Prefer user-configured texture paths from the importer dialog
	if (!importOptions.texturePath.empty()) {
		MStringArray paths;
		MString(importOptions.texturePath.c_str()).split('|', paths);

		for (unsigned int i = 0; i < paths.length(); i++) {
			std::string basePath = paths[i].asChar();

			// Absolute base path + full path
			try {
				std::filesystem::path base(basePath);
				std::filesystem::path direct = base / path;
				if (std::filesystem::exists(direct)) {
					return MString(direct.lexically_normal().string().c_str());
				}

				std::filesystem::path withTextures = base / "textures" / texturesSubpath;
				if (std::filesystem::exists(withTextures)) {
					return MString(withTextures.lexically_normal().string().c_str());
				}
			} catch (...) {
				// ignore filesystem errors
			}
		}
	}

	MFileObject mFile;
	mFile.setRawName(MString(path.c_str()));
	
	// Check if file exists directly (relative to current Maya project)
	if (mFile.exists()) {
		return mFile.resolvedFullName();
	}
	
	// Check relative to import file
	if (!importFileDir.empty()) {
		std::string fullPath = importFileDir + "/" + path;
		mFile.setRawName(MString(fullPath.c_str()));
		if (mFile.exists()) {
			return mFile.resolvedFullName();
		}

		// If under a meshes/ folder, try sibling textures/ folder
		std::string lowerDir = importFileDir;
		std::transform(lowerDir.begin(), lowerDir.end(), lowerDir.begin(), ::tolower);
		const std::string meshesToken = "/meshes/";
		size_t meshPos = lowerDir.find(meshesToken);
		if (meshPos != std::string::npos) {
			std::string rootPath = importFileDir.substr(0, meshPos);
			std::string textureRoot = rootPath + "/textures/" + texturesSubpath;
			mFile.setRawName(MString(textureRoot.c_str()));
			if (mFile.exists()) {
				return mFile.resolvedFullName();
			}
		}

		// Walk up to find a textures/ root
		try {
			std::filesystem::path baseDir(importFileDir);
			for (int i = 0; i < 6; ++i) {
				std::filesystem::path candidate = baseDir / "textures" / texturesSubpath;
				if (std::filesystem::exists(candidate)) {
					return MString(candidate.lexically_normal().string().c_str());
				}
				if (!baseDir.has_parent_path()) break;
				baseDir = baseDir.parent_path();
			}
		} catch (...) {
			// ignore filesystem errors
		}
	}
	
	// Return original path if not found
	LogMessage("[NiflyImporter] Texture not found: " + path);
	return MString(path.c_str());
}

MString NiflyImporter::MakeMayaName(const std::string& nifName) {
	std::ostringstream result;
	
	// Prefix with "nif_" if name starts with digit
	if (!nifName.empty() && std::isdigit(static_cast<unsigned char>(nifName[0]))) {
		result << "nif_";
	}
	
	for (char c : nifName) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			result << c;
		} else {
			result << '_';
		}
	}
	
	std::string name = result.str();
	if (name.empty()) name = "unnamed";
	
	return MString(name.c_str());
}

void NiflyImporter::LogMessage(const std::string& message) {
	AppendLog(message);
}

void NiflyImporter::LogError(const std::string& message) {
	lastError = message;
	AppendLog("[ERROR] " + message);
	MGlobal::displayError(MString(message.c_str()));
}
