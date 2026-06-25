#ifndef _NIFMORPHEXPORTER_H
#define _NIFMORPHEXPORTER_H

#include <maya/MObject.h>
#include <maya/MFnMesh.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnComponentListData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnGeometryFilter.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MObjectArray.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MAnimUtil.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MStatus.h>
#include <maya/MGlobal.h>

#include <string>
#include <vector>
#include <map>

#include <niflib.h>
#include <Ref.h>
#include <obj/NiTriBasedGeom.h>
#include <obj/NiTriBasedGeomData.h>
#include <obj/NiGeometryData.h>
#include <obj/NiGeomMorpherController.h>
#include <obj/NiMorphData.h>
#include <obj/NiFloatInterpolator.h>
#include <obj/NiFloatData.h>
#include <obj/NiInterpolator.h>

#include "include/Common/NifTranslatorFixtureItem.h"

using namespace std;
using namespace Niflib;

class NifMorphExporter;
typedef Ref<NifMorphExporter> NifMorphExporterRef;

// Exports Maya blendShape deformers as NiGeomMorpherController + NiMorphData.
// One instance is shared across all meshes exported in this session.
class NifMorphExporter : public NifTranslatorFixtureItem {
public:
    NifMorphExporter();
    NifMorphExporter(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils);

    // Scans the scene once and caches mesh-full-path -> blendShape node,
    // mirroring NifMeshExporter::EnumerateSkinClusters().
    // Call this once before any ExportMorph() calls.
    virtual void EnumerateBlendShapes();

    // If 'mesh' has a cached blendShape deformer, builds a NiGeomMorpherController
    // and attaches it to 'targetGeom'. Does nothing if there is no blendShape.
    // 'mesh' must be the *base* (input) mesh object, not the deformed output,
    // so vertex order matches what NifMeshExporter already exported.
    virtual void ExportMorph(MObject mesh, NiTriBasedGeomRef targetGeom, const vector<unsigned int>& finalToMayaIndex);

    virtual string asString(bool verbose = false) const;
    virtual const Type& GetType() const;
    const static Type TYPE;

private:
    // mesh full path name -> blendShape deformer MObject
    map<string, MObject> meshBlendShapes;

    // Reads the absolute target-shape vertex positions for one weight index (target),
    // straight from the blendShape's inputPointsTarget plug data (sparse point list).
    // baseVerts are the already-exported base positions; any vertex not present
    // in the sparse target data keeps its base position (i.e. zero delta).
    // Returns false if this target slot has no geometry connected.
    bool GetTargetAbsolutePositions(MFnBlendShapeDeformer& blendFn, unsigned int targetIndex,
        const vector<Vector3>& baseVerts, const vector<unsigned int>& finalToMayaIndex, vector<Vector3>& outPositions);

    // Reads the animation curve (if any) driving the blendShape weight plug
    // and converts it into a vector of (time, weight) keys for NiMorphData::SetMorphKeys.
    // Returns a single static key at the current weight value if there is no curve.
    vector< Key<float> > ExportWeightKeys(const MPlug& weightPlug);

    // Gets the Shape Editor alias for a target's weight; falls back to "TargetN".
    string GetTargetAliasName(MFnDependencyNode& blendDepNode, MPlug& weightPlug, unsigned int targetIndex);
};

#endif