#include "include/Exporters/NifMorphExporter.h"
#include <fstream>

NifMorphExporter::NifMorphExporter() {
}

NifMorphExporter::NifMorphExporter(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils)
    : NifTranslatorFixtureItem(translatorOptions, translatorData, translatorUtils) {
}

void NifMorphExporter::EnumerateBlendShapes()
{
    // Walk every blendShape node in the scene once, same pattern as
    // NifMeshExporter::EnumerateSkinClusters(), so ExportMorph() is a cheap lookup later.
    MItDependencyNodes blendIt(MFn::kBlendShape);
    for (; !blendIt.isDone(); blendIt.next()) {
        MObject blendObj = blendIt.item();

        // getOutputGeometry() lives on the MFnGeometryFilter base class, not
        // directly on MFnBlendShapeDeformer.
        MFnGeometryFilter filterFn(blendObj);

        MObjectArray outputShapes;
        filterFn.getOutputGeometry(outputShapes);

        for (unsigned int i = 0; i < outputShapes.length(); ++i) {
            if (outputShapes[i].hasFn(MFn::kMesh)) {
                MFnMesh meshFn(outputShapes[i]);
                // Only one blendShape per mesh is supported for now, same restriction
                // NifMeshExporter already applies to skinClusters.
                this->meshBlendShapes[meshFn.fullPathName().asChar()] = blendObj;
            }
        }
    }
}

bool NifMorphExporter::GetTargetAbsolutePositions(MFnBlendShapeDeformer& blendFn, unsigned int targetIndex,
    const vector<Vector3>& baseVerts, const vector<unsigned int>& finalToMayaIndex, vector<Vector3>& outPositions)
{
    MStatus stat;

    MFnDependencyNode depFn(blendFn.object());

    MPlug inputTargetPlug = depFn.findPlug("inputTarget", true, &stat);
    if (stat != MS::kSuccess) return false;
    MPlug inputTargetElem = inputTargetPlug.elementByLogicalIndex(0, &stat);
    if (stat != MS::kSuccess) return false;

    MPlug groupPlug = inputTargetElem.child(0);
    MPlug groupElem = groupPlug.elementByLogicalIndex(targetIndex, &stat);
    if (stat != MS::kSuccess) return false;

    MPlug itemPlug = groupElem.child(0);
    MPlug itemElem = itemPlug.elementByLogicalIndex(6000, &stat);
    if (stat != MS::kSuccess) {
        return false;
    }

    MPlug pointsTargetPlug = itemElem.child(3);
    MPlug componentsTargetPlug = itemElem.child(4);

    MObject pointsData, componentsData;
    pointsTargetPlug.getValue(pointsData);
    componentsTargetPlug.getValue(componentsData);

    MFnPointArrayData pointArrayFn(pointsData, &stat);
    if (stat != MS::kSuccess) return false;
    MPointArray deltaPoints = pointArrayFn.array();

    if (deltaPoints.length() == 0) {
        return false;
    }

    MFnComponentListData compListFn(componentsData, &stat);
    MIntArray affectedIndices;
    if (stat == MS::kSuccess) {
        unsigned int listLen = compListFn.length();
        for (unsigned int c = 0; c < listLen; ++c) {
            MObject comp = compListFn[c];
            MFnSingleIndexedComponent compFn(comp, &stat);
            if (stat == MS::kSuccess) {
                MIntArray elems;
                compFn.getElements(elems);
                for (unsigned int e = 0; e < elems.length(); ++e) {
                    affectedIndices.append(elems[e]);
                }
            }
        }
    }

    if (affectedIndices.length() != deltaPoints.length()) {
        MGlobal::displayWarning("blendShape target component/point count mismatch. Skipping this target.");
        return false;
    }

    // Build a quick lookup: Maya vertex index -> delta, for the vertices this target touches.
    map<int, Vector3> mayaDeltaByIndex;
    for (unsigned int i = 0; i < affectedIndices.length(); ++i) {
        Vector3 delta;
        delta.x = float(deltaPoints[i].x);
        delta.y = float(deltaPoints[i].y);
        delta.z = float(deltaPoints[i].z);
        mayaDeltaByIndex[affectedIndices[i]] = delta;
    }

    // --- TEMP DEBUG LOG ---
    //{
    //    std::ofstream log("E:\\Maya Projects\\FNV\\FNV Project\\Rig\\morph_debug.log", std::ios::out | std::ios::app);
    //    if (log.is_open()) {
    //        log << "--- target " << targetIndex << " ---" << std::endl;
    //        log << "affectedIndices.length(): " << affectedIndices.length() << std::endl;
    //        for (unsigned int i = 0; i < affectedIndices.length() && i < 20; ++i) {
    //            log << "  mayaIdx=" << affectedIndices[i]
    //                << "  delta=(" << deltaPoints[i].x << "," << deltaPoints[i].y << "," << deltaPoints[i].z << ")" << std::endl;
    //        }
    //    }
    //}
    // --- END TEMP DEBUG LOG ---

    // Now walk every FINAL (post-split) NIF vertex, look up which Maya vertex it
    // came from, and apply that Maya vertex's delta - this is what correctly
    // handles Split()'s vertex duplication on UV seams / hard edges.
    outPositions.assign(baseVerts.size(), Vector3(0.0f, 0.0f, 0.0f));
    for (unsigned int nifIdx = 0; nifIdx < finalToMayaIndex.size() && nifIdx < outPositions.size(); ++nifIdx) {
        int mayaIdx = (int)finalToMayaIndex[nifIdx];
        map<int, Vector3>::iterator dIt = mayaDeltaByIndex.find(mayaIdx);
        if (dIt != mayaDeltaByIndex.end()) {
            outPositions[nifIdx].x = dIt->second.x;
            outPositions[nifIdx].y = dIt->second.y;
            outPositions[nifIdx].z = dIt->second.z;
        }
    }

    return true;
}

vector< Key<float> > NifMorphExporter::ExportWeightKeys(const MPlug& weightPlug)
{
    MObjectArray animCurves;
    bool hasCurve = MAnimUtil::findAnimation(weightPlug, animCurves) && animCurves.length() > 0;

    vector< Key<float> > keys;

    if (!hasCurve) {
        // No keyframes on this weight: export a single static key with the
        // current weight value so the morph still has valid data.
        double currentWeight = 0.0;
        weightPlug.getValue(currentWeight);

        Key<float> key;
        key.time = 0.0f;
        key.data = float(currentWeight);
        keys.push_back(key);
        return keys;
    }

    // Only the first driving curve is used; layered/blended weight animation
    // is not supported here.
    MFnAnimCurve curveFn(animCurves[0]);
    unsigned int numKeys = curveFn.numKeys();
    keys.resize(numKeys);

    for (unsigned int i = 0; i < numKeys; ++i) {
        MTime keyTime = curveFn.time(i);
        // Convert from Maya time (frames) to NIF time (seconds).
        keys[i].time = float(keyTime.as(MTime::kSeconds));
        keys[i].data = float(curveFn.value(i));
    }

    // --- TEMP DEBUG LOG ---
    //{
    //    std::ofstream log("E:\\Maya Projects\\FNV\\FNV Project\\Rig\\morph_debug.log", std::ios::out | std::ios::app);
    //    if (log.is_open()) {
    //        log << "=== ExportWeightKeys ===" << std::endl;
    //        log << "numKeys: " << numKeys << std::endl;
    //        for (unsigned int i = 0; i < numKeys; ++i) {
    //            log << "  key[" << i << "] time=" << keys[i].time << " value=" << keys[i].data << std::endl;
    //        }
    //    }
    //}
    // --- END TEMP DEBUG LOG ---

    return keys;
}

string NifMorphExporter::GetTargetAliasName(MFnDependencyNode& blendDepNode, MPlug& weightPlug, unsigned int targetIndex)
{
    MStringArray aliasList;
    blendDepNode.getAliasList(aliasList);

    // weightPlug.name() can return the alias itself instead of the raw "weight[i]"
    // path when an alias is set, so build the raw plug name manually instead.
    stringstream rawPlugName;
    rawPlugName << "weight[" << weightPlug.logicalIndex() << "]";
    string barePlugName = rawPlugName.str();

    // aliasList alternates: [alias, plugPath, alias, plugPath, ...]
    for (unsigned int i = 0; i + 1 < aliasList.length(); i += 2) {
        string plugName = aliasList[i + 1].asChar();

        if (plugName == barePlugName) {
            return aliasList[i].asChar();
        }
    }

    // No alias set in Maya: use a generic fallback name.
    stringstream fallback;
    fallback << "Target" << targetIndex;
    return fallback.str();
}

void NifMorphExporter::ExportMorph(MObject mesh, NiTriBasedGeomRef targetGeom, const vector<unsigned int>& finalToMayaIndex)
{
    if (targetGeom == NULL) {
        return;
    }

    MFnMesh meshFn(mesh);
    string meshPath = meshFn.fullPathName().asChar();

    map<string, MObject>::iterator it = this->meshBlendShapes.find(meshPath);
    if (it == this->meshBlendShapes.end()) {
        return;
    }

    MFnBlendShapeDeformer blendFn(it->second);

    NiGeometryDataRef baseGeomData = targetGeom->GetData();
    NiTriBasedGeomDataRef geomData = DynamicCast<NiTriBasedGeomData>(baseGeomData);
    if (geomData == NULL) {
        MGlobal::displayWarning("Mesh has a blendShape but no geometry data was found. Skipping morph export.");
        return;
    }

    vector<Vector3> baseVerts = geomData->GetVertices();
    // --- TEMP DEBUG LOG ---
    //{
    //    std::ofstream log("E:\\Maya Projects\\FNV\\FNV Project\\Rig\\morph_debug.log", std::ios::out | std::ios::app);
    //    if (log.is_open()) {
    //        log << "=== ExportMorph for mesh ===" << std::endl;
    //        log << "baseVertexCount (NIF): " << baseVerts.size() << std::endl;
    //        log << "finalToMayaIndex.size(): " << finalToMayaIndex.size() << std::endl;
    //        for (size_t i = 0; i < finalToMayaIndex.size() && i < 20; ++i) {
    //            log << "  nifIdx=" << i << " -> mayaIdx=" << finalToMayaIndex[i]
    //                << "  basePos=(" << baseVerts[i].x << "," << baseVerts[i].y << "," << baseVerts[i].z << ")" << std::endl;
    //        }
    //    }
    //}
    // --- END TEMP DEBUG LOG ---
    unsigned int baseVertexCount = (unsigned int)baseVerts.size();
    if (baseVertexCount == 0) {
        return;
    }

    MPlug weightArrayPlug = blendFn.findPlug("weight");
    unsigned int numTargets = weightArrayPlug.numElements();

    if (numTargets == 0) {
        return;
    }

    NiMorphDataRef morphData = new NiMorphData();
    morphData->SetRelativeTargets(true); // NEW: must be 1, as in all official files - the niflib default appears to be 0, which inverts weight interpretation in NifSkope
    morphData->SetVertexCount((int)baseVertexCount);
    morphData->SetMorphCount((int)numTargets + 1);

    // Morph 0: base/rest shape, no keys.
    morphData->SetMorphVerts(0, baseVerts);
    morphData->SetFrameName(0, "Basis"); // matches official FNV file convention

    MFnDependencyNode blendDepNode(blendFn.object());

    // One interpolator per target (NOT including the base shape), built in
    // parallel with the NiMorphData entries so both stay in sync.
    vector< Ref<NiInterpolator> > interpolators;

    // NEW: index 0 corresponds to morph 0 (the base/rest shape), which is never
    // animated. NiGeomMorpherController still expects a placeholder interpolator
    // here so that interpolators[i] lines up 1:1 with NiMorphData's morphs[i].
    NiFloatInterpolatorRef baseInterp = new NiFloatInterpolator();
    interpolators.push_back(DynamicCast<NiInterpolator>(baseInterp));

    // Track the overall min/max key time across all target weight curves,
    // used for the controller's StartTime/StopTime.
    bool haveAnyKeys = false;
    float minTime = 0.0f;
    float maxTime = 0.0f;

    for (unsigned int t = 0; t < numTargets; ++t) {
        MPlug weightPlug = weightArrayPlug.elementByPhysicalIndex(t);
        int morphIndex = (int)t + 1;

        string targetName = GetTargetAliasName(blendDepNode, weightPlug, t);
        morphData->SetFrameName(morphIndex, targetName);

        vector<Vector3> targetVerts;
        if (!GetTargetAbsolutePositions(blendFn, t, baseVerts, finalToMayaIndex, targetVerts)) {
            morphData->SetMorphVerts(morphIndex, baseVerts);
            // Still add a placeholder interpolator so the interpolator list
            // stays aligned with the morph target list.
            NiFloatDataRef emptyFloatData = new NiFloatData();
            NiFloatInterpolatorRef emptyInterp = new NiFloatInterpolator();
            emptyInterp->SetData(emptyFloatData);
            interpolators.push_back(DynamicCast<NiInterpolator>(emptyInterp));
            continue;
        }

        morphData->SetMorphVerts(morphIndex, targetVerts);

        vector< Key<float> > weightKeys = ExportWeightKeys(weightPlug);
        morphData->SetMorphKeys(morphIndex, weightKeys);
        morphData->SetMorphKeyType(morphIndex, LINEAR_KEY);

        // Build the matching NiFloatInterpolator + NiFloatData pair, carrying
        // the same keys, as required by the modern (FNV/Skyrim-era) controller format.
        NiFloatDataRef floatData = new NiFloatData();
        floatData->SetKeys(weightKeys);
        floatData->SetKeyType(LINEAR_KEY);

        NiFloatInterpolatorRef interp = new NiFloatInterpolator();
        interp->SetData(floatData);
        if (!weightKeys.empty()) {
            // Seed the interpolator's "posed" value with the first key so it
            // has a sane value even before any controller evaluation happens.
            interp->SetFloatValue(weightKeys.front().data);
        }
        interpolators.push_back(DynamicCast<NiInterpolator>(interp));

        if (!weightKeys.empty()) {
            float t0 = weightKeys.front().time;
            float t1 = weightKeys.back().time;
            if (!haveAnyKeys) {
                minTime = t0;
                maxTime = t1;
                haveAnyKeys = true;
            }
            else {
                minTime = min(minTime, t0);
                maxTime = max(maxTime, t1);
            }
        }
    }

    NiGeomMorpherControllerRef morpher = new NiGeomMorpherController();
    morpher->SetData(morphData);
    morpher->SetInterpolators(interpolators);

    morpher->SetStartTime(haveAnyKeys ? minTime : 0.0f);
    morpher->SetStopTime(haveAnyKeys ? maxTime : 0.0f);
    morpher->SetFrequency(1.0f); // was defaulting to 0 - 1.0 means "normal playback speed"
    morpher->SetPhase(0.0f);
    morpher->SetFlags(12); // active + CYCLE_CLAMP, per FNV convention

    targetGeom->AddController(DynamicCast<NiTimeController>(morpher));
}

string NifMorphExporter::asString(bool verbose /*= false */) const {
    stringstream out;
    out << NifTranslatorFixtureItem::asString(verbose) << endl;
    out << "NifMorphExporter" << endl;
    return out.str();
}

const Type& NifMorphExporter::GetType() const {
    return TYPE;
}

const Type NifMorphExporter::TYPE("NifMorphExporter", &NifTranslatorFixtureItem::TYPE);