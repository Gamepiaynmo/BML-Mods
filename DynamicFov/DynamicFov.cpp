#include "DynamicFov.h"

#define HOOK_GUID CKGUID(0x6bed00f0, 0x7a91139b)

DynamicFov* g_mod;

IMod* BMLEntry(IBML* bml) {
	return new DynamicFov(bml);
}

void RegisterBB(XObjectDeclarationArray* reg) {
	CKStoreDeclaration(reg, (new HookBuilder())
		->SetName("BML OnResetBallPosition")
		->SetDescription("OnResetBallPosition Hook")
		->SetGuid(HOOK_GUID)
		->SetProcessFunction([](HookParams* params) {
			g_mod->SetInactive();
			return false;
			})->Build());
}

void DynamicFov::OnLoad() {
	GetConfig()->SetCategoryComment("Misc", "Miscellaneous");
	m_enabled = GetConfig()->GetProperty("Misc", "Enable");
	m_enabled->SetComment("Enable Dynamic Fov");
	m_enabled->SetDefaultBoolean(true);
}

void DynamicFov::OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName, CK_CLASSID filterClass,
	BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials, BOOL dynamic,
	XObjectArray* objArray, CKObject* masterObj) {
	if (!strcmp(filename, "3D Entities\\Camera.nmo")) {
		m_ingameCam = m_bml->GetTargetCameraByName("InGameCam");
	}

	if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
		m_curLevel = m_bml->GetArrayByName("CurrentLevel");
	}
}

void DynamicFov::OnLoadScript(CKSTRING filename, CKBehavior* script) {
	if (!strcmp(script->GetName(), "Gameplay_Ingame")) {
		m_ingameScript = script;
		CKBehavior* ballMgr = ScriptHelper::FindFirstBB(script, "BallManager");
		ScriptHelper::CreateLink(script, ballMgr, ScriptHelper::CreateBB(script, HOOK_GUID));
		CKBehavior* init = ScriptHelper::FindFirstBB(script, "Init Ingame");
		m_dynamicPos = ScriptHelper::FindNextBB(script, init, "TT Set Dynamic Position");
	}
}

void DynamicFov::OnProcess() {
	if (m_ingameScript && m_ingameCam && m_enabled->GetBoolean()) {
		if (m_ingameScript->IsActive() && m_dynamicPos->IsActive() && !m_dynamicPos->IsOutputActive(1)) {
			CK3dObject* ball = static_cast<CK3dObject*>(m_curLevel->GetElementObject(0, 1));
			if (ball != nullptr) {
				VxVector position;
				ball->GetPosition(&position);
				CKRenderContext* rc = m_bml->GetRenderContext();

				if (!m_isActive) {
					m_lastPos = position;
					m_ingameCam->SetFov(0.75f * rc->GetWidth() / rc->GetHeight());
				}
				else {
					float delta = m_bml->GetTimeManager()->GetLastDeltaTime();
					float speed = (position - m_lastPos).Magnitude() / delta * 6;
					float newfov = (0.75f + speed) * rc->GetWidth() / rc->GetHeight();
					newfov = (std::min)(newfov, PI / 2);
					float curfov = m_ingameCam->GetFov();
					m_ingameCam->SetFov((newfov - curfov) * (std::min)(delta, 20.0f) / 1000 + curfov);
				}

				m_isActive = true;
				m_lastPos = position;
			}
		}
		else if (!m_wasPaused) {
			m_isActive = false;
		}

		m_wasPaused = m_bml->IsPaused();
	}
}

void DynamicFov::OnModifyConfig(CKSTRING category, CKSTRING key, IProperty* prop) {
	if (prop == m_enabled && !m_enabled->GetBoolean()) {
		CKRenderContext* rc = m_bml->GetRenderContext();
		m_ingameCam->SetFov(0.75f * rc->GetWidth() / rc->GetHeight());
		m_isActive = false;
	}
}