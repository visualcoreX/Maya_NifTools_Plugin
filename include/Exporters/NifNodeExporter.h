#ifndef _NIFNODEEXPORTER_H
#define _NIFNODEEXPORTER_H
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
#include <maya/MCommandResult.h>
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
#include <obj/NiTransformController.h>
#include <obj/NiTransformInterpolator.h>
#include <obj/NiTransformData.h>
#include <obj/NiSingleInterpController.h>
#include <Ref.h>
#include "include/Common/NifTranslatorFixtureItem.h"
#include "include/Exporters/NifCollisionExporter.h"

using namespace std;
using namespace Niflib;
// Forward declaration to avoid an include cycle — the .cpp includes the full
// NifKFAnimationExporter.h. Set externally by the owning ExportingFixture
// (same pattern as collisionExporter); used in ExportAV to bake a node's anim
// curves into a real NiTransformInterpolator when exportType == "nifanimation".
class NifKFAnimationExporter;
typedef Ref<NifKFAnimationExporter> NifKFAnimationExporterRef;

class NifNodeExporter;
typedef Ref<NifNodeExporter> NifNodeExporterRef;
class NifNodeExporter : public NifTranslatorFixtureItem {
public:
	NifNodeExporter();
	NifNodeExporter(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils);
	virtual void ExportDAGNodes();
	virtual void ExportAV(NiAVObjectRef avObj, MObject dagNode, bool worldTransform = false);
	virtual string asString(bool verbose = false) const;
	virtual const Type& GetType() const;
	const static Type TYPE;

	// Set externally by the owning ExportingFixture (same pattern as
		// collisionExporter). Used in ExportAV to build a real NiTransformInterpolator
		// from a node's anim curves for embedded-in-NIF animation (exportType ==
		// "nifanimation"); NULL for plain geometry export.
	NifKFAnimationExporterRef animationExporter;

	// Set externally by whichever ExportingFixture owns this node exporter
	// (same pattern as NifNodeImporter::collisionImporter on the import
	// side). Used inside ExportDAGNodes to check each exported joint's own
	// Maya DAG children for a "bhkRigidBody"/"bhkRigidBodyT" collision
	// transform - NOT to create a separate NiNode for the collision itself,
	// just to attach it (via SetCollisionObject) to the NiNode that joint
	// already produced.
	NifCollisionExporterRef collisionExporter;
};
#endif