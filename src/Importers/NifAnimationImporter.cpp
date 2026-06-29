#include "include/Importers/NifAnimationImporter.h"

// Returns true only if this interpolator actually animates something.
bool NifAnimationImporter::HasRealAnimation(NiInterpolatorRef interp) {
	if (interp == NULL) return false;

	// NiTransformInterpolator: real data lives in an attached NiTransformData.
	NiTransformInterpolatorRef ti = DynamicCast<NiTransformInterpolator>(interp);
	if (ti != NULL) {
		NiTransformDataRef d = ti->GetData();
		if (d == NULL) return false;              // empty .kf placeholder -> skip
		// NiTransformData has no KeyCount() getters here; probe the key vectors
		// directly (same getters the KF importer uses).
		if (d->GetTranslateKeys().empty() &&
			d->GetScaleKeys().empty() &&
			d->GetQuatRotateKeys().empty() &&
			d->GetXRotateKeys().empty()) {     // sentinel-only -> skip
			return false;
		}
		return true;
	}

	// Other interpolator kinds (B-spline, point3, float, bool) carry their own
	// data and are handled directly by the KF importer; let them through.
	return true;
}

void NifAnimationImporter::ImportControllers(NiAVObjectRef niAVObj, MDagPath& path) {
	list<NiTimeControllerRef> controllers = niAVObj->GetControllers();

	// Resolve the Maya target by the DAG path we were handed (from importedNodes).
	// Pass its Maya name to the KF importer so GetObjectByName hits the right node
	// even when MakeMayaName renamed it relative to the NIF block name.
	MFnDagNode targetFn(path.node());
	MString targetName = targetFn.name();

	for (list<NiTimeControllerRef>::iterator it = controllers.begin(); it != controllers.end(); ++it) {

		// --- FNV path: NiTransformController (a NiSingleInterpController) ---
		NiSingleInterpControllerRef sic = DynamicCast<NiSingleInterpController>(*it);
		if (sic != NULL) {
			NiInterpolatorRef interp = sic->GetInterpolator();
			if (HasRealAnimation(interp)) {
				// Reuse the tested parser: builds translate/scale/quat/XYZ curves.
				this->kfImporter.ImportAnimation(interp, targetName);
			}
			continue;
		}

		// --- Legacy path: NiKeyframeController (Fallout 3 / Oblivion) ---
		if ((*it)->IsDerivedType(NiKeyframeController::TYPE)) {
			NiKeyframeControllerRef niKeyCont = DynamicCast<NiKeyframeController>(*it);
			NiKeyframeDataRef niKeyData = niKeyCont->GetData();
			if (niKeyData == NULL) continue;

			// Wrap legacy NiKeyframeData in a NiTransformInterpolator so the same
			// KF parser handles it (NiTransformData derives from NiKeyframeData).
			NiTransformDataRef tdata = new NiTransformData();
			tdata->SetRotateType(niKeyData->GetRotateType());
			if (niKeyData->GetRotateType() == XYZ_ROTATION_KEY) {
				tdata->SetXRotateKeys(niKeyData->GetXRotateKeys());
				tdata->SetYRotateKeys(niKeyData->GetYRotateKeys());
				tdata->SetZRotateKeys(niKeyData->GetZRotateKeys());
			}
			else {
				tdata->SetQuatRotateKeys(niKeyData->GetQuatRotateKeys());
			}
			tdata->SetTranslateKeys(niKeyData->GetTranslateKeys());
			tdata->SetScaleKeys(niKeyData->GetScaleKeys());

			NiTransformInterpolatorRef wrap = new NiTransformInterpolator();
			wrap->SetData(tdata);

			this->kfImporter.ImportAnimation(StaticCast<NiInterpolator>(wrap), targetName);
		}
	}
}

NifAnimationImporter::~NifAnimationImporter() {

}


NifAnimationImporter::NifAnimationImporter( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils) 
	: NifTranslatorFixtureItem(translatorOptions,translatorData,translatorUtils) {

}

NifAnimationImporter::NifAnimationImporter() {

}

string NifAnimationImporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose)<<endl;
	out<<"NifAnimationImporter"<<endl;

	return out.str();
}

const Type& NifAnimationImporter::GetType() const {
	return TYPE;
}


const Type NifAnimationImporter::TYPE("NifAnimationImporter",&NifTranslatorFixtureItem::TYPE);
