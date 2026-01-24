#ifndef _BSLIGHTNINGSHADER_H
#define _BSLIGHTNINGSHADER_H

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
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MPxNode.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h> 

#include <gen/enums.h>

using namespace Niflib;

class BSLightningShader : public MPxNode {
public:

	BSLightningShader();

	virtual ~BSLightningShader();

	virtual MStatus compute(const MPlug& plug, MDataBlock& dataBlock);

	static void* creator();

	static MStatus initialize();

public:

	static MStringArray shaderFlags1ToStringArray(SkyrimShaderPropertyFlags1 shader_flags1);

	static SkyrimShaderPropertyFlags1 stringArrayToShaderFlags1(const MStringArray& string_array);

	static MStringArray shaderFlags2ToStringArray(SkyrimShaderPropertyFlags2 shader_flags2);

	static SkyrimShaderPropertyFlags2 stringArrayToShaderFlags2(const MStringArray& string_array);

	static BSLightingShaderPropertyShaderType BSLightningShader::stringToSkyrimShaderType(MString shader_type);

	static MString skyrimShaderTypeToString(BSLightingShaderPropertyShaderType shader_type);

	static MObject targetShader;

	static MObject targetShape;

	static MObject shaderType;

	static MObject shaderFlags1;

	static MObject shaderFlags2;

	static MObject lightingEffect1;

	static MObject lightingEffect2;

	static MObject environmentMapScale;

	static MObject skinTintColor;

	static MObject hairTintColor;

	static MObject textureSlot2;

	static MObject textureSlot3;

	static MObject textureSlot4;

	static MObject textureSlot5;

	static MObject textureSlot6;

	static MObject textureSlot7;

	static MObject textureSlot8;

	static MTypeId id;
};

#endif