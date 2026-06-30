#include "include/Exporters/NifKFExportingFixture.h"

NifKFExportingFixture::NifKFExportingFixture() {

}

NifKFExportingFixture::NifKFExportingFixture( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
	this->animationExporter = new NifKFAnimationExporter(translatorOptions, translatorData, translatorUtils);
}

// Create "start"/"end" bookmarks at the playback range bounds, only if missing.
// Uses Maya's own bookmark Python API so the timeSliderBookmarkManager is created
// correctly and the bookmarks actually render on the timeline.
static void EnsureRangeBookmarks() {
	// Make sure the bookmark node type is registered first.
	int loaded = 0;
	MGlobal::executeCommand("pluginInfo -q -loaded \"timeSliderBookmark\"", loaded);
	if (!loaded) {
		MGlobal::executeCommand("loadPlugin -quiet \"timeSliderBookmark\"");
	}

	double pbMin = 0.0, pbMax = 0.0;
	MGlobal::executeCommand("playbackOptions -q -min", pbMin);
	MGlobal::executeCommand("playbackOptions -q -max", pbMax);

	// Build a Python snippet: import Maya's bookmark module (name varies by version,
	// so try the known ones), then create "start"/"end" only if absent. createBookmark
	// in this module builds the manager + connection + timeline display for us.
	MString py;
	py += "import maya.cmds as cmds\n";
	py += "import maya.plugin.timeSliderBookmark.timeSliderBookmark as tsb\n";
	py += "def _has(nm):\n";
	py += "    for b in (cmds.ls(type='timeSliderBookmark') or []):\n";
	py += "        try:\n";
	py += "            if cmds.getAttr(b + '.name') == nm:\n";
	py += "                return True\n";
	py += "        except Exception:\n";
	py += "            pass\n";
	py += "    return False\n";
	py += "def _mk(nm, f):\n";
	py += "    if not _has(nm):\n";
	py += "        # Maya's own API: builds the manager + timeline display for us.\n";
	py += "        tsb.createBookmark(name=nm, start=f, stop=f)\n";
	py += MString("_mk('start', ") + pbMin + ")\n";
	py += MString("_mk('end', ") + pbMax + ")\n";

	MGlobal::executePythonCommand(py);
}

void CreateTextKeyExtraData(NiControllerSequenceRef controllerSequence)
{
	NiTextKeyExtraData* textKeyExtraData = new NiTextKeyExtraData();
	vector<Key<string>> keys = vector<Key<string>>();
	
	MItDependencyNodes it(MFn::kBase);
	while (!it.isDone())
	{
		MObject node = it.thisNode();
		MFnDependencyNode fnNode(node);

		if (fnNode.typeName() == "timeSliderBookmark")
		{
			MPlug namePlug = fnNode.findPlug("name", false);
			MPlug startPlug = fnNode.findPlug("timeRangeStart", false);
			MPlug stopPlug = fnNode.findPlug("timeRangeStop", false);

			if (!namePlug.isNull() && !startPlug.isNull() && !stopPlug.isNull())
			{
				MString name;
				namePlug.getValue(name);
				
				float startTime;
				startPlug.getValue(startTime);
				
				Key<string> textKey = Key<string>();
				textKey.data = name.asChar();
				textKey.time = startTime;
				
				keys.push_back(textKey);
			}
		}
		it.next();
	}

	std::sort(keys.begin(), keys.end(), [](const Key<string>& a, const Key<string>& b)
	{
		return a.time < b.time;
	});
	
	textKeyExtraData->SetKeys(keys);
	controllerSequence->SetTextKey(textKeyExtraData);
}

MStatus NifKFExportingFixture::WriteNodes( const MFileObject& file ) {
	MItDag iterator(MItDag::kDepthFirst);

	for(; !iterator.isDone(); iterator.next()) {
		MFnDagNode node(iterator.currentItem());
		if(node.isIntermediateObject()) {
			continue;
		}

		if ( 
			node.name().substring(0, 13) == "UniversalManip" ||
			node.name() == "groundPlane_transform" ||
			node.name() == "ViewCompass" ||
			node.name() == "CubeCompass" ||
			node.name() == "Manipulator1" ||
			node.name() == "persp" ||
			node.name() == "top" ||
			node.name() == "front" ||
			node.name() == "side"
			) {
			continue;
			}

		if(!iterator.currentItem().hasFn(MFn::Type::kTransform)) {
			continue;
		}

		if(!(this->translatorOptions->exportType == "allanimation") && 
			(!this->translatorUtils->isExportedJoint(node.name()) && !this->translatorUtils->isExportedJoint(node.name()) && !this->translatorUtils->isExportedMisc(node.name()))) {
			continue;
			}

		this->translatorData->animatedObjects.push_back(iterator.currentItem());
	}

	// TEMP diagnostic: list everything that passed the export filter, with full path
	// (full path shows the namespace) and the bare NIF name it will get.
	//MGlobal::displayInfo(MString("xray-bake: collected ") + (int)this->translatorData->animatedObjects.size() + " nodes");
	//for (size_t d = 0; d < this->translatorData->animatedObjects.size(); ++d) {
	//	MFnDagNode dbg(this->translatorData->animatedObjects[d]);
	//	MGlobal::displayInfo(MString("  node: ") + dbg.fullPathName()
	//		+ "  -> nif: " + this->translatorUtils->MakeNifName(dbg.name()).c_str());
	//}
	
	NiControllerSequenceRef controller_sequence = DynamicCast<NiControllerSequence>(NiControllerSequence::Create());
	// Curve-based range fails for baked nodes (IK/constraints have no anim curves, so
	// GetAnimation*Time() return FLT_MAX / FLT_MIN -> garbage). Fall back to the scene
	// playback range whenever the curve range looks invalid.
	float animStart = this->animationExporter->GetAnimationStartTime();
	float animStop = this->animationExporter->GetAnimationEndTime();
	//MGlobal::displayInfo(MString("xray-bake: curve range ") + animStart + " .. " + animStop);

	// A valid range needs start <= stop AND a real span (a zero/degenerate span means the
	// scanners found no usable keys), with both ends inside a sane bound.
	bool validCurveRange = (animStart < animStop)
		&& (animStart > -1e6f && animStart < 1e6f)
		&& (animStop > -1e6f && animStop < 1e6f);

	// For baking, prefer the visible PLAYBACK range (-min/-max, set by the timeline
	// sliders) rather than the full animation range (-ast/-aet).
	bool bakeOn = this->translatorOptions->bakeAnimation;
	if (bakeOn || !validCurveRange) {
		double pbStart = 0.0, pbEnd = 0.0;
		MGlobal::executeCommand("playbackOptions -q -min", pbStart); // playback start (frames)
		MGlobal::executeCommand("playbackOptions -q -max", pbEnd);   // playback end   (frames)
		animStart = (float)MTime(pbStart, MTime::uiUnit()).asUnits(MTime::kSeconds);
		animStop = (float)MTime(pbEnd, MTime::uiUnit()).asUnits(MTime::kSeconds);
	//	MGlobal::displayInfo(MString("xray-bake: using playback range ") + animStart + " .. " + animStop);
	}

	controller_sequence->SetStartTime(animStart);
	controller_sequence->SetStopTime(animStop);
	controller_sequence->SetName(this->translatorOptions->animationName);
	controller_sequence->SetTargetName(this->translatorOptions->animationTarget);
	controller_sequence->SetFrequency(1.0);
	controller_sequence->SetCycleType(translatorOptions->cycleType);

	EnsureRangeBookmarks();

	CreateTextKeyExtraData(controller_sequence);
	
	// Bake simulation for duplicate.
	
	/*
	int startFrame;
	int endFrame;
	
	MGlobal::executeCommand("playbackOptions -q -ast;", startFrame, true);
	MGlobal::executeCommand("playbackOptions -q -aet;", endFrame, true);
	
	MString timeRange = "\"";
	timeRange += to_string(startFrame).c_str();
	timeRange += ":";
	timeRange += to_string(endFrame).c_str();
	timeRange += "\"";
	
	MString jointsForBake;
	for (size_t i = 0; i < translatorData->animatedObjects.size(); i++)
	{
		MFnDagNode animatedObjectNode(translatorData->animatedObjects[i]);

		MDagPath path;
		animatedObjectNode.getPath(path);
		
		jointsForBake += "\"";
		jointsForBake += animatedObjectNode.fullPathName().asChar();
		jointsForBake += "\"";
		
		if (i != translatorData->animatedObjects.size() - 1) {
			jointsForBake += ", ";
		}
	}

	MString bakeResultsCommand = "bakeResults -simulation true -t " + timeRange + " {" + jointsForBake + "}";
	MGlobal::executeCommand(MString(bakeResultsCommand), true);
	*/

	// Write tree.
	
	vector<MObject> objectsWithExportIndexes;
	vector<MObject> objectsWithoutExportIndexes;

	MFnDependencyNode aux1;
	MFnDependencyNode aux2;

	for(int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		MFnDependencyNode node(this->translatorData->animatedObjects.at(i));
		MPlug plug = node.findPlug("exportIndex");
		string name = node.name().asChar();

		if(plug.isNull()) {
			objectsWithoutExportIndexes.push_back(node.object());
		} else {
			objectsWithExportIndexes.push_back(node.object());
		}
	}
	
	this->translatorData->animatedObjects.clear();

	for(int i = 0;i < (int)(objectsWithExportIndexes.size()) - 1; i++) {
		for(int j = i + 1; j < objectsWithExportIndexes.size(); j++) {
			aux1.setObject(objectsWithExportIndexes.at(i));
			aux2.setObject(objectsWithExportIndexes.at(j));
			MPlug plug_i = aux1.findPlug("exportIndex");
			MPlug plug_j = aux2.findPlug("exportIndex");

			if(plug_i.asInt() > plug_j.asInt()) {
				swap(objectsWithExportIndexes[i], objectsWithExportIndexes[j]);
			}
		}
	}

	for(int i = 0; i < objectsWithExportIndexes.size(); i++) {
		this->translatorData->animatedObjects.push_back(objectsWithExportIndexes[i]);
	}

	for(int i = 0; i < objectsWithoutExportIndexes.size(); i++) {
		this->translatorData->animatedObjects.push_back(objectsWithoutExportIndexes[i]);
	}

	for(int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		
	}

	// Collect nodes whose keys must be baked, plus the empty interpolators created for
	// them, so we can sample the whole skeleton in ONE timeline pass below.
	vector<MObject> bakeObjects;
	vector<NiTransformInterpolatorRef> bakeInterps;

	for (int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		MObject obj = this->translatorData->animatedObjects.at(i);

		NiTransformInterpolatorRef bakedInterp = NULL;
		MObject bakedObject;
		// Pass the batch channel: ExportAnimation builds/binds the interpolator as usual,
		// but for baked nodes it returns an EMPTY one instead of stepping the timeline.
		this->animationExporter->ExportAnimation(controller_sequence, obj, &bakedInterp, &bakedObject);

		if (bakedInterp != NULL) {
			bakeObjects.push_back(bakedObject);
			bakeInterps.push_back(bakedInterp);
		}
	}

	// TEMP diagnostic: how many nodes entered the deferred-bake list, and the bake flag.
	//MGlobal::displayInfo(MString("xray-bake: bakeObjects=") + (int)bakeObjects.size()
	//	+ " globalBakeOption=" + (int)this->translatorOptions->bakeAnimation);

	// Single timeline pass for every baked node: DG is evaluated (frames) times total,
	// not (frames * nodes). This is what makes baked export fast for full skeletons.
	if (!bakeObjects.empty()) {
		this->animationExporter->BakeAllTransforms(
			bakeObjects, bakeInterps,
			controller_sequence->GetStartTime(),
			controller_sequence->GetStopTime());
	}

	//out << "Writing Finished NIF file..." << endl;
	NifInfo nif_info(this->translatorOptions->exportVersion, this->translatorOptions->exportUserVersion);
	nif_info.endian = ENDIAN_LITTLE; //Intel endian format
	nif_info.exportInfo1 = "NifTools Maya NIF Plug-in " + string(PLUGIN_VERSION);
	nif_info.userVersion2 = this->translatorOptions->exportUserVersion2;

	auto fileName = file.resolvedFullName();
	if (translatorOptions->exportType == "animation")
	{
		auto dotIndex = fileName.rindex('.');
		auto fileNameWithoutExtension = fileName.substring(0, dotIndex);
		fileName = fileNameWithoutExtension + "kf";
	}
	
	Niflib::WriteNifTree(fileName.asChar(), controller_sequence, nif_info);

	// Delete duplicate.
	// MGlobal::executeCommand("delete \"" + clonedRootName + "\";", true);
	
	return MStatus::kSuccess;
}

string NifKFExportingFixture::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifExportingFixture::asString(verbose)<<endl;
	out<<"NifKFExportingFixture"<<endl;

	return out.str();
}

const Type& NifKFExportingFixture::getType() const {
	return TYPE;
}

const Type NifKFExportingFixture::TYPE("NifKFExportingFixture",&NifExportingFixture::TYPE);


