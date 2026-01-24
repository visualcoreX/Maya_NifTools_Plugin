#include "include/Exporters/NifKFExportingFixture.h"

NifKFExportingFixture::NifKFExportingFixture() {

}

NifKFExportingFixture::NifKFExportingFixture( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
	this->animationExporter = new NifKFAnimationExporter(translatorOptions, translatorData, translatorUtils);
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
	
	NiControllerSequenceRef controller_sequence = DynamicCast<NiControllerSequence>(NiControllerSequence::Create());
	controller_sequence->SetStartTime(this->animationExporter->GetAnimationStartTime());
	controller_sequence->SetStopTime(this->animationExporter->GetAnimationEndTime());
	controller_sequence->SetName(this->translatorOptions->animationName);
	controller_sequence->SetTargetName(this->translatorOptions->animationTarget);
	controller_sequence->SetFrequency(1.0);
	controller_sequence->SetCycleType(translatorOptions->cycleType);

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

	for(int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		MFnDependencyNode node(this->translatorData->animatedObjects.at(i));
		string name = node.name().asChar();
		this->animationExporter->ExportAnimation(controller_sequence, this->translatorData->animatedObjects.at(i));
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


