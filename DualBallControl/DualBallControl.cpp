#include "DualBallControl.h"

IMod* BMLEntry(IBML* bml) {
	return new DualBallControl(bml);
}

void DualBallControl::OnLoad() {
	GetConfig()->SetCategoryComment("Misc", "Miscellaneous");

	m_switchKey = GetConfig()->GetProperty("Misc", "SwitchKey");
	m_switchKey->SetComment("Switch between two balls");
	m_switchKey->SetDefaultKey(CKKEY_X);
}

void DualBallControl::OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
	CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
	BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) {
	if (isMap) {
		m_dualActive = false;
		CK3dObject* ps = m_bml->Get3dObjectByName("PS_FourFlames_01_Dual");
		CKGroup* pcs = m_bml->GetGroupByName("PC_Checkpoints");
		CKGroup* prs = m_bml->GetGroupByName("PR_Resetpoints");
		if (ps != nullptr && pcs != nullptr && prs != nullptr) {
			m_dualFlames.push_back(ps);
			for (int i = 1; i <= pcs->GetObjectCount(); i++) {
				std::string pcname = "PC_TwoFlames_0" + std::to_string(i) + "_Dual";
				CK3dObject* pc = m_bml->Get3dObjectByName(pcname.c_str());
				if (pc != nullptr)
					m_dualFlames.push_back(pc);
			}

			for (int i = 1; i <= prs->GetObjectCount(); i++) {
				std::string prname = "PR_Resetpoint_0" + std::to_string(i) + "_Dual";
				CK3dObject* pr = m_bml->Get3dObjectByName(prname.c_str());
				if (pr != nullptr)
					m_dualResets.push_back(pr);
			}

			if (m_dualFlames.size() == pcs->GetObjectCount() + 1 && m_dualResets.size() == prs->GetObjectCount()) {
				m_dualActive = true;
				for (CK3dObject* obj : m_dualFlames)
					obj->Show(CKHIDE);
				for (CK3dObject* obj : m_dualResets)
					obj->Show(CKHIDE);
				m_dualBallType = 2;
				m_depthTestCubes = m_bml->GetGroupByName("DepthTestCubes");
			}
		}
	}
	
	if (!strcmp(filename, "3D Entities\\Balls.nmo")) {
		CKDataArray* physBall = m_bml->GetArrayByName("Physicalize_GameBall");
		for (int i = 0; i < physBall->GetRowCount(); i++) {
			std::string ballName(physBall->GetElementStringValue(i, 0, nullptr), '\0');
			physBall->GetElementStringValue(i, 0, ballName.data());
			CK3dObject* ballObj = m_bml->Get3dObjectByName(ballName.c_str());
			m_balls.push_back(ballObj);

			CKDependencies dep;
			dep.Resize(40); dep.Fill(0);
			dep.m_Flags = CK_DEPENDENCIES_CUSTOM;
			dep[CKCID_OBJECT] = CK_DEPENDENCIES_COPY_OBJECT_NAME | CK_DEPENDENCIES_COPY_OBJECT_UNIQUENAME;
			dep[CKCID_SCENEOBJECT] = CK_DEPENDENCIES_COPY_SCENEOBJECT_SCENES;
			ballObj = static_cast<CK3dObject*>(m_bml->GetCKContext()->CopyObject(ballObj, &dep, "_Dual"));
			m_dualBalls.push_back(ballObj);

			PhysBall phys;
			phys.surface = ballName + "_Dual_Mesh";
			physBall->GetElementValue(i, 1, &phys.friction);
			physBall->GetElementValue(i, 2, &phys.elasticity);
			physBall->GetElementValue(i, 3, &phys.mass);
			physBall->GetElementValue(i, 5, &phys.linearDamp);
			physBall->GetElementValue(i, 6, &phys.rotDamp);
			m_phys.push_back(phys);

			std::string trafoName = ballName.substr(5);
			std::transform(trafoName.begin(), trafoName.end(), trafoName.begin(), tolower);
			m_trafoTypes.push_back(trafoName);
		}

		CKAttributeManager* am = m_bml->GetAttributeManager();
		CKAttributeType collId = am->GetAttributeTypeByName("Coll Detection ID");
		m_dualBalls[1]->SetAttribute(collId);
		m_dualBalls[2]->SetAttribute(collId);
		ScriptHelper::SetParamValue(m_dualBalls[1]->GetAttributeParameter(collId), 1);
		ScriptHelper::SetParamValue(m_dualBalls[2]->GetAttributeParameter(collId), 2);

		GetLogger()->Info("Created Dual Balls");
	}

	if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
		m_energy = m_bml->GetArrayByName("Energy");
		m_curLevel = m_bml->GetArrayByName("CurrentLevel");
		m_ingameParam = m_bml->GetArrayByName("IngameParameter");
	}

	if (!strcmp(filename, "3D Entities\\Camera.nmo")) {
		m_camMF = m_bml->Get3dEntityByName("Cam_MF");
		m_camTarget = m_bml->Get3dEntityByName("Cam_Target");
		m_camPos = m_bml->Get3dEntityByName("Cam_Pos");
		m_ingameCam = m_bml->GetTargetCameraByName("InGameCam");
	}
}

void DualBallControl::OnLoadScript(CKSTRING filename, CKBehavior* script) {
	if (!strcmp(script->GetName(), "Gameplay_Ingame"))
		OnEditScript_Gameplay_Ingame(script);
}

void DualBallControl::OnEditScript_Gameplay_Ingame(CKBehavior* script) {
	CKBehavior* ballMgr = ScriptHelper::FindFirstBB(script, "BallManager");
	m_dynamicPos = ScriptHelper::FindNextBB(script, ballMgr, "TT Set Dynamic Position");
	m_deactBall = ScriptHelper::FindFirstBB(ballMgr, "Deactivate Ball");

	CKBehavior* trafoMgr = ScriptHelper::FindFirstBB(script, "Trafo Manager");
	m_setNewBall = ScriptHelper::FindFirstBB(trafoMgr, "set new Ball");
	CKBehavior* sop = ScriptHelper::FindFirstBB(m_setNewBall, "Switch On Parameter");
	m_curTrafo = sop->GetInputParameter(0)->GetDirectSource();
	CKBehavior* gnig = ScriptHelper::FindFirstBB(trafoMgr, "Get Nearest In Group");
	m_nearestTrafo = gnig->GetOutputParameter(1);
}

void DualBallControl::OnProcess() {
	if (m_bml->IsPlaying() && m_dualActive) {
		if (m_bml->GetInputManager()->IsKeyPressed(m_switchKey->GetKey()) && m_switchProgress <= 0
			&& m_ballNavActive && ScriptHelper::GetParamValue<float>(m_nearestTrafo) > 4.3) {
			CKMessageManager* mm = m_bml->GetMessageManager();
			CKMessageType ballDeact = mm->AddMessageType("BallNav deactivate");

			mm->SendMessageSingle(ballDeact, m_bml->GetGroupByName("All_Gameplay"));
			mm->SendMessageSingle(ballDeact, m_bml->GetGroupByName("All_Sound"));

			m_bml->AddTimer(2u, [this]() {
				CK3dEntity* curBall = static_cast<CK3dEntity*>(m_curLevel->GetElementObject(0, 1));
				ExecuteBB::Unphysicalize(curBall);

				m_dynamicPos->ActivateInput(1);
				m_dynamicPos->Activate();

				m_bml->AddTimer(1u, [this, curBall]() {
					ReleaseDualBall();

					CK3dEntity* dualBall = m_dualBalls[m_dualBallType];
					VxMatrix dualMatrix = curBall->GetWorldMatrix();
					VxMatrix matrix = dualBall->GetWorldMatrix();

					VxVector position, dualPosition, camPosition;
					curBall->GetPosition(&dualPosition);
					dualBall->GetPosition(&position);
					m_ingameCam->GetPosition(&camPosition, m_camPos);

					curBall->SetWorldMatrix(matrix);
					ScriptHelper::SetParamString(m_curTrafo, m_trafoTypes[m_dualBallType].c_str());
					m_setNewBall->ActivateInput(0);
					m_setNewBall->Activate();

					m_dualBallType = GetCurrentBallType();
					CK3dEntity* newDualBall = m_dualBalls[GetCurrentBallType()];
					newDualBall->SetWorldMatrix(dualMatrix);
					PhysicalizeDualBall();
					m_switchProgress = 1.0f;

					m_bml->AddTimerLoop(1u, [this, position, dualPosition, camPosition]() {
						if (m_switchProgress < 0) {
							m_camTarget->SetPosition(position);
							m_ingameCam->SetPosition(camPosition, m_camPos);

							m_dynamicPos->ActivateInput(0);
							m_dynamicPos->Activate();
							return false;
						}
						else {
							float progress = m_switchProgress * m_switchProgress * m_switchProgress;
							VxVector midPos = (dualPosition - position) * progress + position;
							m_camTarget->SetPosition(midPos);
							m_ingameCam->SetPosition(camPosition, m_camPos);
						}

						m_switchProgress -= m_bml->GetTimeManager()->GetLastDeltaTime() / 200;
						return true;
						});
					});
				});
		}

		if (!m_deactBall->IsActive()) {
			CKCollisionManager* cm = m_bml->GetCollisionManager();
			for (int i = 0; i < m_depthTestCubes->GetObjectCount(); i++) {
				if (cm->BoxBoxIntersection(static_cast<CK3dEntity*>(m_depthTestCubes->GetObject(i)), false, true,
					m_dualBalls[m_dualBallType], false, true)) {

					m_deactBall->ActivateInput(0);
					m_deactBall->Activate();
				}
			}
		}
	}
}

int DualBallControl::GetCurrentSector() {
	int sector;
	m_ingameParam->GetElementValue(0, 1, &sector);
	return sector;
}

int DualBallControl::GetCurrentBallType() {
	CK3dObject* ball = static_cast<CK3dObject*>(m_curLevel->GetElementObject(0, 1));
	for (auto i = 0u; i < m_balls.size(); i++)
		if (m_balls[i] == ball)
			return i;
	return -1;
}

void DualBallControl::ReleaseDualBall() {
	CK3dObject* ball = m_dualBalls[m_dualBallType];
	ball->Show(CKHIDE);
	ExecuteBB::Unphysicalize(ball);
}

void DualBallControl::PhysicalizeDualBall() {
	CK3dObject* ball = m_dualBalls[m_dualBallType];
	ball->Show();
	PhysBall& phys = m_phys[m_dualBallType];
	ExecuteBB::PhysicalizeBall(ball, false, phys.friction, phys.elasticity, phys.mass, "",
		false, true, false, phys.linearDamp, phys.rotDamp, phys.surface.c_str());
}

void DualBallControl::ResetDualBallMatrix() {
	CK3dObject* pr = m_dualResets[GetCurrentSector() - 1];
	CK3dObject* ball = m_dualBalls[m_dualBallType];
	ball->SetWorldMatrix(pr->GetWorldMatrix());
}

void DualBallControl::ReleaseLevel() {
	m_dualResets.clear();
	m_dualFlames.clear();
}

void DualBallControl::OnPostLoadLevel() {
	if (m_dualActive) {
		CKDependencies dep;
		dep.Resize(40); dep.Fill(0);
		dep.m_Flags = CK_DEPENDENCIES_CUSTOM;
		dep[CKCID_OBJECT] = CK_DEPENDENCIES_COPY_OBJECT_NAME | CK_DEPENDENCIES_COPY_OBJECT_UNIQUENAME;
		dep[CKCID_SCENEOBJECT] = CK_DEPENDENCIES_COPY_SCENEOBJECT_SCENES;
		dep[CKCID_BEOBJECT] = CK_DEPENDENCIES_COPY_BEOBJECT_SCRIPTS;

		CK3dEntity* flame = m_bml->Get3dEntityByName("PS_FourFlames_Flame_A");
		CKGroup* allLevel = m_bml->GetGroupByName("All_Level");
		CKScene* scene = m_bml->GetCKContext()->GetCurrentScene();

		int counter = 0;
		auto createFlame = [this, &counter, flame, &dep, allLevel, scene]() {
			std::string suffix = "_Dual_" + std::to_string(counter++);
			CK3dEntity* newFlame = static_cast<CK3dEntity*>(m_bml->GetCKContext()->CopyObject(flame, &dep, suffix.c_str()));
			scene->Activate(newFlame, false);
			allLevel->AddObject(newFlame);
			for (int i = 0; i < newFlame->GetScriptCount(); i++)
				scene->Activate(newFlame->GetScript(i), true);
			return newFlame;
		};

		createFlame()->SetPosition(VxVector(7.3338f, 2.0526f, 6.1448f), m_dualFlames[0]);
		createFlame()->SetPosition(VxVector(-7.2214f, 2.0526f, 6.1448f), m_dualFlames[0]);
		createFlame()->SetPosition(VxVector(-7.2214f, 2.0526f, -6.1318f), m_dualFlames[0]);
		createFlame()->SetPosition(VxVector(7.3338f, 2.0526f, -6.1318f), m_dualFlames[0]);

		for (auto i = 1u; i < m_dualFlames.size(); i++) {
			createFlame()->SetPosition(VxVector(0.0400f, 2.0526f, -6.9423f), m_dualFlames[i]);
			createFlame()->SetPosition(VxVector(0.0392f, 2.0526f, 7.0605f), m_dualFlames[i]);
		}
	}
}