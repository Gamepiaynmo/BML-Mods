#pragma once

#include <BML/BMLAll.h>
#include <vector>

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

class DualBallControl : public IMod {
public:
	DualBallControl(IBML* bml) : IMod(bml) {}

	virtual CKSTRING GetID() override { return "DualBallControl"; }
	virtual CKSTRING GetVersion() override { return BML_VERSION; }
	virtual CKSTRING GetName() override { return "Dual Ball Control"; }
	virtual CKSTRING GetAuthor() override { return "Zzq_203 & Gamepiaynmo"; }
	virtual CKSTRING GetDescription() override { return "Support loading dual ball maps."; }
	DECLARE_BML_VERSION;

	virtual void OnLoad() override;
	virtual void OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
		CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
		BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) override;
	virtual void OnLoadScript(CKSTRING filename, CKBehavior* script) override;
	virtual void OnProcess() override;

	virtual void OnPostLoadLevel() override;
	virtual void OnStartLevel() override {
		if (m_dualActive) {
			m_bml->AddTimer(1u, [this]() {
				ResetDualBallMatrix();
				PhysicalizeDualBall();
				});
		}
	}
	virtual void OnBallNavActive() override { m_ballNavActive = true; }
	virtual void OnBallNavInactive() override { m_ballNavActive = false; }

	virtual void OnPostResetLevel() override { if (m_dualActive) { ReleaseDualBall(); m_dualBallType = 2; } }
	virtual void OnPostExitLevel() override { if (m_dualActive) { ReleaseDualBall(); ReleaseLevel(); } }
	virtual void OnPostNextLevel() override { if (m_dualActive) { ReleaseDualBall(); ReleaseLevel(); } }
	virtual void OnBallOff() override {
		if (m_dualActive) {
			int lifes;
			m_energy->GetElementValue(0, 1, &lifes);
			m_bml->AddTimer(1000.0f, [this, lifes]() {
				ReleaseDualBall();
				if (lifes > 0) {
					ResetDualBallMatrix();
					PhysicalizeDualBall();
				}
				});
		}
	}

private:
	void OnEditScript_Gameplay_Ingame(CKBehavior* script);

	int GetCurrentSector();
	int GetCurrentBallType();

	void ReleaseDualBall();
	void PhysicalizeDualBall();
	void ResetDualBallMatrix();
	void ReleaseLevel();

	IProperty* m_switchKey;
	bool m_dualActive = false;
	bool m_ballNavActive = false;
	float m_switchProgress = 0.0f;

	std::vector<CK3dObject*> m_balls, m_dualBalls;
	std::vector<std::string> m_trafoTypes;
	CKDataArray* m_energy, * m_curLevel, * m_ingameParam;

	struct PhysBall {
		std::string surface;
		float friction, elasticity, mass, linearDamp, rotDamp;
	};
	std::vector<PhysBall> m_phys;

	std::vector<CK3dObject*> m_dualResets, m_dualFlames;
	int m_dualBallType;
	CKGroup* m_depthTestCubes;

	CK3dEntity* m_camTarget, * m_camMF, * m_camPos;
	CKTargetCamera* m_ingameCam;
	CKBehavior* m_setNewBall, * m_dynamicPos, * m_deactBall;
	CKParameter* m_curTrafo, * m_nearestTrafo;
};