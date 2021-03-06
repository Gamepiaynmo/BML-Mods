#pragma once

#include <BML/BMLAll.h>
#include <thread>

#define TICK_SPEED 8

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

class SpiritTrail : public IMod {
public:
	SpiritTrail(IBML* bml) : IMod(bml) {}

	virtual CKSTRING GetID() override { return "SpiritTrail"; }
	virtual CKSTRING GetVersion() override { return BML_VERSION; }
	virtual CKSTRING GetName() override { return "Spirit Trail"; }
	virtual CKSTRING GetAuthor() override { return "Gamepiaynmo"; }
	virtual CKSTRING GetDescription() override { return "Play the historical best record as a translucent ball."; }
	DECLARE_BML_VERSION;

	virtual void OnLoad() override;
	virtual void OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
		CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
		BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) override;
	virtual void OnProcess() override;

	virtual void OnStartLevel() override { m_curSector = 1; PreparePlaying(); PrepareRecording(); }
	virtual void OnBallNavActive() override { StartPlaying(); StartRecording(); }
	virtual void OnPauseLevel() override { PausePlaying(); PauseRecording(); m_sractive = false; }
	virtual void OnUnpauseLevel() override { UnpausePlaying(); UnpauseRecording(); m_sractive = true; }
	virtual void OnCounterActive() override { m_sractive = true; }
	virtual void OnCounterInactive() override { m_sractive = false; }

	virtual void OnPostResetLevel() override { StopPlaying(); StopRecording(); }
	virtual void OnPostExitLevel() override { StopPlaying(); StopRecording(); }
	virtual void OnBallOff() override {
		int lifes;
		m_energy->GetElementValue(0, 1, &lifes);
		if (m_deathreset->GetBoolean() || lifes <= 0) {
			m_bml->AddTimer(1000.0f, [this, lifes]() {
				StopPlaying(); StopRecording();
				if (lifes > 0) {
					PreparePlaying(); PrepareRecording();
				}
				});
		}
	}

	virtual void OnLevelFinish() override { EndRecording(); }
	virtual void OnPostNextLevel() override { StopPlaying(); }
	virtual void OnPreCheckpointReached() override { StopPlaying(); EndRecording();
		m_curSector = GetCurrentSector() + 1; PreparePlaying(); PrepareRecording(); }
	virtual void OnPostCheckpointReached() override { StartPlaying(); StartRecording(); }

	int GetHSScore();
	float GetSRScore() { return m_srtimer; }
	int GetCurrentBall();
	int GetCurrentSector();
	void SetCurrentBall(int curBall);

	void PreparePlaying();
	void StartPlaying();
	void PausePlaying();
	void UnpausePlaying();
	void StopPlaying();

	void PrepareRecording();
	void StartRecording();
	void PauseRecording();
	void UnpauseRecording();
	void StopRecording();
	void EndRecording();

private:
	std::string m_curMap;
	std::string m_recordDir;
	int m_curSector;

	bool m_waitRecording = false;
	bool m_isRecording = false;
	int m_startHS;
	float m_recordTimer;
	size_t m_curBall;
	bool m_recordPaused;

	bool m_waitPlaying = false;
	bool m_isPlaying = false;
	float m_playTimer;
	size_t m_playBall;
	bool m_playhssr = false;
	size_t m_playFrame, m_playTrafo;
	bool m_playPaused;

	struct Record {
		int hsscore;
		float srscore;

		struct State {
			VxVector pos;
			VxQuaternion rot;
		};

		std::vector<State> states;
		std::vector<std::pair<int, int>> trafo;
	};
	Record m_record, m_play[2];
	std::thread m_loadPlay;
	bool m_loadingPlay = false;

	IProperty* m_enabled, * m_hssr, * m_deathreset;

	struct SpiritBall {
		std::string name;
		CK3dObject* obj;
		std::vector<CKMaterial*> materials;
	};
	std::vector<SpiritBall> m_dualBalls;

	CKDataArray* m_energy, * m_curLevel, * m_ingameParam;
	float m_srtimer;
	bool m_sractive = false;
};