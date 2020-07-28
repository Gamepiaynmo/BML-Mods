#include "BallSticky.h"

using namespace ScriptHelper;

#define VT_KEEPACTIVE CKGUID(0x7160133a,0x1f2532fe)
#define VT_PERSECOND CKGUID(0x448e54ce, 0x75a655c5)
#define VT_REMOVEATTRIBUTE CKGUID(0x6b6340c4,0x61e94a41)
#define TT_PHYSICSIMPLUSE CKGUID(0xc7e39bb,0x16db20d5)

IMod* BMLEntry(IBML* bml) {
	return new BallSticky(bml);
}

void BallSticky::OnLoad() {
	m_stickyImpulse = 90.0f;
	m_bml->RegisterBallType("Ball_Sticky.nmo", "sticky", "Sticky", "Ball_Sticky", 10.0f, 0.0f, 1.4f, "Ball", 0.8f, 7.0f, 0.12f, 2.0f);
	m_bml->RegisterModulBall("P_Ball_Sticky", false, 10.0f, 0.0f, 1.4f, "", false, true, false, 0.8f, 7.0f, 2.0f);
	m_bml->RegisterTrafo("P_Trafo_Sticky");

	for (int i = 0; i < 2; i++) {
		m_ballRef[i] = static_cast<CK3dEntity*>(m_bml->GetCKContext()->CreateObject(CKCID_3DENTITY, "Ball_Sticky_Ref"));
		m_bml->GetCKContext()->GetCurrentScene()->AddObjectToScene(m_ballRef[i]);
	}
}

void BallSticky::OnProcess() {
	if (m_bml->IsIngame() && m_curLevel) {
		CK3dEntity* curBall = static_cast<CK3dEntity*>(m_curLevel->GetElementObject(0, 1));
		if (curBall) {
			VxVector pos;
			curBall->GetPosition(&pos);
			pos.y += 1;
			m_ballRef[0]->SetPosition(&pos);
			pos.y -= 2;
			m_ballRef[1]->SetPosition(&pos);

			bool isSticky = !strcmp(curBall->GetName(), "Ball_Sticky");
			SetParamValue(m_stickyForce[0], isSticky ? m_stickyImpulse : 0);
			SetParamValue(m_stickyForce[1], isSticky ? -m_stickyImpulse : 0);
		}
	}
}

void BallSticky::OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName, CK_CLASSID filterClass,
	BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials, BOOL dynamic,
	XObjectArray* objArray, CKObject* masterObj) {
	if (!strcmp(filename, "3D Entities\\Levelinit.nmo"))
		OnLoadLevelinit(objArray);
}

void BallSticky::OnLoadScript(CKSTRING filename, CKBehavior* script) {
	if (!strcmp(script->GetName(), "Gameplay_Ingame"))
		OnEditScript_Gameplay_Ingame(script);
}

void BallSticky::OnLoadLevelinit(XObjectArray* objArray) {
	// CKDataArray* allLevel = m_bml->GetArrayByName("AllLevel");
	// for (int i = 0; i < allLevel->GetRowCount(); i++)
	// 	allLevel->SetElementStringValue(i, 1, "Ball_Sticky");
}

void BallSticky::OnEditScript_Gameplay_Ingame(CKBehavior* script) {
	m_curLevel = m_bml->GetArrayByName("CurrentLevel");

	CKBehavior* ballNav = FindFirstBB(script, "Ball Navigation");
	CKBehavior* oForce = FindFirstBB(ballNav, "SetPhysicsForce");
	CKBehavior* forces[8], * keepActive[8], * perSecond[8];
	m_stickyForce[0] = CreateParamValue(ballNav, "Force", CKPGUID_FLOAT, m_stickyImpulse);
	m_stickyForce[1] = CreateParamValue(ballNav, "Force", CKPGUID_FLOAT, -m_stickyImpulse);
	CKParameter* posRef[2] = { CreateParamObject(ballNav, "PosRef", CKPGUID_3DENTITY, m_ballRef[0]),
		CreateParamObject(ballNav, "PosRef", CKPGUID_3DENTITY, m_ballRef[1]) };
	for (int i = 0; i < 8; i++) {
		keepActive[i] = CreateBB(ballNav, VT_KEEPACTIVE);
		perSecond[i] = CreateBB(ballNav, VT_PERSECOND);
		perSecond[i]->GetInputParameter(0)->SetDirectSource(m_stickyForce[i % 2]);
		CreateLink(ballNav, keepActive[i], perSecond[i], 1);
		forces[i] = CreateBB(ballNav, TT_PHYSICSIMPLUSE, true);
		forces[i]->GetTargetParameter()->ShareSourceWith(oForce->GetTargetParameter());
		forces[i]->GetInputParameter(0)->ShareSourceWith(oForce->GetInputParameter(0));
		forces[i]->GetInputParameter(1)->SetDirectSource(posRef[i % 2]);
		forces[i]->GetInputParameter(3)->ShareSourceWith(oForce->GetInputParameter(3));
		forces[i]->GetInputParameter(4)->SetDirectSource(perSecond[i]->GetOutputParameter(0));
		CreateLink(ballNav, perSecond[i], forces[i]);
	}

	int cnt = 0;
	FindBB(ballNav, [ballNav, forces, keepActive, &cnt](CKBehavior* beh) {
		VxVector dir;
		beh->GetInputParameterValue(2, &dir);
		int idx = -1;
		if (dir.x == 1) idx = 0;
		if (dir.x == -1) idx = 2;
		if (dir.z == 1) idx = 4;
		if (dir.z == -1) idx = 6;
		if (idx >= 0) {
			cnt++;
			for (int i = idx; i < idx + 2; i++) {
				forces[i]->GetInputParameter(2)->ShareSourceWith(beh->GetInputParameter(2));
				CreateLink(ballNav, beh, keepActive[i], 0, 1);
				CreateLink(ballNav, beh, keepActive[i], 1, 0);
			}
		}
		return cnt < 4;
		}, "SetPhysicsForce");
}