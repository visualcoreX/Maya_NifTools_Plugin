#include "Importers/NiflyMeshImporter.h"

#include <windows.h>
#include <fstream>
#include <vector>

#include <maya/MFnMesh.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnLambertShader.h>
#include <maya/MFnSet.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MFloatArray.h>
#include <maya/MVectorArray.h>

namespace {
	void AppendNiflyLog(const std::string& message) {
		const char* logPath = "C:\\Users\\rober\\Documents\\maya\\2025\\scripts\\nifTranslator_debug.log";
		std::ofstream log(logPath, std::ios::out | std::ios::app);
		if (log.is_open()) {
			log << message << std::endl;
		}
	}

	struct NiflyApi {
		HMODULE module = nullptr;

		using load_t = void* (__cdecl *)(const char*);
		using destroy_t = void (__cdecl *)(void*);
		using getShapes_t = int (__cdecl *)(void*, void**, int, int);
		using getShapeName_t = int (__cdecl *)(void*, char*, int);
		using getShapeBlockName_t = int (__cdecl *)(void*, char*, int);
		using getVertsForShape_t = int (__cdecl *)(void*, void*, float*, int, int);
		using getNormalsForShape_t = int (__cdecl *)(void*, void*, float*, int, int);
		using getTriangles_t = int (__cdecl *)(void*, void*, std::uint16_t*, int, int);
		using getUVs_t = int (__cdecl *)(void*, void*, float*, int, int);
		using getColorsForShape_t = int (__cdecl *)(void*, void*, float*, int);
		using getShaderTextureSlot_t = int (__cdecl *)(void*, void*, int, char*, int);

		load_t load = nullptr;
		destroy_t destroy = nullptr;
		getShapes_t getShapes = nullptr;
		getShapeName_t getShapeName = nullptr;
		getShapeBlockName_t getShapeBlockName = nullptr;
		getVertsForShape_t getVertsForShape = nullptr;
		getNormalsForShape_t getNormalsForShape = nullptr;
		getTriangles_t getTriangles = nullptr;
		getUVs_t getUVs = nullptr;
		getColorsForShape_t getColorsForShape = nullptr;
		getShaderTextureSlot_t getShaderTextureSlot = nullptr;

		bool Load() {
			const char* candidates[] = {
				"NiflyDLL.dll",
				"C:\\Users\\rober\\Documents\\maya\\2025\\plug-ins\\NiflyDLL.dll",
				"C:\\Program Files\\Autodesk\\Maya2025\\bin\\NiflyDLL.dll",
				"E:\\Skyrim Animation\\SKSE\\Maya Scripts\\PyNifly-main\\NiflyDLL\\x64\\Release\\NiflyDLL.dll"
			};

			for (const char* path : candidates) {
				module = LoadLibraryA(path);
				if (module) {
					AppendNiflyLog(std::string("[NiflyMeshImporter] Loaded NiflyDLL: ") + path);
					break;
				}
			}

			if (!module) {
				AppendNiflyLog("[NiflyMeshImporter] Failed to load NiflyDLL.dll");
				return false;
			}

			load = reinterpret_cast<load_t>(GetProcAddress(module, "load"));
			destroy = reinterpret_cast<destroy_t>(GetProcAddress(module, "destroy"));
			getShapes = reinterpret_cast<getShapes_t>(GetProcAddress(module, "getShapes"));
			getShapeName = reinterpret_cast<getShapeName_t>(GetProcAddress(module, "getShapeName"));
			getShapeBlockName = reinterpret_cast<getShapeBlockName_t>(GetProcAddress(module, "getShapeBlockName"));
			getVertsForShape = reinterpret_cast<getVertsForShape_t>(GetProcAddress(module, "getVertsForShape"));
			getNormalsForShape = reinterpret_cast<getNormalsForShape_t>(GetProcAddress(module, "getNormalsForShape"));
			getTriangles = reinterpret_cast<getTriangles_t>(GetProcAddress(module, "getTriangles"));
			getUVs = reinterpret_cast<getUVs_t>(GetProcAddress(module, "getUVs"));
			getColorsForShape = reinterpret_cast<getColorsForShape_t>(GetProcAddress(module, "getColorsForShape"));
			getShaderTextureSlot = reinterpret_cast<getShaderTextureSlot_t>(GetProcAddress(module, "getShaderTextureSlot"));

			if (!load || !destroy || !getShapes || !getShapeName || !getShapeBlockName ||
				!getVertsForShape || !getNormalsForShape || !getTriangles || !getUVs || !getColorsForShape ||
				!getShaderTextureSlot) {
				AppendNiflyLog("[NiflyMeshImporter] Missing required NiflyDLL exports.");
				FreeLibrary(module);
				module = nullptr;
				return false;
			}

			return true;
		}

		void Unload() {
			if (module) {
				FreeLibrary(module);
				module = nullptr;
			}
		}
	};

	bool ResolveTexturePath(const std::string& texture,
		const NifTranslatorOptionsRef& options,
		std::string& resolved)
	{
		if (texture.empty()) {
			return false;
		}

		if (texture.find(':') != std::string::npos || texture.rfind("/", 0) == 0 || texture.rfind("\\", 0) == 0) {
			resolved = texture;
			return true;
		}

		if (!options) {
			resolved = texture;
			return true;
		}

		std::string search = options->texturePath;
		size_t start = 0;
		while (start < search.size()) {
			size_t end = search.find('|', start);
			if (end == std::string::npos) {
				end = search.size();
			}
			std::string root = search.substr(start, end - start);
			if (!root.empty() && root.back() != '/' && root.back() != '\\') {
				root += '/';
			}
			std::string candidate = root + texture;
			std::ifstream test(candidate.c_str(), std::ios::binary);
			if (test.good()) {
				resolved = candidate;
				return true;
			}
			start = end + 1;
		}

		resolved = texture;
		return true;
	}

	void AssignBasicMaterial(MObject meshObj,
		void* nifHandle,
		void* shapeHandle,
		NiflyApi& api,
		const NifTranslatorOptionsRef& options)
	{
		char texBuf[512] = {};
		if (api.getShaderTextureSlot(nifHandle, shapeHandle, 0, texBuf, sizeof(texBuf)) <= 0) {
			return;
		}

		std::string texturePath;
		if (!ResolveTexturePath(texBuf, options, texturePath)) {
			return;
		}

		MStatus status;
		MFnLambertShader shaderFn;
		MObject shaderObj = shaderFn.create(&status);
		if (status != MS::kSuccess) {
			return;
		}

		MFnDependencyNode fileNode;
		MObject fileObj = fileNode.create("file", "nifDiffuseFile", &status);
		if (status != MS::kSuccess) {
			return;
		}
		fileNode.findPlug("fileTextureName").setString(MString(texturePath.c_str()));

		MFnDependencyNode placeNode;
		MObject placeObj = placeNode.create("place2dTexture", "nifPlace2d", &status);
		if (status == MS::kSuccess) {
			static const char* plugPairs[][2] = {
				{"coverage", "coverage"},
				{"translateFrame", "translateFrame"},
				{"rotateFrame", "rotateFrame"},
				{"mirrorU", "mirrorU"},
				{"mirrorV", "mirrorV"},
				{"stagger", "stagger"},
				{"wrapU", "wrapU"},
				{"wrapV", "wrapV"},
				{"repeatUV", "repeatUV"},
				{"offset", "offset"},
				{"rotateUV", "rotateUV"},
				{"noiseUV", "noiseUV"},
				{"vertexUvOne", "vertexUvOne"},
				{"vertexUvTwo", "vertexUvTwo"},
				{"vertexUvThree", "vertexUvThree"},
				{"vertexCameraOne", "vertexCameraOne"},
				{"outUV", "uvCoord"},
				{"outUvFilterSize", "uvFilterSize"}
			};
			for (const auto& pair : plugPairs) {
				MPlug src = placeNode.findPlug(pair[0]);
				MPlug dst = fileNode.findPlug(pair[1]);
				if (!src.isNull() && !dst.isNull()) {
					MDGModifier dg;
					dg.connect(src, dst);
					dg.doIt();
				}
			}
		}

		{
			MDGModifier dg;
			dg.connect(fileNode.findPlug("outColor"), shaderFn.findPlug("color"));
			dg.doIt();
		}

		MFnSet sgFn;
		MSelectionList emptyList;
		MObject sgObj = sgFn.create(emptyList, MFnSet::kRenderableOnly, &status);
		if (status != MS::kSuccess) {
			return;
		}
		sgFn.setName("nifShadingGroup");

		{
			MDGModifier dg;
			dg.connect(shaderFn.findPlug("outColor"), sgFn.findPlug("surfaceShader"));
			dg.doIt();
		}

		sgFn.addMember(meshObj);
	}

	MStatus BuildMeshFromNifly(void* nifHandle,
		void* shapeHandle,
		NiflyApi& api,
		const NifTranslatorOptionsRef& options,
		const NifTranslatorUtilsRef& utils,
		int shapeIndex)
	{
		float vertSample[3] = { 0.0f, 0.0f, 0.0f };
		std::uint16_t triSample[3] = { 0, 0, 0 };

		const int vertCount = api.getVertsForShape(nifHandle, shapeHandle, vertSample, 3, 0);
		const int triCount = api.getTriangles(nifHandle, shapeHandle, triSample, 3, 0);

		if (vertCount <= 0 || triCount <= 0) {
			return MStatus::kFailure;
		}

		std::vector<float> verts(static_cast<size_t>(vertCount) * 3);
		api.getVertsForShape(nifHandle, shapeHandle, verts.data(), static_cast<int>(verts.size()), 0);

		std::vector<std::uint16_t> tris(static_cast<size_t>(triCount) * 3);
		api.getTriangles(nifHandle, shapeHandle, tris.data(), static_cast<int>(tris.size()), 0);

		int uvCount = api.getUVs(nifHandle, shapeHandle, vertSample, 2, 0);
		std::vector<float> uvs;
		if (uvCount == vertCount) {
			uvs.resize(static_cast<size_t>(uvCount) * 2);
			api.getUVs(nifHandle, shapeHandle, uvs.data(), static_cast<int>(uvs.size()), 0);
		}

		int normalCount = api.getNormalsForShape(nifHandle, shapeHandle, vertSample, 3, 0);
		std::vector<float> normals;
		if (normalCount == vertCount) {
			normals.resize(static_cast<size_t>(normalCount) * 3);
			api.getNormalsForShape(nifHandle, shapeHandle, normals.data(), static_cast<int>(normals.size()), 0);
		}

		float colorSample[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		int colorCount = api.getColorsForShape(nifHandle, shapeHandle, colorSample, 1);
		std::vector<float> colors;
		if (colorCount == vertCount) {
			colors.resize(static_cast<size_t>(colorCount) * 4);
			api.getColorsForShape(nifHandle, shapeHandle, colors.data(), static_cast<int>(colors.size()));
		}

		MPointArray mayaVerts(static_cast<unsigned int>(vertCount));
		const float scale = options ? options->importScale : 1.0f;
		for (int i = 0; i < vertCount; ++i) {
			const size_t base = static_cast<size_t>(i) * 3;
			mayaVerts[static_cast<unsigned int>(i)] = MPoint(
				verts[base] * scale,
				verts[base + 1] * scale,
				verts[base + 2] * scale,
				0.0f);
		}

		MIntArray polyCounts;
		MIntArray connects;
		polyCounts.setLength(static_cast<unsigned int>(triCount));
		connects.setLength(static_cast<unsigned int>(triCount) * 3);

		unsigned int faceIndex = 0;
		unsigned int connIndex = 0;
		for (int i = 0; i < triCount; ++i) {
			const std::uint16_t p0 = tris[static_cast<size_t>(i) * 3];
			const std::uint16_t p1 = tris[static_cast<size_t>(i) * 3 + 1];
			const std::uint16_t p2 = tris[static_cast<size_t>(i) * 3 + 2];
			if (p0 == p1 || p0 == p2 || p1 == p2) {
				continue;
			}
			if (p0 >= vertCount || p1 >= vertCount || p2 >= vertCount) {
				continue;
			}
			polyCounts[faceIndex++] = 3;
			connects[connIndex++] = static_cast<int>(p0);
			connects[connIndex++] = static_cast<int>(p1);
			connects[connIndex++] = static_cast<int>(p2);
		}

		if (faceIndex == 0) {
			return MStatus::kFailure;
		}

		polyCounts.setLength(faceIndex);
		connects.setLength(faceIndex * 3);

		MFnMesh meshFn;
		MStatus stat;
		MObject meshObj = meshFn.create(
			static_cast<int>(mayaVerts.length()),
			static_cast<int>(polyCounts.length()),
			mayaVerts,
			polyCounts,
			connects,
			MObject::kNullObj,
			&stat);

		if (stat != MS::kSuccess) {
			return stat;
		}

		if (!normals.empty()) {
			MVectorArray mayaNormals(static_cast<unsigned int>(vertCount));
			MIntArray normalIds(static_cast<unsigned int>(vertCount));
			for (int i = 0; i < vertCount; ++i) {
				const size_t base = static_cast<size_t>(i) * 3;
				mayaNormals[static_cast<unsigned int>(i)] = MVector(
					normals[base],
					normals[base + 1],
					normals[base + 2]);
				normalIds[static_cast<unsigned int>(i)] = i;
			}
			meshFn.setVertexNormals(mayaNormals, normalIds);
		}

		if (!uvs.empty()) {
			MFloatArray uArr(static_cast<unsigned int>(vertCount));
			MFloatArray vArr(static_cast<unsigned int>(vertCount));
			for (int i = 0; i < vertCount; ++i) {
				const size_t base = static_cast<size_t>(i) * 2;
				uArr[static_cast<unsigned int>(i)] = uvs[base];
				vArr[static_cast<unsigned int>(i)] = 1.0f - uvs[base + 1];
			}
			meshFn.setUVs(uArr, vArr);
			MIntArray uvCounts;
			MIntArray uvIds;
			uvCounts.setLength(faceIndex);
			uvIds.setLength(faceIndex * 3);
			for (unsigned int i = 0; i < faceIndex; ++i) {
				uvCounts[i] = 3;
				uvIds[i * 3] = connects[i * 3];
				uvIds[i * 3 + 1] = connects[i * 3 + 1];
				uvIds[i * 3 + 2] = connects[i * 3 + 2];
			}
			meshFn.assignUVs(uvCounts, uvIds);
		}

		if (!colors.empty()) {
			MColorArray mayaColors;
			MIntArray faceList;
			MIntArray vertList;
			mayaColors.setLength(faceIndex * 3);
			faceList.setLength(faceIndex * 3);
			vertList.setLength(faceIndex * 3);
			unsigned int idx = 0;
			for (unsigned int f = 0; f < faceIndex; ++f) {
				for (int v = 0; v < 3; ++v) {
					const int vertId = connects[f * 3 + v];
					const size_t cbase = static_cast<size_t>(vertId) * 4;
					mayaColors[idx] = MColor(
						colors[cbase],
						colors[cbase + 1],
						colors[cbase + 2],
						colors[cbase + 3]);
					faceList[idx] = static_cast<int>(f);
					vertList[idx] = vertId;
					++idx;
				}
			}
			meshFn.setFaceVertexColors(mayaColors, faceList, vertList);
		}

		char shapeName[256] = {};
		std::string resolvedName;
		if (api.getShapeName(shapeHandle, shapeName, sizeof(shapeName)) > 0 && utils) {
			resolvedName = utils->MakeMayaName(shapeName).asChar();
		}
		if (resolvedName.empty()) {
			resolvedName = "nifShape_" + std::to_string(shapeIndex);
		}
		if (!resolvedName.empty() && std::isdigit(resolvedName[0])) {
			resolvedName = "nif_" + resolvedName;
		}

		MFnDagNode dagFn(meshObj);
		dagFn.setName(resolvedName.c_str());

		AssignBasicMaterial(meshObj, nifHandle, shapeHandle, api, options);

		return MStatus::kSuccess;
	}
}

MStatus NiflyMeshImporter::ImportFile(const MFileObject& file,
	const NifTranslatorOptionsRef& options,
	const NifTranslatorUtilsRef& utils)
{
	NiflyApi api;
	if (!api.Load()) {
		return MStatus::kFailure;
	}

	void* nifHandle = api.load(file.fullName().asChar());
	if (!nifHandle) {
		AppendNiflyLog("[NiflyMeshImporter] Nifly load failed.");
		api.Unload();
		return MStatus::kFailure;
	}

	int shapeCount = api.getShapes(nifHandle, nullptr, 0, 0);
	if (shapeCount <= 0) {
		AppendNiflyLog("[NiflyMeshImporter] No shapes found.");
		api.destroy(nifHandle);
		api.Unload();
		return MStatus::kFailure;
	}

	std::vector<void*> shapes(static_cast<size_t>(shapeCount));
	api.getShapes(nifHandle, shapes.data(), shapeCount, 0);

	AppendNiflyLog("[NiflyMeshImporter] Importing shapes via NiflyDLL.");
	for (int i = 0; i < shapeCount; ++i) {
		char blockName[128] = {};
		if (api.getShapeBlockName(shapes[static_cast<size_t>(i)], blockName, sizeof(blockName)) > 0) {
			AppendNiflyLog(std::string("[NiflyMeshImporter] Shape block: ") + blockName);
		}
		MStatus stat = BuildMeshFromNifly(nifHandle, shapes[static_cast<size_t>(i)], api, options, utils, i);
		if (stat != MS::kSuccess) {
			AppendNiflyLog("[NiflyMeshImporter] Failed to build mesh from shape.");
		}
	}

	api.destroy(nifHandle);
	api.Unload();
	return MStatus::kSuccess;
}

