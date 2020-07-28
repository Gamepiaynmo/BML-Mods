#include "Rotboard.h"

IMod* BMLEntry(IBML* bml) {
	return new RotboardWood(bml);
}

void RotboardWood::OnLoad() {
	m_bml->RegisterModul("P_Rotboard_Wood");
}