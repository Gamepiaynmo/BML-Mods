#include "TASSupport.h"
#include "minhook/MinHook.h"
#include <ctime>

TASSupport* g_mod;

IMod* BMLEntry(IBML* bml) {
	return g_mod = new TASSupport(bml);
}

class HookTimeManager : public CKTimeManager {
public:
	typedef CKERROR(HookTimeManager::* PreProcessFunc)();

	static CKDWORD tickCount;
	static PreProcessFunc m_preProcess;

	CKERROR HookPreProcess() {
		CKERROR ret = (this->*m_preProcess)();

		tickCount++;
		SetLastDeltaTime(1000.0f / 144);
		SetLastDeltaTimeFree(1000.0f / 144);
		SetAbsoluteTime(tickCount * 1000.0f / 144);
		SetMainTickCount(tickCount);

		return ret;
	}
};

CKDWORD HookTimeManager::tickCount = 0;
HookTimeManager::PreProcessFunc HookTimeManager::m_preProcess = &HookTimeManager::HookPreProcess;

BOOL WINAPI HookQueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
	lpPerformanceCount->QuadPart = 0;
	return true;
}

auto hookCount = &HookQueryPerformanceCounter;

clock_t hook_clock(void) {
	return 0;
}

auto hookClock = &hook_clock;

time_t hook_time(time_t* const _Time) {
	if (_Time) *_Time = 0;
	return 0;
}

auto hookTime = &hook_time;

auto hookPhysicalize = &TASSupport::HookPhysicalize;

int TASSupport::HookPhysicalize(const CKBehaviorContext& behcontext) {
	if (behcontext.Behavior->IsInputActive(0)) {
		CKBeObject* target = behcontext.Behavior->GetTarget();
		CKSTRING name = target->GetName();
		VxVector pos;
		reinterpret_cast<CK3dEntity*>(target)->GetPosition(&pos);
		g_mod->Logger()->Info("%s %f %f %f", name, pos.x, pos.y, pos.z);
	}

	return hookPhysicalize(behcontext);
}

struct KeyState {
	BYTE up, down, left, right, q, shift, space, esc;
};
int curFrame = 0;
FILE* recFile;
class HookInputManager : public CKInputManager {
public:
	typedef CKERROR(HookInputManager::* PreProcessFunc)();

	static PreProcessFunc m_preProcess;
	static bool m_record, m_play;

	CKERROR HookPreProcess() {
		CKERROR ret = (this->*m_preProcess)();
		if (m_play) {
			if (ftell(recFile) / 4 == 8831)
				recFile = recFile;
			KeyState state;
			fread(&state, sizeof(state), 1, recFile);
			BYTE* stateBuf = GetKeyboardState();
			stateBuf[CKKEY_UP] = state.up;
			stateBuf[CKKEY_DOWN] = state.down;
			stateBuf[CKKEY_LEFT] = state.left;
			stateBuf[CKKEY_RIGHT] = state.right;
			stateBuf[CKKEY_Q] = state.q;
			stateBuf[CKKEY_LSHIFT] = state.shift;
			stateBuf[CKKEY_SPACE] = state.space;
			stateBuf[CKKEY_ESCAPE] = state.esc;
		}

		if (m_record) {
			KeyState state;
			BYTE* stateBuf = GetKeyboardState();
			state.up = stateBuf[CKKEY_UP];
			state.down = stateBuf[CKKEY_DOWN];
			state.left = stateBuf[CKKEY_LEFT];
			state.right = stateBuf[CKKEY_RIGHT];
			state.q = stateBuf[CKKEY_Q];
			state.shift = stateBuf[CKKEY_LSHIFT];
			state.space = stateBuf[CKKEY_SPACE];
			state.esc = stateBuf[CKKEY_ESCAPE];
			fwrite(&state, sizeof(state), 1, recFile);
		}

		return ret;
	}
};

HookInputManager::PreProcessFunc HookInputManager::m_preProcess = &HookInputManager::HookPreProcess;
bool HookInputManager::m_record = false;
bool HookInputManager::m_play = false;

void WriteMemory(LPVOID dest, LPVOID src, int len) {
	DWORD oldFlag;
	VirtualProtect(dest, len, PAGE_EXECUTE_READWRITE, &oldFlag);
	memcpy(dest, src, len);
	VirtualProtect(dest, len, oldFlag, &oldFlag);
}

void TASSupport::OnLoad() {
	void* countHook = (reinterpret_cast<BYTE*>(LoadLibrary("Physics_RT.dll")) + 0x630E8);
	WriteMemory(countHook, &hookCount, sizeof(&hookCount));

	void* clockHook = (reinterpret_cast<BYTE*>(LoadLibrary("Physics_RT.dll")) + 0x63154);
	WriteMemory(clockHook, &hookClock, sizeof(&hookClock));

	void* timeHook = (reinterpret_cast<BYTE*>(LoadLibrary("Physics_RT.dll")) + 0x63190);
	WriteMemory(timeHook, &hookTime, sizeof(&hookTime));

	BYTE randcode[] = { 0x33, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90 };
	void* randHook = (reinterpret_cast<BYTE*>(LoadLibrary("Physics_RT.dll")) + 0x52f95);
	WriteMemory(randHook, randcode, sizeof(randcode));

	MH_Initialize();

	if (MH_CreateHook(reinterpret_cast<LPVOID>(0x24017F8C),
		*reinterpret_cast<LPVOID*>(&HookTimeManager::m_preProcess),
		reinterpret_cast<LPVOID*>(&HookTimeManager::m_preProcess)) != MH_OK
		|| MH_EnableHook(reinterpret_cast<LPVOID>(0x24017F8C)) != MH_OK) {
		GetLogger()->Error("Create Time Manager Hook Failed");
		return;
	}

	/*void* physicalizeAddr = (reinterpret_cast<BYTE*>(LoadLibrary("Physics_RT.dll")) + 0x26B0);
	if (MH_CreateHook(reinterpret_cast<LPVOID>(physicalizeAddr), &HookPhysicalize,
		reinterpret_cast<LPVOID*>(&hookPhysicalize)) != MH_OK
		|| MH_EnableHook(reinterpret_cast<LPVOID>(physicalizeAddr)) != MH_OK) {
		m_logger->Error("Create Physicalize Hook Failed");
		return;
	}*/

	if (MH_CreateHook(reinterpret_cast<LPVOID>(0x24AC1C30),
		*reinterpret_cast<LPVOID*>(&HookInputManager::m_preProcess),
		reinterpret_cast<LPVOID*>(&HookInputManager::m_preProcess)) != MH_OK
		|| MH_EnableHook(reinterpret_cast<LPVOID>(0x24AC1C30)) != MH_OK) {
		return;
	}
}

void TASSupport::OnUnload() {
	MH_Uninitialize();
}

void TASSupport::OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
	CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
	BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) {
	if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
		m_curLevel = m_bml->GetArrayByName("CurrentLevel");
	}
}

void TASSupport::OnLoadScript(CKSTRING filename, CKBehavior* script) {
	if (!strcmp(script->GetName(), "Ball_Explosion_Wood")
		|| !strcmp(script->GetName(), "Ball_Explosion_Paper")
		|| !strcmp(script->GetName(), "Ball_Explosion_Stone")) {
		CKBehavior* beh = ScriptHelper::FindFirstBB(script, "Set Position");
		ScriptHelper::DeleteBB(script, beh);
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
	}
}

void TASSupport::OnPreLoadLevel() {
	CKBaseManager* physicsManager = m_bml->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
	*reinterpret_cast<double*>(reinterpret_cast<BYTE*>(physicsManager) + 0xC8) = 1000.0f / 144;
	BYTE* timer = *reinterpret_cast<BYTE**>(reinterpret_cast<BYTE*>(physicsManager) + 0xC0);
	*reinterpret_cast<double*>(timer + 0x120) = 0;
	*reinterpret_cast<double*>(timer + 0x128) = 1.0 / 66;
	*reinterpret_cast<double*>(timer + 0x130) = 0;
	*reinterpret_cast<DWORD*>(timer + 0x138) = 0;
	*reinterpret_cast<double*>(*reinterpret_cast<BYTE**>(timer + 0x4) + 0x18) = 0;
}

void TASSupport::OnStartLevel() {
#if 0
	HookInputManager::m_record = true;
	recFile = fopen("record.bin", "wb");
#else
	HookInputManager::m_play = true;
	recFile = fopen("record.bin", "rb");
#endif
}