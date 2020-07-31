#pragma once

#include <BML/BMLAll.h>

extern "C" {
	__declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

class BMLModuls : public IMod {
public:
	BMLModuls(IBML* bml) : IMod(bml) {}

	virtual CKSTRING GetID() override { return "BMLModuls"; }
	virtual CKSTRING GetVersion() override { return BML_VERSION; }
	virtual CKSTRING GetName() override { return "BML Moduls"; }
	virtual CKSTRING GetAuthor() override { return "Gamepiaynmo"; }
	virtual CKSTRING GetDescription() override { return "Add some new moduls to the game."; }
	DECLARE_BML_VERSION;

	virtual void OnLoad() override;
};