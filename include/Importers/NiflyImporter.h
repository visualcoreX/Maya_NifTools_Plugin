#pragma once

// NiflyImporter - Mesh import using nifly library for Skyrim SE/FO4 NIFs
// This replaces the problematic niflib-based BSTriShape parsing
//
// IMPORTANT: This header must NOT include any niflib headers directly or indirectly
// to avoid symbol conflicts between niflib and nifly libraries.

#include <maya/MObject.h>
#include <maya/MDagPath.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MFileObject.h>
#include <maya/MStatus.h>

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// Forward declarations for nifly types
namespace nifly {
	class NifFile;
	class NiShape;
	class BSTriShape;
	class NiNode;
	class NiShader;
	class NiAlphaProperty;
	struct Vector3;
	struct Vector2;
	struct Triangle;
	struct Color4;
}

// Note: This header is intentionally kept isolated from the niflib-based translator types
// to avoid symbol conflicts. Use NiflyImportOptions instead of NifTranslatorOptions.

// Represents imported shape data
struct NiflyShapeData {
	std::string name;
	std::vector<float> vertices;      // x,y,z triplets
	std::vector<float> normals;       // x,y,z triplets
	std::vector<float> uvs;           // u,v pairs
	std::vector<float> colors;        // r,g,b,a quads
	std::vector<unsigned int> triangles;  // 3 indices per triangle
	
	// Material info
	std::string diffuseTexture;
	std::string normalTexture;
	std::string glowTexture;
	float specularStrength = 1.0f;
	float glossiness = 100.0f;
	float alpha = 1.0f;
	bool hasAlpha = false;
};

// Represents transform data
struct NiflyTransform {
	float translation[3] = {0, 0, 0};
	float rotation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // 3x3 matrix as row-major
	float scale = 1.0f;
};

// Import options - simple struct to pass options without including niflib-polluted headers
struct NiflyImportOptions {
	float importScale = 1.0f;
	std::string texturePath;
	std::string importFileDir;
	bool useNameMangling = false;
	bool importNormalizedWeights = true;
};

// Main importer class using nifly
class NiflyImporter {
public:
	NiflyImporter();
	~NiflyImporter();

	// Check if a file should be handled by nifly (Skyrim SE, FO4, etc.)
	static bool ShouldUseNifly(const std::string& filename);
	
	// Set options for import
	void SetOptions(const NiflyImportOptions& options);
	
	// Import the entire file
	MStatus Import(const MFileObject& file);
	
	// Get the last error message
	std::string GetLastError() const { return lastError; }

private:
	NiflyImportOptions importOptions;
	std::unique_ptr<nifly::NifFile> nifFile;
	std::string lastError;
	std::string importFileDir;
	
	// Import helpers
	MStatus ImportShapes();
	MStatus ImportShape(nifly::NiShape* shape, const MObject& parentTransform);
	MStatus CreateMayaMesh(const NiflyShapeData& shapeData, const NiflyTransform& transform, const MObject& parentTransform, nifly::NiShape* shape);
	MStatus CreateMayaMaterial(const NiflyShapeData& shapeData, MObject& outMaterial);
	MStatus ApplySkinning(nifly::NiShape* shape, const MObject& meshObj);
	void ApplyDismemberPartitions(nifly::NiShape* shape, const MObject& meshObj);
	MObject GetOrCreateJoint(const std::string& boneName);
	void BuildJointHierarchy();
	void LoadReferenceSkeleton();
	const nifly::NifFile* GetBoneSource() const;
	bool ShouldUseReferenceSkeleton(const std::vector<std::string>& boneNames) const;
	
	// Data extraction from nifly shapes
	bool ExtractShapeData(nifly::NiShape* shape, NiflyShapeData& outData);
	bool ExtractTransform(nifly::NiShape* shape, NiflyTransform& outTransform);
	
	// Texture path resolution
	MString ResolveTexturePath(const std::string& texturePath);
	
	// Utility to make valid Maya name
	MString MakeMayaName(const std::string& nifName);
	
	// Logging
	void LogMessage(const std::string& message);
	void LogError(const std::string& message);

	MObject rootTransform;
	std::unordered_map<std::string, MObject> jointMap;
	std::unique_ptr<nifly::NifFile> referenceSkeleton;
	bool useReferenceSkeleton = false;
};
