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
#include <maya/MDGModifier.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyNodes.h>
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

namespace {
	void AppendLog(const std::string& message) {
		const char* logPath = "C:\\Users\\rober\\Documents\\maya\\2025\\scripts\\nifTranslator_debug.log";
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			log << message << std::endl;
		}
		// Also output to Maya script editor
		MGlobal::displayInfo(MString(message.c_str()));
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
	
	return CreateMayaMesh(shapeData, transform, parentTransform);
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

MStatus NiflyImporter::CreateMayaMesh(const NiflyShapeData& shapeData, const NiflyTransform& transform, const MObject& parentTransform) {
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
		
		// Apply shape transform
		MFnTransform transformFnObj(transformObj);
		transformFnObj.setTranslation(MVector(
			transform.translation[0] * scale,
			transform.translation[1] * scale,
			transform.translation[2] * scale
		), MSpace::kTransform);
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

MString NiflyImporter::ResolveTexturePath(const std::string& texturePath) {
	if (texturePath.empty()) return MString();
	
	std::string path = texturePath;
	// Replace backslashes with forward slashes
	for (char& c : path) {
		if (c == '\\') c = '/';
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
	
	MFileObject mFile;
	mFile.setRawName(MString(path.c_str()));
	
	// Check if file exists directly
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
	
	// Check in configured texture paths
	if (!importOptions.texturePath.empty()) {
		MStringArray paths;
		MString(importOptions.texturePath.c_str()).split('|', paths);
		
		for (unsigned int i = 0; i < paths.length(); i++) {
			std::string basePath = paths[i].asChar();
			std::string fullPath = basePath + "/" + path;
			mFile.setRawName(MString(fullPath.c_str()));
			if (mFile.exists()) {
				return mFile.resolvedFullName();
			}

			// Also try basePath/textures/<subpath>
			std::string textureFullPath = basePath + "/textures/" + texturesSubpath;
			mFile.setRawName(MString(textureFullPath.c_str()));
			if (mFile.exists()) {
				return mFile.resolvedFullName();
			}
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
