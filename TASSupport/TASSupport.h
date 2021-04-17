#pragma once

#include <BML/BMLAll.h>

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

struct KeyState {
	unsigned key_up : 1;
	unsigned key_down : 1;
	unsigned key_left : 1;
	unsigned key_right : 1;
	unsigned key_shift : 1;
	unsigned key_space : 1;
	unsigned key_q : 1;
	unsigned key_esc : 1;
	unsigned key_enter : 1;
};

struct FrameData {
	FrameData() {}
	FrameData(float deltaTime) : deltaTime(deltaTime) {
		keyStates = 0;
	}

	float deltaTime;
	union {
		KeyState keyState;
		int keyStates;
	};
};

class GuiTASList : public BGui::Gui {
public:
	GuiTASList();
	void Init(int size, int maxsize);
	void Resize(int size);
	void RefreshRecords();

	virtual BGui::Button* CreateButton(int index);
	virtual const std::string GetButtonText(int index);
	virtual void SetPage(int page);
	virtual void Exit();

	virtual void SetVisible(bool visible) override;
	bool IsVisible() { return m_visible; }

	void PreviousPage() { if (m_curpage > 0) SetPage(m_curpage - 1); }
	void NextPage() { if (m_curpage < m_maxpage - 1) SetPage(m_curpage + 1); }

protected:
	std::vector<BGui::Button*> m_buttons;
	int m_curpage, m_maxpage, m_size, m_maxsize;
	BGui::Button* m_left, * m_right;

	struct TASInfo {
		std::string displayname, filepath;
		bool operator <(const TASInfo& o) {
			return displayname < o.displayname;
		}
	};

	std::vector<TASInfo> m_records;
	std::vector<BGui::Text*> m_texts;
	bool m_visible;
};

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

	virtual void OnPreLoadLevel() override { OnStart(); }
	virtual void OnPreResetLevel() override { OnStop(); }
	virtual void OnPreExitLevel() override { OnStop(); }
	virtual void OnLevelFinish() override { OnFinish(); }
	//virtual void OnPostExitLevel() override;

#ifdef _DEBUG
	virtual void OnPhysicalize(CK3dEntity* target, CKBOOL fixed, float friction, float elasticity, float mass,
		CKSTRING collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter, float linearDamp,
		float rotDamp, CKSTRING collSurface, VxVector massCenter, int convexCnt, CKMesh** convexMesh,
		int ballCnt, VxVector* ballCenter, float* ballRadius, int concaveCnt, CKMesh** concaveMesh) override;
	virtual void OnUnphysicalize(CK3dEntity* target) override;
#endif

	ILogger* Logger() { return GetLogger(); }
	IBML* GetBML() { return m_bml; }

	void OnStart();
	void OnStop();
	void OnFinish();
	void LoadTAS(const std::string& filename);

	CKDataArray* m_curLevel, * m_keyboard = nullptr;
	CKKEYBOARD m_key_up = CKKEY_UP,
		m_key_down = CKKEY_DOWN,
		m_key_left = CKKEY_LEFT,
		m_key_right = CKKEY_RIGHT,
		m_key_shift = CKKEY_LSHIFT,
		m_key_space = CKKEY_SPACE;

	IProperty* m_enabled, * m_record, * m_stopKey;
	bool m_readyToPlay = false;

	bool m_recording = false, m_playing = false;
	int m_curFrame;
	std::vector<FrameData> m_recordData;
	std::string m_mapName;

	CK2dEntity* m_level01;
	CKBehavior* m_exitStart;
	BGui::Gui* m_tasEntryGui = nullptr;
	BGui::Button* m_tasEntry;
	GuiTASList* m_tasListGui = nullptr;

	BGui::Gui* m_keysGui = nullptr;
	BGui::Button* m_butUp, * m_butDown, * m_butLeft, * m_butRight,
		* m_butShift, * m_butSpace, * m_butQ, * m_butEsc;
};