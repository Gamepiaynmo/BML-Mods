#pragma once

#include <BML/BMLAll.h>

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

class RotboardWood : public IMod {
public:
	RotboardWood(IBML* bml) : IMod(bml) {}

	virtual CKSTRING GetID() override { return "ModulRotboardWood"; }
	virtual CKSTRING GetVersion() override { return BML_VERSION; }
	virtual CKSTRING GetName() override { return "Modul Rotboard Wood"; }
	virtual CKSTRING GetAuthor() override { return "Gamepiaynmo"; }
	virtual CKSTRING GetDescription() override { return "Add a new Wood Rotboard Modul to the game."; }
	DECLARE_BML_VERSION;

	virtual void OnLoad() override;
};