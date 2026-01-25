#include "include/Exporters/NiflyExportingFixture.h"

#include <maya/MItDag.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MGlobal.h>
#include <maya/MTypes.h>
#include <maya/MMatrix.h>
#include <maya/MPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <algorithm>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>

#include "NifFile.hpp"
#include "Geometry.hpp"
#include "Nodes.hpp"
#include "Shaders.hpp"

#include <unordered_set>

namespace {
	nifly::MatTransform ToNiflyTransform(const MTransformationMatrix& mayaXform) {
		nifly::MatTransform out;
		MVector translation = mayaXform.getTranslation(MSpace::kTransform);
		out.translation.x = static_cast<float>(translation.x);
		out.translation.y = static_cast<float>(translation.y);
		out.translation.z = static_cast<float>(translation.z);

		MMatrix rotMatrix = mayaXform.asMatrix();
		out.rotation[0][0] = static_cast<float>(rotMatrix(0, 0));
		out.rotation[0][1] = static_cast<float>(rotMatrix(0, 1));
		out.rotation[0][2] = static_cast<float>(rotMatrix(0, 2));
		out.rotation[1][0] = static_cast<float>(rotMatrix(1, 0));
		out.rotation[1][1] = static_cast<float>(rotMatrix(1, 1));
		out.rotation[1][2] = static_cast<float>(rotMatrix(1, 2));
		out.rotation[2][0] = static_cast<float>(rotMatrix(2, 0));
		out.rotation[2][1] = static_cast<float>(rotMatrix(2, 1));
		out.rotation[2][2] = static_cast<float>(rotMatrix(2, 2));

		double scale[3] = {1.0, 1.0, 1.0};
		mayaXform.getScale(scale, MSpace::kTransform);
		out.scale = static_cast<float>(scale[0]);
		return out;
	}

	std::string GetFileTextureFromShader(const MObject& shaderObj, const char* plugName) {
		MFnDependencyNode shaderFn(shaderObj);
		MPlug plug = shaderFn.findPlug(plugName, true);
		MPlugArray connections;
		plug.connectedTo(connections, true, false);
		for (unsigned int i = 0; i < connections.length(); ++i) {
			MObject node = connections[i].node();
			MFnDependencyNode nodeFn(node);
			if (nodeFn.typeName() == "file") {
				MPlug ftn = nodeFn.findPlug("fileTextureName", true);
				MString path;
				ftn.getValue(path);
				return path.asChar();
			}
			if (nodeFn.typeName() == "bump2d" || nodeFn.typeName() == "bump3d") {
				MPlug bumpValue = nodeFn.findPlug("bumpValue", true);
				MPlugArray bumpConnections;
				bumpValue.connectedTo(bumpConnections, true, false);
				for (unsigned int j = 0; j < bumpConnections.length(); ++j) {
					MObject bumpNode = bumpConnections[j].node();
					MFnDependencyNode bumpFn(bumpNode);
					if (bumpFn.typeName() == "file") {
						MPlug ftn = bumpFn.findPlug("fileTextureName", true);
						MString path;
						ftn.getValue(path);
						return path.asChar();
					}
				}
			}
		}
		return std::string();
	}
}

NiflyExportingFixture::NiflyExportingFixture(const NiflyExportOptions& options)
	: exportOptions(options) {}

bool NiflyExportingFixture::ShouldExportNode(const MString& name) const {
	if (exportOptions.exportedShapes.empty())
		return true;
	for (const std::string& item : exportOptions.exportedShapes) {
		if (name == item.c_str())
			return true;
	}
	return false;
}

MStatus NiflyExportingFixture::WriteNodes(const MFileObject& file) {
	try {
		nifly::NiVersion version = nifly::NiVersion::getSSE();
		if (exportOptions.exportUserVersion2 >= 130) {
			version = nifly::NiVersion::getFO4();
		}
		nifly::NifFile nifFile;
		nifFile.Create(version);

		std::unordered_set<std::string> exportedMeshes;

		MItDag dagIt(MItDag::kDepthFirst);
		for (; !dagIt.isDone(); dagIt.next()) {
			MObject obj = dagIt.currentItem();
			if (!obj.hasFn(MFn::kTransform))
				continue;

			MFnTransform transformFn(obj);
			MString nodeName = transformFn.name();
			if (!ShouldExportNode(nodeName))
				continue;

			for (unsigned int i = 0; i < transformFn.childCount(); ++i) {
				MObject child = transformFn.child(i);
				if (!child.hasFn(MFn::kMesh))
					continue;

				MFnMesh meshFn(child);
				if (meshFn.isIntermediateObject())
					continue;

				std::string shapeName = nodeName.asChar();
				if (exportedMeshes.count(shapeName))
					continue;
				exportedMeshes.insert(shapeName);

				// Vertices
				MPointArray points;
				meshFn.getPoints(points, MSpace::kTransform);
				std::vector<nifly::Vector3> verts;
				verts.reserve(points.length());
				for (unsigned int v = 0; v < points.length(); ++v) {
					verts.emplace_back(static_cast<float>(points[v].x),
						static_cast<float>(points[v].y),
						static_cast<float>(points[v].z));
				}

				// Triangles
				MIntArray triangleCounts, triangleVertices;
				meshFn.getTriangles(triangleCounts, triangleVertices);
				std::vector<nifly::Triangle> tris;
				tris.reserve(triangleVertices.length() / 3);
				for (unsigned int t = 0; t + 2 < triangleVertices.length(); t += 3) {
					nifly::Triangle tri;
					tri.p1 = static_cast<uint16_t>(triangleVertices[t]);
					tri.p2 = static_cast<uint16_t>(triangleVertices[t + 1]);
					tri.p3 = static_cast<uint16_t>(triangleVertices[t + 2]);
					tris.push_back(tri);
				}

				// Normals
				MFloatVectorArray normals;
				meshFn.getVertexNormals(false, normals, MSpace::kTransform);
				std::vector<nifly::Vector3> nifNormals;
				nifNormals.reserve(normals.length());
				for (unsigned int n = 0; n < normals.length(); ++n) {
					nifNormals.emplace_back(normals[n].x, normals[n].y, normals[n].z);
				}

				// UVs
				MFloatArray uArray, vArray;
				meshFn.getUVs(uArray, vArray);
				std::vector<nifly::Vector2> uvs;
				uvs.resize(points.length(), nifly::Vector2(0.0f, 0.0f));
				MItMeshPolygon itPoly(child);
				MString uvSetName = meshFn.currentUVSetName();
				for (; !itPoly.isDone(); itPoly.next()) {
					for (unsigned int v = 0; v < itPoly.polygonVertexCount(); ++v) {
						int vertId = itPoly.vertexIndex(v);
						float2 uv;
						if (itPoly.getUV(v, uv, &uvSetName)) {
							if (vertId >= 0 && static_cast<size_t>(vertId) < uvs.size()) {
								uvs[vertId].u = uv[0];
								uvs[vertId].v = 1.0f - uv[1];
							}
						}
					}
				}

				// Colors
				std::vector<nifly::Color4> colors;
				MColorArray mayaColors;
				if (meshFn.getVertexColors(mayaColors) == MS::kSuccess && mayaColors.length() == points.length()) {
					colors.reserve(mayaColors.length());
					for (unsigned int c = 0; c < mayaColors.length(); ++c) {
						colors.emplace_back(mayaColors[c].r, mayaColors[c].g, mayaColors[c].b, mayaColors[c].a);
					}
				}

				nifly::NiShape* shape = nifFile.CreateShapeFromData(shapeName, &verts, &tris, &uvs, &nifNormals);
				if (!shape)
					continue;

				if (!colors.empty()) {
					nifFile.SetColorsForShape(shape, colors);
				}

				// Skinning
				MObject skinObj;
				MItDependencyGraph dgIt(child, MFn::kSkinClusterFilter, MItDependencyGraph::kUpstream, MItDependencyGraph::kDepthFirst);
				if (!dgIt.isDone()) {
					skinObj = dgIt.currentItem();
				}

				if (!skinObj.isNull()) {
					MFnSkinCluster skinFn(skinObj);
					MDagPathArray influences;
					skinFn.influenceObjects(influences);

					// Create bone nodes and map to indices
					std::vector<int> boneIds;
					boneIds.reserve(influences.length());
					std::unordered_map<std::string, nifly::NiNode*> boneNodes;

					for (unsigned int inf = 0; inf < influences.length(); ++inf) {
						MFnTransform infFn(influences[inf]);
						std::string boneName = infFn.name().asChar();
						nifly::MatTransform boneXform = ToNiflyTransform(infFn.transformation());

						nifly::NiNode* parentNode = nifFile.GetRootNode();
						MObject parentObj = influences[inf].node();
						MFnDagNode dagNode(parentObj);
						MObject parentMObj = dagNode.parent(0);
						if (parentMObj.hasFn(MFn::kJoint)) {
							MFnTransform parentFn(parentMObj);
							auto it = boneNodes.find(parentFn.name().asChar());
							if (it != boneNodes.end()) {
								parentNode = it->second;
							}
						}

						nifly::NiNode* boneNode = nifFile.AddNode(boneName, boneXform, parentNode);
						boneNodes[boneName] = boneNode;
						boneIds.push_back(static_cast<int>(nifFile.GetBlockID(boneNode)));
					}

					nifFile.CreateSkinning(shape);
					nifFile.SetShapeBoneIDList(shape, boneIds);

					MDagPath meshPath;
					MDagPath::getAPathTo(child, meshPath);

					unsigned int influenceCount = influences.length();
					MDoubleArray weights;
					unsigned int elements = 0;

					for (unsigned int vtx = 0; vtx < points.length(); ++vtx) {
						MFnSingleIndexedComponent compFn;
						MObject vertComp = compFn.create(MFn::kMeshVertComponent);
						compFn.addElement(static_cast<int>(vtx));
						skinFn.getWeights(meshPath, vertComp, weights, elements);

						std::vector<std::pair<uint8_t, float>> weightPairs;
						weightPairs.reserve(influenceCount);
						for (unsigned int i = 0; i < influenceCount; ++i) {
							float w = static_cast<float>(weights[i]);
							if (w > 0.0001f) {
								weightPairs.emplace_back(static_cast<uint8_t>(i), w);
							}
						}

						if (!weightPairs.empty()) {
							std::sort(weightPairs.begin(), weightPairs.end(),
								[](const auto& a, const auto& b) { return a.second > b.second; });
							if (weightPairs.size() > 4) {
								weightPairs.resize(4);
							}

							std::vector<uint8_t> boneIdx;
							std::vector<float> boneW;
							boneIdx.reserve(weightPairs.size());
							boneW.reserve(weightPairs.size());
							for (const auto& pair : weightPairs) {
								boneIdx.push_back(pair.first);
								boneW.push_back(pair.second);
							}
							nifFile.SetShapeVertWeights(shapeName, static_cast<uint16_t>(vtx), boneIdx, boneW);
						}
					}

					// Default single partition until dismember mapping is implemented
					nifFile.SetDefaultPartition(shape);
				}

				// Node transform
				nifly::MatTransform xform = ToNiflyTransform(transformFn.transformation());
				nifly::NiNode* node = nifFile.AddNode(shapeName, xform, nifFile.GetRootNode());
				if (node) {
					nifFile.SetParentNode(shape, node);
				}

				// Shader/texture wiring
				MObjectArray shaders;
				MIntArray faceIndices;
				meshFn.getConnectedShaders(0, shaders, faceIndices);
				if (shaders.length() > 0) {
					MObject shaderObj = shaders[0];
					std::string diffuse = GetFileTextureFromShader(shaderObj, "color");
					std::string normal = GetFileTextureFromShader(shaderObj, "normalCamera");

					auto shader = nifFile.GetShader(shape);
					auto lsShader = dynamic_cast<nifly::BSLightingShaderProperty*>(shader);
					if (lsShader) {
						auto texSet = nifFile.GetHeader().GetBlock<nifly::BSShaderTextureSet>(lsShader->textureSetRef);
						if (texSet) {
							if (!diffuse.empty())
								texSet->textures[0].get() = diffuse;
							if (!normal.empty())
								texSet->textures[1].get() = normal;
						}
					}
				}
			}
		}

		nifFile.Save(std::filesystem::path(file.resolvedFullName().asChar()));
		return MS::kSuccess;
	} catch (const std::exception& e) {
		MGlobal::displayError(MString("Nifly export failed: ") + e.what());
		return MS::kFailure;
	} catch (...) {
		MGlobal::displayError("Nifly export failed: unknown error.");
		return MS::kFailure;
	}
}

