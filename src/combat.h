#ifndef COMBAT_H
#define COMBAT_H
//-----------------------------------------------------------------------
// Combat Class
// This class use the properties of NPCs to calculate the battle damage, XP etc.
// Actually is run as a single instance. (if using the getInstance() function)

#include <vector>
#include <irrlicht.h>

#include "GUIManager.h"
#include "DynamicObject.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}


class Combat
{
    public:
        Combat();
		~Combat();

	static Combat* getInstance();
	void attack(DynamicObject* attacker, DynamicObject* defender);
	void dot(DynamicObject* victim, int duration, int damage);
	void dumpProperties (DynamicObject* object);
	void update();

	protected:
	int chances(int min, int max);

	private:

      
		DynamicObject* tempObject;
		lua_State *L;

		vector<DynamicObject*> dotvictim;
		vector<int> dotduration;
		vector<int> dotdamage;
		vector<u32> dottimer;
		bool dotenabled;
      
};

#endif // DYNAMICOBJECT_H
