#pragma once

#include <BML/BMLAll.h>

extern class DynamicFov* g_mod;

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
	__declspec(dllexport) void RegisterBB(XObjectDeclarationArray* reg);
}

class DynamicFov : public IMod {
public:
	DynamicFov(IBML* bml) : IMod(bml) { g_mod = this; }

	virtual CKSTRING GetID() override { return "DynamicFov"; }
	virtual CKSTRING GetVersion() override { return BML_VERSION; }
	virtual CKSTRING GetName() override { return "Dynamic Fov"; }
	virtual CKSTRING GetAuthor() override { return "Gamepiaynmo"; }
	virtual CKSTRING GetDescription() override { return "Dynamically adjust camera fov according to ball speed."; }
	DECLARE_BML_VERSION;

	virtual void OnLoad() override;
	virtual void OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
		CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
		BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) override;
	virtual void OnLoadScript(CKSTRING filename, CKBehavior* script) override;
	virtual void OnProcess() override;
	virtual void OnModifyConfig(CKSTRING category, CKSTRING key, IProperty* prop) override;

	void SetInactive() { m_bml->AddTimer(1u, [this]() { m_isActive = false; }); }

private:
	CKCamera* m_ingameCam;
	CKDataArray* m_curLevel;
	CKBehavior* m_ingameScript;
	CKBehavior* m_dynamicPos;

	bool m_isActive = false;
	VxVector m_lastPos;
	bool m_wasPaused = false;

	IProperty* m_enabled;
};