#include "DeformedWB.h"
#include <random>

std::default_random_engine e(time(0));

IMod* BMLEntry(IBML* bml) {
	return new DeformedWB(bml);
}

void DeformedWB::OnLoad() {
	m_enabled = GetConfig()->GetProperty("Misc", "Enable");
	m_enabled->SetComment("Enable deforming player wooden ball");
	m_enabled->SetDefaultBoolean(false);

	m_extent = GetConfig()->GetProperty("Misc", "Extent");
	m_extent->SetComment("A float ranged from 0 to 1 representing the extent of deformation");
	m_extent->SetDefaultFloat(0.25);
}

void DeformedWB::OnLoadObject(CKSTRING filename, BOOL isMap, CKSTRING masterName,
	CK_CLASSID filterClass, BOOL addtoscene, BOOL reuseMeshes, BOOL reuseMaterials,
	BOOL dynamic, XObjectArray* objArray, CKObject* masterObj) {
	if (!strcmp(filename, "3D Entities\\Balls.nmo")) {
		m_ballMesh = m_bml->GetMeshByName("Ball_Wood_Mesh");
		m_vertices.resize(m_ballMesh->GetVertexCount());
		m_normals.resize(m_ballMesh->GetVertexCount());
		for (int i = 0; i < m_ballMesh->GetVertexCount(); i++) {
			m_ballMesh->GetVertexPosition(i, &m_vertices[i]);
			m_ballMesh->GetVertexNormal(i, &m_normals[i]);
		}
	}
}

void DeformedWB::OnStartLevel() {
	if (m_enabled->GetBoolean()) {
		std::uniform_real_distribution<float> rnd(-1, 1);
		VxMatrix proj, invp, scale, invs;
		proj.SetIdentity();
		scale.SetIdentity();
		invs.SetIdentity();
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++)
				proj[i][j] = rnd(e);
			for (int j = 0; j < i; j++)
				proj[i] -= DotProduct(proj[i], proj[j]) * proj[j];
			proj[i] = Normalize(proj[i]);
			scale[i][i] = rnd(e) * m_extent->GetFloat() + 1;
		}

		float slen = scale[0][0] + scale[1][1] + scale[2][2];
		for (int i = 0; i < 3; i++) {
			scale[i][i] *= 3 / slen;
			invs[i][i] = 1 / scale[i][i];
		}

		Vx3DTransposeMatrix(invp, proj);
		VxMatrix def = proj * scale * invp, invd = proj * invs * invp;
		for (int i = 0; i < m_ballMesh->GetVertexCount(); i++) {
			VxVector vertex = def * m_vertices[i];
			VxVector normal = invd * m_normals[i];
			m_ballMesh->SetVertexPosition(i, &vertex);
			m_ballMesh->SetVertexNormal(i, &normal);
		}
	}
	else {
		VxVector tmp; m_ballMesh->GetVertexPosition(0, &tmp);
		if (tmp != m_vertices[0]) {
			for (int i = 0; i < m_ballMesh->GetVertexCount(); i++) {
				m_ballMesh->SetVertexPosition(i, &m_vertices[i]);
				m_ballMesh->SetVertexNormal(i, &m_normals[i]);
			}
		}
	}
};