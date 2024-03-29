#include "TASSupport.h"
#include "minhook/MinHook.h"
#include <ctime>
#include <thread>
#include <filesystem>

TASSupport* g_mod;

IMod* BMLEntry(IBML* bml) {
	return g_mod = new TASSupport(bml);
}

std::pair<char*, int> ReadDataFromFile(const char* filename) {
	FILE* fp = fopen(filename, "rb");
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char* buffer = new char[size];
	fread(buffer, size, 1, fp);
	fclose(fp);
	return { buffer, size };
}

void WriteDataToFile(char* data, int size, const char* filename) {
	FILE* fp = fopen(filename, "wb");
	fwrite(data, size, 1, fp);
	fclose(fp);
}

std::pair<char*, int> UncompressDataFromFile(const char* filename) {
	FILE* fp = fopen(filename, "rb");
	fseek(fp, 0, SEEK_END);
	int nsize = ftell(fp) - 4;
	fseek(fp, 0, SEEK_SET);

	int size;
	char* buffer = new char[nsize];
	fread(&size, 4, 1, fp);
	fread(buffer, nsize, 1, fp);
	fclose(fp);

	char* res = CKUnPackData(size, buffer, nsize);
	delete[] buffer;
	return { res, size };
}

void CompressDataToFile(char* data, int size, const char* filename) {
	int nsize;
	char* res = CKPackData(data, size, nsize, 9);

	FILE* fp = fopen(filename, "wb");
	fwrite(&size, 4, 1, fp);
	fwrite(res, nsize, 1, fp);
	fclose(fp);

	CKDeletePointer(res);
}

//BOOL WINAPI HookQueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
//	lpPerformanceCount->QuadPart = 0;
//	return true;
//}
//
//auto hookCount = &HookQueryPerformanceCounter;
//
//clock_t hook_clock(void) {
//	return 0;
//}
//
//auto hookClock = &hook_clock;
//
//time_t hook_time(time_t* const _Time) {
//	if (_Time) *_Time = 0;
//	return 0;
//}
//
//auto hookTime = &hook_time;

#ifdef _DEBUG
void TASSupport::OnPhysicalize(CK3dEntity* target, CKBOOL fixed, float friction, float elasticity, float mass,
	CKSTRING collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter, float linearDamp,
	float rotDamp, CKSTRING collSurface, VxVector massCenter, int convexCnt, CKMesh** convexMesh,
	int ballCnt, VxVector* ballCenter, float* ballRadius, int concaveCnt, CKMesh** concaveMesh) {

	//CK3dEntity* curBall = static_cast<CK3dEntity*>(m_curLevel->GetElementObject(0, 1));
	//if (curBall == target) {
		CKSTRING name = target->GetName();
		VxVector pos;
		reinterpret_cast<CK3dEntity*>(target)->GetPosition(&pos);
		g_mod->Logger()->Info("Physicalize %s %f %f %f", name, pos.x, pos.y, pos.z);
	//}
}

void TASSupport::OnUnphysicalize(CK3dEntity* target) {

	//CK3dEntity* curBall = static_cast<CK3dEntity*>(m_curLevel->GetElementObject(0, 1));
	//if (curBall == target) {
		CKSTRING name = target->GetName();
		VxVector pos;
		reinterpret_cast<CK3dEntity*>(target)->GetPosition(&pos);
		g_mod->Logger()->Info("Unphysicalize %s %f %f %f", name, pos.x, pos.y, pos.z);
	//}
}
#endif

class HookInputManager : public CKInputManager {
public:
	typedef CKERROR(HookInputManager::* PreProcessFunc)();

	static PreProcessFunc m_preProcess;

	CKERROR HookPreProcess() {
		CKERROR ret = (this->*m_preProcess)();

		if (g_mod->m_playing) {
			if (g_mod->m_curFrame < g_mod->m_recordData.size()) {
				KeyState state = g_mod->m_recordData[g_mod->m_curFrame++].keyState;
				BYTE* stateBuf = GetKeyboardState();
				stateBuf[g_mod->m_key_up] = state.key_up;
				stateBuf[g_mod->m_key_down] = state.key_down;
				stateBuf[g_mod->m_key_left] = state.key_left;
				stateBuf[g_mod->m_key_right] = state.key_right;
				stateBuf[CKKEY_Q] = state.key_q;
				stateBuf[g_mod->m_key_shift] = state.key_shift;
				stateBuf[g_mod->m_key_space] = state.key_space;
				stateBuf[CKKEY_ESCAPE] = state.key_esc;
			}
			else {
				g_mod->OnStop();
			}
		}

		if (g_mod->m_recording) {
			KeyState state;
			BYTE* stateBuf = GetKeyboardState();
			state.key_up = stateBuf[g_mod->m_key_up];
			state.key_down = stateBuf[g_mod->m_key_down];
			state.key_left = stateBuf[g_mod->m_key_left];
			state.key_right = stateBuf[g_mod->m_key_right];
			state.key_q = stateBuf[CKKEY_Q];
			state.key_shift = stateBuf[g_mod->m_key_shift];
			state.key_space = stateBuf[g_mod->m_key_space];
			state.key_esc = stateBuf[CKKEY_ESCAPE];
			g_mod->m_recordData.rbegin()->keyState = state;
		}

		return ret;
	}
};

HookInputManager::PreProcessFunc HookInputManager::m_preProcess = &HookInputManager::HookPreProcess;

class HookTimeManager : public CKTimeManager {
public:
	typedef CKERROR(HookTimeManager::* PreProcessFunc)();

	//static CKDWORD tickCount;
	static PreProcessFunc m_preProcess;

	CKERROR HookPreProcess() {
		CKERROR ret = (this->*m_preProcess)();

		//tickCount++;
		if (g_mod->m_recording) {
			g_mod->m_recordData.push_back(FrameData(GetLastDeltaTime()));
		}

		if (g_mod->m_playing) {
			if (g_mod->m_curFrame < g_mod->m_recordData.size())
				SetLastDeltaTime(g_mod->m_recordData[g_mod->m_curFrame].deltaTime);
			else g_mod->OnStop();
		}

		return ret;
	}
};

//CKDWORD HookTimeManager::tickCount = 0;
HookTimeManager::PreProcessFunc HookTimeManager::m_preProcess = &HookTimeManager::HookPreProcess;

//void WriteMemory(LPVOID dest, LPVOID src, int len) {
//	DWORD oldFlag;
//	VirtualProtect(dest, len, PAGE_EXECUTE_READWRITE, &oldFlag);
//	memcpy(dest, src, len);
//	VirtualProtect(dest, len, oldFlag, &oldFlag);
//}

void TASSupport::OnLoad() {
	//void* countHook = (reinterpret_cast<BYTE*>(LoadLibrary("Physics_RT.dll")) + 0x630E8);
	//WriteMemory(countHook, &hookCount, sizeof(&hookCount));

	//void* clockHook = (reinterpret_cast<BYTE*>(LoadLibrary("Physics_RT.dll")) + 0x63154);
	//WriteMemory(clockHook, &hookClock, sizeof(&hookClock));

	//void* timeHook = (reinterpret_cast<BYTE*>(LoadLibrary("Physics_RT.dll")) + 0x63190);
	//WriteMemory(timeHook, &hookTime, sizeof(&hookTime));

	//BYTE randcode[] = { 0x33, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90 };
	//void* randHook = (reinterpret_cast<BYTE*>(LoadLibrary("Physics_RT.dll")) + 0x52f95);
	//WriteMemory(randHook, randcode, sizeof(randcode));

	std::filesystem::create_directories("..\\ModLoader\\TASRecords\\");

	GetConfig()->SetCategoryComment("Misc", "Miscellaneous");
	m_enabled = GetConfig()->GetProperty("Misc", "Enable");
	m_enabled->SetComment("Enable TAS Features");
	m_enabled->SetDefaultBoolean(false);

	m_record = GetConfig()->GetProperty("Misc", "Record");
	m_record->SetComment("Record Actions");
	m_record->SetDefaultBoolean(false);

	m_stopKey = GetConfig()->GetProperty("Misc", "StopKey");
	m_stopKey->SetComment("Key for stop playing");
	m_stopKey->SetDefaultKey(CKKEY_F3);

	m_showKeys = GetConfig()->GetProperty("Misc", "ShowKeysGui");
	m_showKeys->SetComment("Show realtime keyboard behavior for TAS records");
	m_showKeys->SetDefaultBoolean(true);

	m_skipRender = GetConfig()->GetProperty("Misc", "SkipRenderUntil");
	m_skipRender->SetComment("Skip render until the given frame to speed up TAS playing");
	m_skipRender->SetDefaultInteger(0);

	m_exitOnDead = GetConfig()->GetProperty("Misc", "ExitOnDead");
	m_exitOnDead->SetComment("Automatically exit game when ball fell");
	m_exitOnDead->SetDefaultBoolean(false);

	m_exitOnFinish = GetConfig()->GetProperty("Misc", "ExitOnFinish");
	m_exitOnFinish->SetComment("Automatically exit game when TAS playing finished");
	m_exitOnFinish->SetDefaultBoolean(false);

	m_exitKey = GetConfig()->GetProperty("Misc", "ExitKey");
	m_exitKey->SetComment("Press to exit game quickly");
	m_exitKey->SetDefaultKey(CKKEY_DELETE);

	m_loadTAS = GetConfig()->GetProperty("Misc", "AutoLoadTAS");
	m_loadTAS->SetComment("Automatically load TAS record on game startup");
	m_loadTAS->SetDefaultString("");

	m_loadLevel = GetConfig()->GetProperty("Misc", "AutoLoadLevel");
	m_loadLevel->SetComment("Automatically load given level on game startup");
	m_loadLevel->SetDefaultInteger(0);

	if (m_enabled->GetBoolean()) {
		MH_Initialize();

		if (MH_CreateHook(reinterpret_cast<LPVOID>(0x24017F8C),
			*reinterpret_cast<LPVOID*>(&HookTimeManager::m_preProcess),
			reinterpret_cast<LPVOID*>(&HookTimeManager::m_preProcess)) != MH_OK
			|| MH_EnableHook(reinterpret_cast<LPVOID>(0x24017F8C)) != MH_OK) {
			GetLogger()->Error("Create Time Manager Hook Failed");
			return;
		}

		if (MH_CreateHook(reinterpret_cast<LPVOID>(0x24AC1C30),
			*reinterpret_cast<LPVOID*>(&HookInputManager::m_preProcess),
			reinterpret_cast<LPVOID*>(&HookInputManager::m_preProcess)) != MH_OK
			|| MH_EnableHook(reinterpret_cast<LPVOID>(0x24AC1C30)) != MH_OK) {
			GetLogger()->Error("Create Input Manager Hook Failed");
			return;
		}

		m_tasEntryGui = new BGui::Gui();
		m_tasEntry = m_tasEntryGui->AddSmallButton("M_Enter_TAS_Records", "TAS", 0.88f, 0.61f, [this]() {
			m_exitStart->ActivateInput(0);
			m_exitStart->Activate();
			m_tasListGui->SetVisible(true);
			});
		m_tasEntry->SetActive(false);
		m_tasListGui = new GuiTASList();

		m_keysGui = new BGui::Gui();
		m_keysGui->AddPanel("M_TAS_Keys_Bg", VxColor(0, 0, 0, 180), 0.28f, 0.74f, 0.45f, 0.12f);
		m_butUp = m_keysGui->AddSmallButton("M_TAS_Keys_Up", "^", 0.76f, 0.56f);
		m_butDown = m_keysGui->AddSmallButton("M_TAS_Keys_Down", "v", 0.8f, 0.56f);
		m_butLeft = m_keysGui->AddSmallButton("M_TAS_Keys_Left", "<", 0.8f, 0.48f);
		m_butRight = m_keysGui->AddSmallButton("M_TAS_Keys_Right", ">", 0.8f, 0.64f);
		m_butShift = m_keysGui->AddSmallButton("M_TAS_Keys_Shift", "Shift", 0.8f, 0.30f);
		m_butSpace = m_keysGui->AddSmallButton("M_TAS_Keys_Space", "Space", 0.8f, 0.38f);
		m_butQ = m_keysGui->AddSmallButton("M_TAS_Keys_Q", "Q", 0.76f, 0.38f);
		m_butEsc = m_keysGui->AddSmallButton("M_TAS_Keys_Esc", "Esc", 0.76f, 0.30f);
		m_frameCountLabel = m_keysGui->AddTextLabel("M_TAS_FrameCount", "", ExecuteBB::GAMEFONT_01, 0.0f, 0.7f, 1.0f, 0.0353f);
		m_frameCountLabel->SetAlignment(ALIGN_CENTER);
		m_butUp->SetActive(false);
		m_butDown->SetActive(false);
		m_butLeft->SetActive(false);
		m_butRight->SetActive(false);
		m_butShift->SetActive(false);
		m_butSpace->SetActive(false);
		m_butQ->SetActive(false);
		m_butEsc->SetActive(false);
		m_keysGui->SetVisible(false);
	}
}

void TASSupport::OnUnload() {
	if (m_enabled->GetBoolean()) {
		MH_Uninitialize();
	}
}

void TASSupport::OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
	CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
	BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) {
	if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
		m_curLevel = m_bml->GetArrayByName("CurrentLevel");
	}

	if (!strcmp(filename, "3D Entities\\Menu.nmo")) {
		m_level01 = m_bml->Get2dEntityByName("M_Start_But_01");
		CKBehavior* menuStart = m_bml->GetScriptByName("Menu_Start");
		m_exitStart = ScriptHelper::FindFirstBB(menuStart, "Exit");
		m_keyboard = m_bml->GetArrayByName("Keyboard");
		CKBehavior* menuMain = m_bml->GetScriptByName("Menu_Main");
		m_exitMain = ScriptHelper::FindFirstBB(menuMain, "Exit", false, 1, 0);
	}

	if (isMap) {
		m_mapName = filename;
		m_mapName = m_mapName.substr(m_mapName.find_last_of('\\') + 1);
		m_mapName = m_mapName.substr(0, m_mapName.find_last_of('.'));
	}
}

void TASSupport::OnLoadScript(CKSTRING filename, CKBehavior* script) {
	if (m_enabled->GetBoolean()) {
		if (!strcmp(script->GetName(), "Ball_Explosion_Wood")
			|| !strcmp(script->GetName(), "Ball_Explosion_Paper")
			|| !strcmp(script->GetName(), "Ball_Explosion_Stone")) {
			CKBehavior* beh = ScriptHelper::FindFirstBB(script, "Set Position");
			ScriptHelper::DeleteBB(script, beh);
		}

		if (!strcmp(script->GetName(), "Balls_Init")) {
			m_initPieces = ScriptHelper::FindFirstBB(script, "Init Ballpieces");
		}
	}
}

void TASSupport::OnProcess() {
	if (m_bml->IsPlaying()) {
		/*CK3dEntity* curBall = static_cast<CK3dEntity*>(m_curLevel->GetElementObject(0, 1));
		VxVector pos;
		curBall->GetPosition(&pos);
		GetLogger()->Info("%f %f %f %x %x %x", pos.x, pos.y, pos.z, *(DWORD*)&pos.x, *(DWORD*)&pos.y, *(DWORD*)&pos.z);

		CKBaseManager* physicsManager = m_bml->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
		int objCnt = *reinterpret_cast<short*>(reinterpret_cast<BYTE*>(physicsManager) + 0x2A);
		void** objList = *reinterpret_cast<void***>(reinterpret_cast<BYTE*>(physicsManager) + 0x2C);
		for (int i = 0; i < objCnt; i++) {
			void* obj = objList[i];
			BYTE* posptr = *reinterpret_cast<BYTE**>(reinterpret_cast<BYTE*>(obj) + 0xA4);
			double* pos = reinterpret_cast<double*>(posptr + 0xB8);
			float* speed = reinterpret_cast<float*>(posptr + 0xD8);
			GetLogger()->Info("Physics Process %lf %lf %lf %f %f %f", pos[0], pos[1], pos[2], speed[0], speed[1], speed[2]);
		}*/

		//GetLogger()->Info("Tick %d", HookTimeManager::tickCount);
		//VxVector pos;
		//CK3dEntity* stoneball = m_bml->Get3dObjectByName("P_Ball_Stone_MF");
		//stoneball->GetPosition(&pos);
		//GetLogger()->Info("%d %f %f %f %x %x %x", 0, pos.x, pos.y, pos.z, *(DWORD*)&pos.x, *(DWORD*)&pos.y, *(DWORD*)&pos.z);

		//char ballname[100];
		//for (int i = 1; i < 16; i++) {
		//	sprintf(ballname, "P_Ball_Stone_MF%03d", i);
		//	CK3dEntity* stoneball = m_bml->Get3dObjectByName(ballname);
		//	stoneball->GetPosition(&pos);
		//	GetLogger()->Info("%d %f %f %f %x %x %x", i, pos.x, pos.y, pos.z, *(DWORD*)&pos.x, *(DWORD*)&pos.y, *(DWORD*)&pos.z);
		//}
	}

	if (m_playing || m_recording) {
		//CK3dEntity* curBall = static_cast<CK3dEntity*>(m_curLevel->GetElementObject(0, 1));
		//if (curBall != nullptr) {
		//	VxVector pos;
		//	curBall->GetPosition(&pos);
		//	GetLogger()->Info("%d %d %f %f %f %f %f", m_curFrame, m_recordData.size(), m_bml->GetTimeManager()->GetLastDeltaTime(),
		//		pos.x, pos.y, pos.z, m_bml->GetSRScore());
		//}
		//else {
		//	GetLogger()->Info("%d %d %f", m_curFrame, m_recordData.size(), m_bml->GetTimeManager()->GetLastDeltaTime());

		//	//std::function<void(CKBehavior*, int)> active = [this, &active](CKBehavior* script, int depth) {
		//	//	int num = script->GetSubBehaviorCount();
		//	//	for (int j = 0; j < num; j++) {
		//	//		CKBehavior* beh = script->GetSubBehavior(j);
		//	//		if (beh->IsActive()) {
		//	//			GetLogger()->Info("%s%s", std::string(depth, ' ').c_str(), beh->GetName());
		//	//			if (!beh->IsUsingFunction())
		//	//				active(beh, depth + 1);
		//	//		}
		//	//	}
		//	//};

		//	//CKContext* context = m_bml->GetCKContext();
		//	//int cnt = context->GetObjectsCountByClassID(CKCID_BEHAVIOR);
		//	//CK_ID* scripts = context->GetObjectsListByClassID(CKCID_BEHAVIOR);
		//	//bool act = false;
		//	//for (int i = 0; i < cnt; i++) {
		//	//	CKBehavior* script = (CKBehavior*)context->GetObject(scripts[i]);
		//	//	if (script->GetType() == CKBEHAVIORTYPE_SCRIPT) {
		//	//		// delay(script);
		//	//		if (script->IsActive()) {
		//	//			GetLogger()->Info("%s", script->GetName());
		//	//			act = true;
		//	//			active(script, 1);
		//	//		}
		//	//	}
		//	//}
		//}

		//CKBaseManager* physicsManager = m_bml->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
		//BYTE* timer = *reinterpret_cast<BYTE**>(reinterpret_cast<BYTE*>(physicsManager) + 0xC0);

		//GetLogger()->Info("%f %f %llx %llx", m_bml->GetTimeManager()->GetLastDeltaTime(),
		//	m_bml->GetTimeManager()->GetLastDeltaTimeFree(),
		//	*reinterpret_cast<double*>(reinterpret_cast<BYTE*>(physicsManager) + 0xC8),
		//	*reinterpret_cast<double*>(timer + 0x120));
	}

	if (m_enabled->GetBoolean()) {
#ifndef _DEBUG
		if (m_bml->IsCheatEnabled() && m_recording)
			OnStop();
#endif

		if (m_tasEntryGui && m_level01) {
			m_tasEntryGui->Process();
			bool visible = m_level01->IsVisible();
			m_tasEntry->SetVisible(visible);

			if (m_tasListGui->IsVisible())
				m_tasListGui->Process();
		}

		if (m_playing) {
			if (m_showKeys->GetBoolean() && m_keysGui && m_curFrame < m_recordData.size()) {
				KeyState state = m_recordData[m_curFrame].keyState;
				state.key_up ? m_butUp->OnMouseEnter() : m_butUp->OnMouseLeave();
				m_butUp->Process();
				state.key_down ? m_butDown->OnMouseEnter() : m_butDown->OnMouseLeave();
				m_butDown->Process();
				state.key_left ? m_butLeft->OnMouseEnter() : m_butLeft->OnMouseLeave();
				m_butLeft->Process();
				state.key_right ? m_butRight->OnMouseEnter() : m_butRight->OnMouseLeave();
				m_butRight->Process();
				state.key_shift ? m_butShift->OnMouseEnter() : m_butShift->OnMouseLeave();
				m_butShift->Process();
				state.key_space ? m_butSpace->OnMouseEnter() : m_butSpace->OnMouseLeave();
				m_butSpace->Process();
				state.key_q ? m_butQ->OnMouseEnter() : m_butQ->OnMouseLeave();
				m_butQ->Process();
				state.key_esc ? m_butEsc->OnMouseEnter() : m_butEsc->OnMouseLeave();
				m_butEsc->Process();
				sprintf(m_frameCountText, "#%d", m_curFrame);
				m_frameCountLabel->SetText(m_frameCountText);
				m_frameCountLabel->Process();
			}

			if (m_bml->GetInputManager()->IsKeyPressed(m_stopKey->GetKey()))
				OnStop();

			if (m_curFrame < m_skipRender->GetInteger())
				m_bml->SkipRenderForNextTick();

			if (m_bml->GetInputManager()->IsKeyPressed(m_exitKey->GetKey()))
				m_bml->ExitGame();
		}
	}
}

void TASSupport::OnPostStartMenu() {
	static bool firstTime = true;

	if (firstTime) {
		std::string tasFile = m_loadTAS->GetString();
		if (m_enabled->GetBoolean() && !tasFile.empty()) {
			std::string tasPath = "..\\ModLoader\\TASRecords\\" + tasFile + ".tas";
			if (std::filesystem::exists(tasPath)) {
				g_mod->GetBML()->SendIngameMessage(("Loading TAS Record: " + tasFile + ".tas").c_str());
				LoadTAS(tasPath);

				int level = m_loadLevel->GetInteger();
				if (level >= 1 && level <= 13) {
					m_bml->AddTimer(2u, [this, level]() {
						m_curLevel->SetElementValue(0, 0, &level);

						CKContext* ctx = m_bml->GetCKContext();
						CKMessageManager* mm = ctx->GetMessageManager();
						CKMessageType loadLevel = mm->AddMessageType("Load Level");
						CKMessageType loadMenu = mm->AddMessageType("Menu_Load");

						mm->SendMessageSingle(loadLevel, ctx->GetCurrentLevel());
						mm->SendMessageSingle(loadMenu, m_bml->GetGroupByName("All_Sound"));
						m_bml->Get2dEntityByName("M_BlackScreen")->Show(CKHIDE);
						m_exitMain->ActivateInput(0);
						m_exitMain->Activate();
						});
				}
			}
			else m_bml->SendIngameMessage(("TAS file " + tasFile + ".tas not found.").c_str());
		}

		firstTime = false;
	}
}

void TASSupport::OnBallOff() {
	if (m_enabled->GetBoolean() && m_playing && m_exitOnDead->GetBoolean())
		m_bml->ExitGame();
}

#ifdef _DEBUG
void TASSupport::OnPostExitLevel() {
	CKBaseManager* physicsManager = m_bml->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
	BYTE* moduleAddr = reinterpret_cast<BYTE*>(GetModuleHandle("physics_RT.dll"));
	CKERROR(CKBaseManager:: * ClearFunc)();
	*reinterpret_cast<DWORD*>(&ClearFunc) = *(*reinterpret_cast<DWORD**>(physicsManager) + 4);
	(physicsManager->*ClearFunc)();
	CKERROR(CKBaseManager:: * EndFunc)();
	*reinterpret_cast<DWORD*>(&EndFunc) = *(*reinterpret_cast<DWORD**>(physicsManager) + 12);
	(physicsManager->*EndFunc)();
	CKERROR(CKBaseManager:: * ConstructorFunc)(CKContext*);
	*reinterpret_cast<BYTE**>(&ConstructorFunc) = reinterpret_cast<BYTE*>(GetModuleHandle("physics_RT.dll")) + 0x6730;
	(physicsManager->*ConstructorFunc)(m_bml->GetCKContext());
	//CKERROR(CKBaseManager:: * PauseFunc)();
	//*reinterpret_cast<DWORD*>(&PauseFunc) = *(*reinterpret_cast<DWORD**>(physicsManager) + 15);
	//(physicsManager->*PauseFunc)();
	//CKERROR(CKBaseManager:: * ResetFunc)();
	//*reinterpret_cast<DWORD*>(&ResetFunc) = *(*reinterpret_cast<DWORD**>(physicsManager) + 13);
	//(physicsManager->*ResetFunc)();
	CKERROR(CKBaseManager:: * InitFunc)();
	*reinterpret_cast<DWORD*>(&InitFunc) = *(*reinterpret_cast<DWORD**>(physicsManager) + 11);
	(physicsManager->*InitFunc)();
	CKERROR(CKBaseManager:: * PlayFunc)();
	*reinterpret_cast<BYTE**>(&PlayFunc) = reinterpret_cast<BYTE*>(GetModuleHandle("physics_RT.dll")) + 0x6BB0;
	(physicsManager->*PlayFunc)();

	m_bml->RestoreIC(m_bml->Get3dEntityByName("Ball_PaperPieces_Frame"), true);
	m_bml->RestoreIC(m_bml->Get3dEntityByName("Ball_StonePieces_Frame"), true);
	m_bml->RestoreIC(m_bml->Get3dEntityByName("Ball_WoodPieces_Frame"), true);

	m_initPieces->ActivateInput(0);
	m_initPieces->Activate();
	CKBehavior* Balls_Init = m_bml->GetScriptByName("Balls_Init");
	CKBehavior* temp = m_initPieces->GetParent();
	m_initPieces->GetParent()->Activate();
}
#endif

void TASSupport::OnStart() {
	if (m_enabled->GetBoolean()) {
		m_bml->AddTimer(1u, [this]() {
			// 0x43ec
			// OnCKReset
			// OnCKPlay
			// OnCKPause empty
			// OnCKEnd
			// OnCKInit
			// PostClearAll
			// PostProcess
			// PreProcess
			// vtable 100632D0
			CKBaseManager* physicsManager = m_bml->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
			float* averageTickTime = reinterpret_cast<float*>(reinterpret_cast<BYTE*>(physicsManager) + 0xC8);
			*averageTickTime = m_bml->GetTimeManager()->GetLastDeltaTime();
			BYTE* timer = *reinterpret_cast<BYTE**>(reinterpret_cast<BYTE*>(physicsManager) + 0xC0);
			*reinterpret_cast<double*>(timer + 0x120) = 0;
			*reinterpret_cast<double*>(timer + 0x128) = 1.0 / 66;
			*reinterpret_cast<double*>(timer + 0x130) = 0;
			*reinterpret_cast<DWORD*>(timer + 0x138) = 0;
			*reinterpret_cast<double*>(*reinterpret_cast<BYTE**>(timer + 0x4) + 0x18) = 0;
			});

		if (m_keyboard) {
			m_keyboard->GetElementValue(0, 0, &m_key_up);
			m_keyboard->GetElementValue(0, 1, &m_key_down);
			m_keyboard->GetElementValue(0, 2, &m_key_left);
			m_keyboard->GetElementValue(0, 3, &m_key_right);
			m_keyboard->GetElementValue(0, 4, &m_key_shift);
			m_keyboard->GetElementValue(0, 5, &m_key_space);
		}

		if (m_readyToPlay) {
			m_readyToPlay = false;
			m_playing = true;
			m_curFrame = 0;
			m_bml->SendIngameMessage("Start playing TAS.");
			if (m_showKeys->GetBoolean())
				m_keysGui->SetVisible(true);
		}
		else if (m_record->GetBoolean()) {
			m_recording = true;
			m_curFrame = 0;
			m_recordData.clear();
			m_bml->SendIngameMessage("Start recording TAS.");
		}
	}
}

void TASSupport::OnStop() {
	if (m_enabled->GetBoolean()) {
		if (m_playing || m_recording) {
			if (m_playing) {
				m_bml->SendIngameMessage("TAS playing stopped.");
				if (m_exitOnFinish->GetBoolean())
					m_bml->ExitGame();
			}
			else m_bml->SendIngameMessage("TAS recording stopped.");
			if (m_showKeys->GetBoolean())
				m_keysGui->SetVisible(false);
			m_playing = m_recording = false;
			m_recordData.clear();
			m_recordData.shrink_to_fit();
			m_curFrame = 0;
		}
	}
}

void TASSupport::OnFinish() {
	if (m_enabled->GetBoolean()) {
		if (m_recording) {
			m_bml->AddTimer(4u, [this]() {
				static char filename[MAX_PATH];
				time_t stamp = time(0);
				tm* curtime = localtime(&stamp);
				sprintf(filename, "%04d%02d%02d_%02d%02d%02d_%s.tas", curtime->tm_year + 1900, curtime->tm_mon + 1,
					curtime->tm_mday, curtime->tm_hour, curtime->tm_min, curtime->tm_sec, m_mapName.c_str());
				m_bml->SendIngameMessage(("TAS record saved to " + std::string(filename)).c_str());
				int length = m_recordData.size();

				std::thread([this, length]() {
					char filepath[MAX_PATH];
					sprintf(filepath, "..\\ModLoader\\TASRecords\\%s", filename);
					CompressDataToFile((char*)&m_recordData[0], length * sizeof(FrameData), filepath);
					OnStop();
					}).detach();
				});
		}
	}
}

void TASSupport::LoadTAS(const std::string &filename) {
	std::thread([this, filename]() {
		auto data = UncompressDataFromFile(filename.c_str());
		int length = data.second / sizeof(FrameData);
		m_recordData.resize(length);
		memcpy(&m_recordData[0], data.first, data.second);
		m_readyToPlay = true;
		CKDeletePointer(data.first);
		}).detach();
}

GuiTASList::GuiTASList() {
	m_left = AddLeftButton("M_List_Left", 0.4f, 0.34f, [this]() { PreviousPage(); });
	m_right = AddRightButton("M_List_Right", 0.4f, 0.6238f, [this]() { NextPage(); });
	AddBackButton("M_Opt_Mods_Back")->SetCallback([this]() { Exit(); });
	AddTextLabel("M_Opt_Mods_Title", "TAS Records", ExecuteBB::GAMEFONT_02, 0.35f, 0.07f, 0.3f, 0.05f);

	Init(0, 12);
	SetVisible(false);
}

void GuiTASList::Init(int size, int maxsize) {
	m_size = size;
	m_maxpage = (size + maxsize - 1) / maxsize;
	m_maxsize = maxsize;
	m_curpage = 0;

	for (int i = 0; i < m_maxsize; i++) {
		BGui::Button* button = CreateButton(i);
		m_buttons.push_back(button);
		BGui::Text* text = AddText(("M_TAS_Text_" + std::to_string(i)).c_str(), "", 0.44f, 0.15f + 0.057f * i, 0.3f, 0.05f);
		text->SetZOrder(100);
		m_texts.push_back(text);
	}
}

void GuiTASList::Resize(int size) {
	m_size = size;
	m_maxpage = (size + m_maxsize - 1) / m_maxsize;
}

void GuiTASList::RefreshRecords() {
	using convert_type = std::codecvt_byname<wchar_t, char, std::mbstate_t>;
	std::wstring_convert<convert_type, wchar_t> converter(new convert_type("zh-CN"));

	m_records.clear();
	for (auto& f : std::filesystem::recursive_directory_iterator("..\\ModLoader\\TASRecords")) {
		if (f.status().type() == std::filesystem::file_type::regular) {
			std::string ext = f.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), tolower);
			if (ext == ".tas") {
				TASInfo info;
				try {
					info.displayname = f.path().stem().string();
					info.filepath = "..\\ModLoader\\TASRecords\\" + info.displayname + ".tas";
				}
				catch (std::system_error e) {}

				m_records.push_back(info);
			}
		}
	}

	std::sort(m_records.begin(), m_records.end());
}

void GuiTASList::SetPage(int page) {
	int size = (std::min)(m_maxsize, m_size - page * m_maxsize);
	for (int i = 0; i < m_maxsize; i++) {
		m_buttons[i]->SetVisible(i < size);
		m_texts[i]->SetVisible(i < size);
	}
	for (int i = 0; i < size; i++)
		m_texts[i]->SetText(GetButtonText(page * m_maxsize + i).c_str());

	m_curpage = page;
	m_left->SetVisible(page > 0);
	m_right->SetVisible(page < m_maxpage - 1);
}

void GuiTASList::SetVisible(bool visible) {
	Gui::SetVisible(visible);
	m_visible = visible;
	if (visible) {
		RefreshRecords();
		Resize(m_records.size());
		SetPage((std::max)((std::min)(m_curpage, m_maxpage - 1), 0));
	}
}

BGui::Button* GuiTASList::CreateButton(int index) {
	return AddLevelButton(("M_TAS_But_" + std::to_string(index)).c_str(), "", 0.15f + 0.057f * index, 0.4031f, [this, index]() {
		GuiTASList::Exit();
		TASInfo& info = m_records[m_curpage * m_maxsize + index];
		g_mod->GetBML()->SendIngameMessage(("Loading TAS Record: " + info.displayname).c_str());
		g_mod->LoadTAS(info.filepath);
		});
}

const std::string GuiTASList::GetButtonText(int index) {
	return m_records[index].displayname;
}

void GuiTASList::Exit() {
	g_mod->GetBML()->GetCKContext()->GetCurrentScene()->Activate(
		g_mod->GetBML()->GetScriptByName("Menu_Main"), true);
	SetVisible(false);
}