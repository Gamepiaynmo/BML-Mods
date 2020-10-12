#pragma once

#include <BML/BMLAll.h>

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

class TASSupport : public IMod {
public:
	TASSupport(IBML* bml) : IMod(bml) {}

	virtual CKSTRING GetID() override { return "TASSupport"; }
	virtual CKSTRING GetVersion() override { return BML_VERSION; }
	virtual CKSTRING GetName() override { return "TAS Support"; }
	virtual CKSTRING GetAuthor() override { return "Gamepiaynmo"; }
	virtual CKSTRING GetDescription() override { return "Make TAS possible in Ballance (WIP)."; }
	DECLARE_BML_VERSION;

	virtual void OnLoad() override;
	virtual void OnUnload() override;
	virtual void OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
		CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
		BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) override;
	virtual void OnLoadScript(CKSTRING filename, CKBehavior* script) override;
	virtual void OnProcess() override;

	virtual void OnPreLoadLevel() override;
	virtual void OnStartLevel() override;

	static int HookPhysicalize(const CKBehaviorContext& behcontext);

	ILogger* Logger() { return GetLogger(); }

private:
	CKDataArray* m_curLevel;
};