#ifndef _NIFKFANIMATIONEXPORTER_H
#define _NIFKFANIMATIONEXPORTER_H

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
#include <maya/MItMeshVertex.h>
#include <maya/MAnimControl.h>   // MAnimControl::currentTime / setCurrentTime for frame stepping
#include <maya/MTransformationMatrix.h>  // decompose into translate/rotate/scale

#include <string> 
#include <vector>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <iostream>

#include <ComplexShape.h>
#include <MatTexCollection.h>
#include <niflib.h>
#include <obj/NiAlphaProperty.h>
#include <obj/NiMaterialProperty.h>
#include <obj/NiNode.h>
#include <obj/NiObject.h>
#include <obj/NiProperty.h>
#include <obj/NiSkinData.h>
#include <obj/NiSkinInstance.h>
#include <obj/NiSourceTexture.h>
#include <obj/NiSpecularProperty.h>
#include <obj/NiTexturingProperty.h>
#include <obj/NiTriBasedGeom.h>
#include <obj/NiTriBasedGeomData.h>
#include <obj/NiTriShape.h>
#include <obj/NiTriShapeData.h>
#include <obj/NiTriStripsData.h>
#include <obj/NiTimeController.h>
#include <obj/NiKeyframeController.h>
#include <obj/NiKeyframeData.h>
#include <obj/NiTextureProperty.h>
#include <obj/NiImage.h>
#include <obj/NiControllerSequence.h>
#include <obj/NiStringPalette.h>
#include <gen/ControllerLink.h>
#include <obj/NiTransformInterpolator.h>
#include <obj/NiTransformData.h>
#include "obj/NiTransformController.h"
#include "obj/NiBSplineCompTransformInterpolator.h"
#include "obj/NiBSplineBasisData.h"
#include "obj/NiBSplineData.h"
#include "obj/NiPoint3Interpolator.h"
#include "obj/NiPosData.h"
#include <obj/NiFloatInterpolator.h>
#include <obj/NiFloatData.h>
#include <obj/NiBoolInterpolator.h>
#include <obj/NiBoolData.h>
#include <obj/NiGeomMorpherController.h>
#include <obj/NiVisController.h>
#include "obj/NiTextKeyExtraData.h"
#include <Ref.h>

#include "include/Common/NifTranslatorFixtureItem.h"

using namespace Niflib;
using namespace std;

class NifKFAnimationExporter;

typedef Ref<NifKFAnimationExporter> NifKFAnimationExporterRef;

class NifKFAnimationExporter : public NifTranslatorFixtureItem {

public:

	NifKFAnimationExporter();

	NifKFAnimationExporter(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils);
	
	// Builds a NiTransformInterpolator (+ NiTransformData) from a node's anim
	// curves, using the exact sampling logic as .kf export. Factored out so
	// embedded-NIF animation (NifNodeExporter::ExportAV) can reuse it. Only the
	// NiTransformInterpolator case is handled here - the one used for transform-
	// node animation in FNV. The interpolator's base translation/rotation/scale
	// are left as the -FLT_MAX sentinel (vanilla stores the base there even when
	// NiTransformData is present); the real values live in the keyed data.
	//
	// Returns NULL if the node has no translate/scale/rotate curves.
	//
	// outStartTime/outStopTime (optional): receive this node's own key time range
	// - the first and last key across all its curves - matching what vanilla puts
	// on the NiTransformController's Start/Stop Time. Left untouched if the node
	// has no keys, so callers should pre-seed them (e.g. to 0.0).
	virtual NiTransformInterpolatorRef BuildTransformInterpolator(MObject object,
		float* outStartTime = NULL, float* outStopTime = NULL);

	// Frame-baked variant: samples the evaluated local transform (captures IK/constraints).
	NiTransformInterpolatorRef BuildBakedTransformInterpolator(MObject object,
		float startTime, float stopTime);

	// Batch bake: ONE timeline pass for many nodes at once. Steps each frame a single
	// time and samples every node, so the DG is evaluated (frames) times instead of
	// (frames * nodes). Use this instead of calling BuildBakedTransformInterpolator
	// per node when baking a whole skeleton.
	void BakeAllTransforms(const std::vector<MObject>& nodes,
		const std::vector<NiTransformInterpolatorRef>& interps,
		float startTime, float stopTime);

	// outBakedInterp/outBakedObject (optional): in bake mode the created interpolator is
	// left EMPTY (no keys) and handed back here so the caller can fill every node's keys
	// in a single timeline pass via BakeAllTransforms. NULL => behave exactly as before.
	virtual void ExportAnimation(NiControllerSequenceRef controller_sequence, MObject object,
		NiTransformInterpolatorRef* outBakedInterp = NULL, MObject* outBakedObject = NULL);

	virtual float GetAnimationStartTime();

	virtual float GetAnimationEndTime();

	virtual string asString( bool verbose = false ) const;

	virtual const Type& getType() const;

	const static Type TYPE;
};

#endif