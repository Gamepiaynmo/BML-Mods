#pragma once

#include <BML/BMLAll.h>

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

class DeformedWB : public IMod {
public:
	DeformedWB(IBML* bml) : IMod(bml) {}

	virtual CKSTRING GetID() override { return "DeformedWB"; }
	virtual CKSTRING GetVersion() override { return BML_VERSION; }
	virtual CKSTRING GetName() override { return "Deformed Wooden Ball"; }
	virtual CKSTRING GetAuthor() override { return "Gamepiaynmo"; }
	virtual CKSTRING GetDescription() override { return "Randomly deform the player wooden ball. This does not affect the physical volume."; }
	DECLARE_BML_VERSION;

	virtual void OnLoad() override;
	virtual void OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
		CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
		BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) override;
	virtual void OnStartLevel() override;

	CKMesh* m_ballMesh;
	std::vector<VxVector> m_vertices, m_normals;
	IProperty* m_enabled, * m_extent;
};
