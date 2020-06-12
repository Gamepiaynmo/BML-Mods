#include "SpiritTrail.h"
#include <ImageHlp.h>
#include <filesystem>

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
	MakeSureDirectoryPathExists("..\\ModLoader\\Trails\\");

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

void SpiritTrail::OnLoadObject(CKSTRING filename, CKSTRING masterName, CK_CLASSID filterClass,
	BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials, BOOL dynamic,
	XObjectArray* objArray, CKObject* masterObj) {
	std::string mapDirs[] = { "3D Entities\\Level\\", "..\\ModLoader\\Maps\\" };
	for (auto& dir : mapDirs) {
		if (!strncmp(filename, dir.c_str(), dir.size())) {
			m_curMap = filename;
			XString filepath = filename;
			m_bml->GetPathManager()->ResolveFileName(filepath, DATA_PATH_IDX);
			m_recordDir = "..\\ModLoader\\Trails\\" + GetFileHash(filepath.CStr()) + "\\";
			MakeSureDirectoryPathExists(m_recordDir.c_str());
		}
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

	if (m_isRecording && !m_recordPaused) {
		m_recordTimer += m_bml->GetTimeManager()->GetLastDeltaTime();
		while (m_recordTimer > 0) {
			m_recordTimer -= 1000.0f / TICK_SPEED;

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

		if (m_bml->IsCheatEnabled())
			StopRecording();
		if (GetSRScore() > 1000 * 3600)
			StopRecording();
	}

	if (m_isPlaying && !m_playPaused) {
		m_playTimer += m_bml->GetTimeManager()->GetLastDeltaTime();
		while (m_playTimer > 0) {
			m_playTimer -= 1000.0f / TICK_SPEED;

			auto& trafos = m_play[m_playhssr].trafo;
			if (m_playTrafo < trafos.size()) {
				auto& trafo = trafos[m_playTrafo];
				if (m_playFrame == trafo.first) {
					SetCurrentBall(trafo.second);
					m_playTrafo++;
				}
			}

			auto& states = m_play[m_playhssr].states;
			if (m_playFrame >= 0 && m_playFrame < states.size()) {
				if (m_playBall >= 0 && m_playBall < m_balls.size()) {
					CKObject* playerBall = m_curLevel->GetElementObject(0, 1);
					CK3dObject* ball = m_balls[m_playBall].obj;
					ball->Show(playerBall->IsVisible() ? CKSHOW : CKHIDE);
					Record::State& state = states[m_playFrame];
					ball->SetPosition(&state.pos);
					ball->SetQuaternion(&state.rot);
				}
			}
			else StopPlaying();

			m_playFrame++;
		}
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

void SpiritTrail::StartPlaying() {
	if (!m_isPlaying && m_enabled->GetBoolean()) {
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

		if (m_play[m_playhssr].states.size() > 0) {
			m_isPlaying = true;
			m_playPaused = false;
			m_playTimer = 0;
			m_playFrame = 0;
			m_playTrafo = 0;
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

void SpiritTrail::StartRecording() {
	if (!m_isRecording && !m_bml->IsCheatEnabled() && m_enabled->GetBoolean()) {
		m_isRecording = true;
		m_recordPaused = false;
		m_bml->AddTimer(2u, [this]() { m_startHS = GetHSScore(); });
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

		if (m_record.hsscore > m_play[0].hsscore || m_record.srscore < m_play[1].srscore) {
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

			if (m_record.hsscore > m_play[0].hsscore) {
				CompressDataToFile(buffer, size, (m_recordDir + "hs" + std::to_string(m_curSector) + ".rec").c_str());
			}

			if (m_record.srscore < m_play[1].srscore) {
				CompressDataToFile(buffer, size, (m_recordDir + "sr" + std::to_string(m_curSector) + ".rec").c_str());
			}

			delete[] buffer;
		}

		StopRecording();
	}
}