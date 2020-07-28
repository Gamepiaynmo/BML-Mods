#include "SpiritTrail.h"
#include <filesystem>
#include <thread>

IMod* BMLEntry(IBML* bml) {
	return new SpiritTrail(bml);
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

std::string GetFileHash(const char* filename) {
	auto data = ReadDataFromFile(filename);

	std::string res;
	int segm[] = { 0, data.second / 4, data.second / 2, data.second * 3 / 4, data.second };
	for (int i = 0; i < 4; i++) {
		char hash[10];
		CKDWORD crc = CKComputeDataCRC(data.first + segm[i], segm[i + 1] - segm[i]);
		sprintf(hash, "%08x", crc);
		res += hash;
	}

	delete[] data.first;
	return res;
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

void SpiritTrail::OnLoad() {
	std::filesystem::create_directories("..\\ModLoader\\Trails\\");

	GetConfig()->SetCategoryComment("Misc", "Miscellaneous");
	m_enabled = GetConfig()->GetProperty("Misc", "Enable");
	m_enabled->SetComment("Enable Spirit Trail");
	m_enabled->SetDefaultBoolean(true);

	m_hssr = GetConfig()->GetProperty("Misc", "HS_SR");
	m_hssr->SetComment("Play HS(No) or SR(Yes) record");
	m_hssr->SetDefaultBoolean(false);

	m_deathreset = GetConfig()->GetProperty("Misc", "DeathReset");
	m_deathreset->SetComment("Reset record on Death");
	m_deathreset->SetDefaultBoolean(true);
}

void SpiritTrail::OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName, CK_CLASSID filterClass,
	BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials, BOOL dynamic,
	XObjectArray* objArray, CKObject* masterObj) {
	std::string mapDirs[] = { "3D Entities\\Level\\", "..\\ModLoader\\Maps\\" };
	if (isMap) {
		m_curMap = filename;
		XString filepath = filename;
		m_bml->GetPathManager()->ResolveFileName(filepath, DATA_PATH_IDX);
		m_recordDir = "..\\ModLoader\\Trails\\" + GetFileHash(filepath.CStr()) + "\\";
		std::filesystem::create_directories(m_recordDir.c_str());
	}

	if (!strcmp(filename, "3D Entities\\Balls.nmo")) {
		CKDataArray* physBall = m_bml->GetArrayByName("Physicalize_GameBall");
		for (int i = 0; i < physBall->GetRowCount(); i++) {
			SpiritBall ball;
			ball.name.insert(0, physBall->GetElementStringValue(i, 0, nullptr), '\0');
			physBall->GetElementStringValue(i, 0, &ball.name[0]);
			ball.name.pop_back();
			ball.obj = m_bml->Get3dObjectByName(ball.name.c_str());

			CKDependencies dep;
			dep.Resize(40); dep.Fill(0);
			dep.m_Flags = CK_DEPENDENCIES_CUSTOM;
			dep[CKCID_OBJECT] = CK_DEPENDENCIES_COPY_OBJECT_NAME | CK_DEPENDENCIES_COPY_OBJECT_UNIQUENAME;
			dep[CKCID_MESH] = CK_DEPENDENCIES_COPY_MESH_MATERIAL;
			dep[CKCID_3DENTITY] = CK_DEPENDENCIES_COPY_3DENTITY_MESH;
			ball.obj = static_cast<CK3dObject*>(m_bml->GetCKContext()->CopyObject(ball.obj, &dep, "_Spirit"));
			for (int j = 0; j < ball.obj->GetMeshCount(); j++) {
				CKMesh* mesh = ball.obj->GetMesh(j);
				for (int k = 0; k < mesh->GetMaterialCount(); k++) {
					CKMaterial* mat = mesh->GetMaterial(k);
					mat->EnableAlphaBlend();
					mat->SetSourceBlend(VXBLEND_SRCALPHA);
					mat->SetDestBlend(VXBLEND_INVSRCALPHA);
					VxColor color = mat->GetDiffuse();
					color.a = 0.5f;
					mat->SetDiffuse(color);
					ball.materials.push_back(mat);
					m_bml->SetIC(mat);
				}
			}
			m_balls.push_back(ball);
		}

		GetLogger()->Info("Created Spirit Balls");
	}

	if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
		m_energy = m_bml->GetArrayByName("Energy");
		m_curLevel = m_bml->GetArrayByName("CurrentLevel");
		m_ingameParam = m_bml->GetArrayByName("IngameParameter");
	}
}

void SpiritTrail::OnProcess() {
	if (m_sractive)
		m_srtimer += m_bml->GetTimeManager()->GetLastDeltaTime();

	const float delta = 1000.0f / TICK_SPEED;
	if (m_isRecording && !m_recordPaused) {
		m_recordTimer += m_bml->GetTimeManager()->GetLastDeltaTime();
		m_recordTimer = (std::min)(m_recordTimer, 1000.0f);

		while (m_recordTimer > 0) {
			m_recordTimer -= delta;

			int curBall = GetCurrentBall();
			if (curBall != m_curBall)
				m_record.trafo.push_back({ m_record.states.size(), curBall });
			m_curBall = curBall;

			CK3dObject* ball = static_cast<CK3dObject*>(m_curLevel->GetElementObject(0, 1));
			if (ball) {
				Record::State state;
				ball->GetPosition(&state.pos);
				ball->GetQuaternion(&state.rot);
				m_record.states.push_back(state);
			}
		}

		if (m_bml->IsCheatEnabled()) {
			StopRecording();
		}

		if (GetSRScore() > 1000 * 3600) {
			GetLogger()->Info("Record is longer than 1 hour, stop recording");
			StopRecording();
		}
	}

	if (m_isPlaying && !m_playPaused) {
		m_playTimer += m_bml->GetTimeManager()->GetLastDeltaTime();
		m_playTimer = (std::min)(m_playTimer, 1000.0f);

		while (m_playTimer > 0) {
			m_playTimer -= delta;
			m_playFrame++;

			auto& trafos = m_play[m_playhssr].trafo;
			if (m_playTrafo < trafos.size()) {
				auto& trafo = trafos[m_playTrafo];
				if (m_playFrame == trafo.first) {
					SetCurrentBall(trafo.second);
					m_playTrafo++;
				}
			}
		}

		auto& states = m_play[m_playhssr].states;
		if (m_playFrame >= 0 && m_playFrame < states.size() - 1) {
			if (m_playBall >= 0 && m_playBall < m_balls.size()) {
				CKObject* playerBall = m_curLevel->GetElementObject(0, 1);
				CK3dObject* ball = m_balls[m_playBall].obj;
				ball->Show(playerBall->IsVisible() ? CKSHOW : CKHIDE);

				float portion = (m_playTimer / delta + 1);
				Record::State& cur = states[m_playFrame], & next = states[m_playFrame + 1];
				VxVector position = (next.pos - cur.pos) * portion + cur.pos;
				VxQuaternion rotation = Slerp(portion, cur.rot, next.rot);
				ball->SetPosition(&position);
				ball->SetQuaternion(&rotation);
			}
		}
		else StopPlaying();
	}
}

int SpiritTrail::GetHSScore() {
	int points, lifes;
	m_energy->GetElementValue(0, 0, &points);
	m_energy->GetElementValue(0, 1, &lifes);
	return points + lifes * 200;
}

int SpiritTrail::GetCurrentBall() {
	CKObject* ball = m_curLevel->GetElementObject(0, 1);
	if (ball) {
		std::string ballName = ball->GetName();
		for (size_t i = 0; i < m_balls.size(); i++) {
			if (m_balls[i].name == ballName)
				return i;
		}
	}

	return 0;
}

int SpiritTrail::GetCurrentSector() {
	int res;
	m_ingameParam->GetElementValue(0, 1, &res);
	return res;
}

void SpiritTrail::SetCurrentBall(int curBall) {
	m_playBall = curBall;
	for (auto& ball : m_balls)
		ball.obj->Show(CKHIDE);
	if (m_playBall >= 0 && m_playBall < m_balls.size())
		m_balls[m_playBall].obj->Show();
}

void SpiritTrail::PreparePlaying() {
	if (!m_isPlaying && m_enabled->GetBoolean() && !m_waitPlaying) {
		m_waitPlaying = true;

		m_loadPlay = std::thread([this]() {
			std::string recfile[2] = { (m_recordDir + "hs" + std::to_string(m_curSector) + ".rec"),
				(m_recordDir + "sr" + std::to_string(m_curSector) + ".rec") };
			m_playhssr = m_hssr->GetBoolean();

			for (int i = 0; i < 2; i++) {
				if (std::filesystem::exists(recfile[i])) {
					std::pair<char*, int> data = UncompressDataFromFile(recfile[i].c_str());
					m_play[i].hsscore = *reinterpret_cast<int*>(data.first);
					m_play[i].srscore = *reinterpret_cast<float*>(data.first + 4);
					if (bool(i) == m_playhssr) {
						int ssize = *reinterpret_cast<int*>(data.first + 8);
						int tsize = *reinterpret_cast<int*>(data.first + 12);
						m_play[i].states.resize(ssize);
						memcpy(&m_play[i].states[0], data.first + 16, ssize * sizeof(Record::State));
						m_play[i].trafo.resize(tsize);
						memcpy(&m_play[i].trafo[0], data.first + 16 + ssize * sizeof(Record::State), tsize * sizeof(std::pair<int, int>));
					}

					CKDeletePointer(data.first);
				}
				else {
					m_play[i].hsscore = INT_MIN;
					m_play[i].srscore = FLT_MAX;
				}
			}
			});
	}
}

void SpiritTrail::StartPlaying() {
	if (!m_isPlaying && m_enabled->GetBoolean() && m_waitPlaying) {
		m_waitPlaying = false;

		if (m_loadPlay.joinable())
			m_loadPlay.join();

		if (m_play[m_playhssr].hsscore > INT_MIN) {
			m_isPlaying = true;
			m_playPaused = false;
			m_playTimer = -1000.0f / TICK_SPEED;
			m_playFrame = 0;
			m_playTrafo = 1;
			SetCurrentBall(m_play[m_playhssr].trafo[0].second);
		}
	}
}

void SpiritTrail::PausePlaying() {
	m_playPaused = true;
}

void SpiritTrail::UnpausePlaying() {
	m_playPaused = false;
}

void SpiritTrail::StopPlaying() {
	if (m_isPlaying) {
		m_isPlaying = false;
		for (int i = 0; i < 2; i++) {
			m_play[i].states.clear();
			m_play[i].states.shrink_to_fit();
			m_play[i].trafo.clear();
			m_play[i].trafo.shrink_to_fit();
		}

		for (auto& ball : m_balls)
			ball.obj->Show(CKHIDE);
	}
}

void SpiritTrail::PrepareRecording() {
	if (!m_isRecording && !m_bml->IsCheatEnabled() && m_enabled->GetBoolean() && !m_waitRecording) {
		m_waitRecording = true;
	}
}

void SpiritTrail::StartRecording() {
	if (!m_isRecording && !m_bml->IsCheatEnabled() && m_enabled->GetBoolean() && m_waitRecording) {
		m_waitRecording = false;
		m_isRecording = true;
		m_recordPaused = false;

		m_startHS = GetHSScore();
		m_recordTimer = 0;
		m_srtimer = 0;
		m_curBall = GetCurrentBall();
		m_record.trafo.push_back({ 0, m_curBall });
	}
}

void SpiritTrail::PauseRecording() {
	m_recordPaused = true;
}

void SpiritTrail::UnpauseRecording() {
	m_recordPaused = false;
}

void SpiritTrail::StopRecording() {
	if (m_isRecording) {
		m_isRecording = false;
		m_record.trafo.clear();
		m_record.trafo.shrink_to_fit();
		m_record.states.clear();
		m_record.states.shrink_to_fit();
	}
}

void SpiritTrail::EndRecording() {
	if (m_isRecording) {
		m_record.hsscore = GetHSScore() - m_startHS;
		m_record.srscore = GetSRScore();

		bool savehs = m_record.hsscore > m_play[0].hsscore,
			savesr = m_record.srscore < m_play[1].srscore;
		if (savehs || savesr) {
			int ssize = sizeof(Record::State) * m_record.states.size();
			int tsize = sizeof(std::pair<int, int>) * m_record.trafo.size();
			int size = 16 + ssize + tsize;

			char* buffer = new char[size];
			*reinterpret_cast<int*>(buffer) = m_record.hsscore;
			*reinterpret_cast<float*>(buffer + 4) = m_record.srscore;
			*reinterpret_cast<int*>(buffer + 8) = m_record.states.size();
			*reinterpret_cast<int*>(buffer + 12) = m_record.trafo.size();
			memcpy(buffer + 16, &m_record.states[0], ssize);
			memcpy(buffer + 16 + ssize, &m_record.trafo[0], tsize);

			std::string hspath = m_recordDir + "hs" + std::to_string(m_curSector) + ".rec",
				srpath = m_recordDir + "sr" + std::to_string(m_curSector) + ".rec";

			std::thread([savehs, savesr, hspath, srpath, buffer, size]() {
				if (savehs) {
					CompressDataToFile(buffer, size, hspath.c_str());
					if (savesr) {
						std::filesystem::copy_file(hspath, srpath, std::filesystem::copy_options::overwrite_existing);
					}
				}
				else {
					CompressDataToFile(buffer, size, srpath.c_str());
				}
				delete[] buffer;
				}).detach();

			if (savehs)
				GetLogger()->Info("HS of sector %d has updated", m_curSector);
			if (savesr)
				GetLogger()->Info("SR of sector %d has updated", m_curSector);
		}

		StopRecording();
	}
}