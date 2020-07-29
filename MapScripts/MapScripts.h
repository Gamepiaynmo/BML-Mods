#pragma once

#include <BML/BMLAll.h>
#include <map>

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

class MapScripts : public IMod {
public:
	MapScripts(IBML* bml) : IMod(bml) {}

	virtual CKSTRING GetID() override { return "MapScripts"; }
	virtual CKSTRING GetVersion() override { return BML_VERSION; }
	virtual CKSTRING GetName() override { return "Map Scripts"; }
	virtual CKSTRING GetAuthor() override { return "Gamepiaynmo"; }
	virtual CKSTRING GetDescription() override { return "Activate callback scripts in maps."; }
	DECLARE_BML_VERSION;

	virtual void OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
		CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
		BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) override;

	virtual void OnPostLoadLevel() override;
	virtual void OnStartLevel() override;
	virtual void OnPreResetLevel() override;
	virtual void OnPostResetLevel() override;
	virtual void OnPauseLevel() override;
	virtual void OnUnpauseLevel() override;
	virtual void OnPreExitLevel() override;
	virtual void OnPreNextLevel() override;
	virtual void OnDead() override;
	virtual void OnPreEndLevel() override;
	virtual void OnPostEndLevel() override;

	virtual void OnCounterActive() override;
	virtual void OnCounterInactive() override;
	virtual void OnBallNavActive() override;
	virtual void OnBallNavInactive() override;
	virtual void OnCamNavActive() override;
	virtual void OnCamNavInactive() override;
	virtual void OnBallOff() override;
	virtual void OnPreCheckpointReached() override;
	virtual void OnPostCheckpointReached() override;
	virtual void OnLevelFinish() override;
	virtual void OnGameOver() override;
	virtual void OnExtraPoint() override;
	virtual void OnPreSubLife() override;
	virtual void OnPostSubLife() override;
	virtual void OnPreLifeUp() override;
	virtual void OnPostLifeUp() override;

private:
	std::map<void*, CKBehavior*> mapScripts;
};