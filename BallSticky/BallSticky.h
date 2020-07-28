#pragma once

#include <BML\BMLAll.h>

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

class BallSticky : public IMod {
public:
	BallSticky(IBML* bml) : IMod(bml) {}

	virtual CKSTRING GetID() override { return "BallSticky"; }
	virtual CKSTRING GetVersion() override { return BML_VERSION; }
	virtual CKSTRING GetName() override { return "Ball Sticky"; }
	virtual CKSTRING GetAuthor() override { return "Gamepiaynmo"; }
	virtual CKSTRING GetDescription() override { return "Add a new ball type that can climb walls."; }

	virtual void OnLoad() override;
	virtual void OnLoadObject(CKSTRING filename, CKSTRING masterName, CK_CLASSID filterClass,
		BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials, BOOL dynamic,
		XObjectArray* objArray, CKObject* masterObj) override;
	virtual void OnLoadScript(CKSTRING filename, CKBehavior* script) override;
	virtual void OnProcess() override;

private:
	void OnLoadLevelinit(XObjectArray* objArray);
	void OnEditScript_Gameplay_Ingame(CKBehavior* script);

	CK3dEntity* m_ballRef[2] = { 0 };
	CK3dEntity* m_camTarget = nullptr;
	CKDataArray* m_curLevel = nullptr;
	float m_stickyImpulse;
	CKParameter* m_stickyForce[2];
};