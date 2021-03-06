#include "App.h"
#include "raytests.h" //To test rays

#include "camera/CameraSystem.h" // Camera manager
#include "events/EventReceiver.h" // Event manager
#include "gui/GUIManager.h" // GUI Manager
#include "gui/CGUIFileSelector.h" // GUI custom file selector
#include "terrain/TerrainManager.h" //Terrain Manager
#include "fx/EffectsManager.h" // Fx Manager
#include "LANGManager.h" // Languages string manager
#include "objects/DynamicObjectsManager.h" //Dynamic Object manager class
#include "objects/Projectile.h" //Projectile class manager
#include "LuaGlobalCaller.h" //Lua Global caller functions (basic functions, objects state before/after play)
#include "objects/combat.h" //For updating the combat manager (damage calculation and DOT updates)
#include "Editor/AppEditor.h" //Functions for APP mostly used in editor mode.

#ifdef win32
//To support the experimental filesystem features
#include <filesystem>
#endif

#include "sound/SoundManager.h"
#include "objects/Player.h"


const float DEG2RAD = 3.14159f/180;


App::App()
{
	gamestarted=false;
	tileformat="B3D"; //Default format for tiles.
	logoimage="../media/art/gametitle.jpg";
	filename="";
	appname=L"IrrRPG Builder - Alpha release 0.31 SVN - JULY 2015";
	// Initialize some values
	selector=NULL;
	app_state=APP_EDIT_LOOK;

	textevent.clear();
	lastScannedPick.pickedNode=NULL;
	lastMousePick.pickedNode=NULL;

	selector=NULL;
	saveselector=NULL;
	selectedNode=NULL;
	scriptNode=NULL;
	lastPickedNodeName="";
	lastFilename="";
	timer=0;
	timer2=0;
	timer3=0;
	timer4=0;
	timer_lua=0;
	delta=0;

	ingamebackground=SColor(0,0,0,0); // Default ingame color is black
	moveupdown = false; // Mouse item move up/down in dynamic object ADD mode
	snapfunction = false;

	levelchange=false;

	overdraw=false;
	tex_occluded=NULL; // Texture to put on the player when he is occluded
	tex_normal=NULL;

	df = DF_PROJECT; // Default state for the fileselector (Dialog)

	toolstate = TOOL_NONE; // no tools activated
	old_do_state = TOOL_DO_ADD; // no tools activated
	currentObject = LIST_OBJ;
	toolactivated = false; // no tools activated
	raytester=0; // Initialize and the ray tester class

	current_listfilter = DynamicObject::OBJECT_TYPE_NONE;//Show all the objects in the object list set as initial value

	currentsnapping=64.0f; //set the current snapping distance;
	path = "";
	xeffectenabler=false;
	//Init file functions
	currentProjectName = "";
	currentMapName = "";
	currentMapDescription = "";
	currentMapNo = 0;
	editorfunc = new AppEditor();
	askedClearMap = false;

}

App::~App()
{
	this->cleanWorkspace();

	// Remove the raytester class from memory
	delete raytester;
	if (editorfunc)
		delete editorfunc;

	SoundManager::getInstance()->stopEngine();
	device->drop();
	// exit(0);
}

void App::draw2DImages()
{
#ifdef EDITOR
	if(app_state == APP_EDIT_TERRAIN_TRANSFORM)
	{
		GUIManager::getInstance()->drawHelpImage(GUIManager::HELP_TERRAIN_TRANSFORM);
	}

	if(app_state == APP_EDIT_TERRAIN_PAINT_VEGETATION)
	{
		GUIManager::getInstance()->drawHelpImage(GUIManager::HELP_VEGETATION_PAINT);
	}

	if(app_state == APP_EDIT_TERRAIN_SEGMENTS)
	{
		GUIManager::getInstance()->drawHelpImage(GUIManager::HELP_TERRAIN_SEGMENTS);
	}

	if(app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
	{
	}

	if (app_state > APP_STATE_CONTROL)
	{
		//GUIManager::getInstance()->drawPlayerStats();
	}
	#ifdef DEBUG
	//GUIManager::getInstance()->drawHelpImage(HELP_IRR_RPG_BUILDER_1);
	#endif
#endif
}


void App::displayGuiConsole()
{
	bool result=!guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_CONSOLE,true)->isVisible();
	GUIManager::getInstance()->setElementVisible(GUIManager::CONSOLE,result);
	GUIManager::getInstance()->setConsoleText(L"",true);
}
///TODO: mover isso para GUIManager
// Would be nice to only check the tools windows we have opened and check their position / scale
bool App::cursorIsInEditArea()
{
	bool condition = true;

	//Changed for this, will check ALL the guis and be sure there NO ONE under the mouse
	//Good for determining if we can "pick" a 3D object in the scene
	if (GUIManager::getInstance()->isGuiChildPresent(device->getGUIEnvironment()->getRootGUIElement(),device->getCursorControl()->getPosition()))
		condition = false;

	//Exception for the gameplay bar image. We should be able to select in the empty area
	IGUIImage* gameplay_bar_image = (IGUIImage*)GUIManager::getInstance()->getGUIElement(GUIManager::IMG_BAR);
	if (gameplay_bar_image->isVisible() && gameplay_bar_image->isPointInside(device->getCursorControl()->getPosition()))
	{
		s32 startpos = (gameplay_bar_image->getAbsolutePosition().UpperLeftCorner.Y) + (gameplay_bar_image->getAbsolutePosition().getHeight()/2);
		if (device->getCursorControl()->getPosition().Y<startpos)
			condition = true;
	}


	if(device->getCursorControl()->getPosition().Y < 92 && app_state != APP_GAMEPLAY_NORMAL)  condition = false;

	return condition;
}

App::APP_STATE App::getAppState()
{
	return app_state;
}

void App::setAppState(APP_STATE newAppState)
{

	//just record the state before changing..
	APP_STATE old_app_state = app_state;
	app_state = newAppState;
	//Store the content of the editor inside the object is the app_state changes
	if (app_state!=old_app_state)
	{
		if (old_app_state == APP_EDIT_PLAYER_SCRIPT)
		{
			Player::getInstance()->getObject()->setScript(GUIManager::getInstance()->getEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT));
			scriptNode=NULL;
		}

		if (old_app_state == APP_EDIT_SCRIPT_GLOBAL)
		{
			scriptGlobal = GUIManager::getInstance()->getEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT);
			scriptNode=NULL;
		}

		//If the user was doing other things than editing objects, then the scriptNode will be cleared
		if (old_app_state != APP_EDIT_DYNAMIC_OBJECTS_MODE && old_app_state != APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE && old_app_state != APP_EDIT_DYNAMIC_OBJECTS_SCRIPT)
			scriptNode=NULL;

		if (old_app_state == APP_EDIT_DYNAMIC_OBJECTS_SCRIPT && scriptNode)
			DynamicObjectsManager::getInstance()->getObjectByName(scriptNode->getName())->setScript(GUIManager::getInstance()->getEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT));
	}
#ifdef EDITOR

	if (old_app_state != app_state && app_state != APP_EDIT_VIEWDRAG && app_state != APP_EDIT_DYNAMIC_OBJECTS_MODE)
	{
		if (selectedNode) //Unselect and remove the selected node in mode changes
		{
			//GUIManager::getInstance()->setElementVisible(BT_ID_DO_SEL_MODE,false);
			if (app_state != APP_EDIT_DYNAMIC_OBJECTS_SCRIPT)
			{
				selectedNode->setDebugDataVisible(0);
				selectedNode=NULL;
			}


		}
	}

	if (old_app_state == APP_EDIT_TERRAIN_TRANSFORM && app_state != APP_EDIT_TERRAIN_TRANSFORM)
	{
		// Change the props to be collidable with the ray test
		DynamicObjectsManager::getInstance()->setObjectsID(DynamicObject::OBJECT_TYPE_NON_INTERACTIVE,100);
	}

	if(app_state == APP_EDIT_TERRAIN_TRANSFORM)
	{
		IGUIButton* button=(IGUIButton*)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_ID_TERRAIN_TRANSFORM,true);
		// Change the props to be non-collidable with the ray test
		DynamicObjectsManager::getInstance()->setObjectsID(DynamicObject::OBJECT_TYPE_NON_INTERACTIVE,0x0010);
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_TERRAIN_TOOLBAR,true);
		button->setPressed(true);
		button->setEnabled(false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_TRANSFORM,false);
		GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
		timer1 = device->getTimer()->getRealTime();
	}
	else
	{
		if (old_app_state == APP_EDIT_TERRAIN_TRANSFORM)
		{
			IGUIButton* button=(IGUIButton*)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_ID_TERRAIN_TRANSFORM,true);
			GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_TERRAIN_TOOLBAR,false);
			ShaderCallBack::getInstance()->setFlagEditingTerrain(false);
			button->setEnabled(true);
			button->setPressed(false);

			GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
		}
	}

	if(app_state == APP_EDIT_TERRAIN_PAINT_VEGETATION)
	{
		IGUIButton* button=(IGUIButton*)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_ID_TERRAIN_PAINT_VEGETATION,true);
		if (button)
		{
			GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_VEGE_TOOLBAR,true);
			button->setEnabled(false);
			button->setPressed(true);
		}
		GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
		if (selectedNode)
		{
			selectedNode->setDebugDataVisible(0);
			selectedNode=NULL;
		}
		timer1 = device->getTimer()->getRealTime();
	}
	else
	{
		if (old_app_state == APP_EDIT_TERRAIN_PAINT_VEGETATION)
		{
			IGUIButton* button=(IGUIButton*)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_ID_TERRAIN_PAINT_VEGETATION,true);
			button->setEnabled(true);
			button->setPressed(false);
			GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_VEGE_TOOLBAR,false);
			GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
		}
	}

	if(app_state == APP_EDIT_TERRAIN_SEGMENTS)
	{
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_SEGMENT,false);
		GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
		if (selectedNode)
		{
			selectedNode->setDebugDataVisible(0);
			selectedNode=NULL;
		}
	}
	else
	{
		if (old_app_state == APP_EDIT_TERRAIN_SEGMENTS)
		{
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_SEGMENT,true);
			GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
		}
	}

	if(app_state == APP_EDIT_TERRAIN_EMPTY_SEGMENTS)
	{
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_EMPTY_SEGMENT,false);
		GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
	}
	else
	{
		if (old_app_state == APP_EDIT_TERRAIN_EMPTY_SEGMENTS)
		{
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_EMPTY_SEGMENT,true);
			GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
		}
	}

	if (app_state == APP_EDIT_TERRAIN_CUSTOM_SEGMENTS) //Old code should not be used.
	{
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT,false);
		GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_CUSTOM_SEGMENT_CHOOSER,true);
		if (selectedNode)
		{
			selectedNode->setDebugDataVisible(0);
			selectedNode=NULL;
		}
	}
	else
	{
		if  (old_app_state == APP_EDIT_TERRAIN_CUSTOM_SEGMENTS)//Old code should not be used.
		{
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT,true);
			GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());

			if (selectedNode)
			{
				selectedNode->setDebugDataVisible(0);
				selectedNode=NULL;
			}
			toolstate = TOOL_NONE;
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TILE_ROT_LEFT,false);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TILE_ROT_RIGHT,false);
		}
	}

	if(old_app_state == APP_EDIT_TERRAIN_CUSTOM_SEGMENTS)
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_CUSTOM_SEGMENT_CHOOSER,false);

	if(app_state == APP_EDIT_TERRAIN_TRANSFORM)
	{
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_TRANSFORM,false);
		GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
	}
	else
	{
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_TRANSFORM,true);
	}


	if(app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
	{
		//If the tools was in move/rotate mode then set back the current mode as the old tool state
		//if (old_app_state == APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE || old_app_state == APP_EDIT_DYNAMIC_OBJECTS_SCRIPT)
			toolstate = old_do_state;

		if (old_app_state != APP_EDIT_VIEWDRAG)
		{

			if (old_app_state != APP_EDIT_DYNAMIC_OBJECTS_MODE)
			{
				//Reset to the last button states (based on the "currentObject" value)
				//Only valid if the old APP State was not dynamic object mode and viewdrag
				GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT,currentObject!=LIST_SEGMENT);
				GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,currentObject!=LIST_OBJ);
				GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_PROPS,currentObject!=LIST_PROPS);
				GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_LOOT,currentObject!=LIST_LOOT);
			}
			GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DYNAMIC_OBJECT_CHOOSER,true);
			GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
			//If the up/down mode was last used then reset if
			if (moveupdown)
				moveupdown=false;

			//GUIManager::getInstance()->UpdateGUIChooser();
			//GUIManager::getInstance()->updateCurrentCategory(currentObject);
			if ((old_app_state != APP_EDIT_DYNAMIC_OBJECTS_MODE && old_app_state != APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE) || old_app_state == APP_EDIT_CHARACTER)
				GUIManager::getInstance()->expandPanel(GUIManager::GCW_DYNAMIC_OBJECT_CHOOSER);

			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT,currentObject!=LIST_SEGMENT);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,currentObject!=LIST_OBJ);

		}


	}
	else
	{
		//Reset the tools state if going outside of the dynamic object edit mode
		if (app_state != APP_EDIT_VIEWDRAG && app_state!=APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE)
		{
			GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DYNAMIC_OBJECT_CHOOSER,false);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,true);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT,true);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,true);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_LOOT,true);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_PROPS,true);
			toolstate = TOOL_NONE;
		}
	}

	if(app_state != APP_EDIT_ABOUT)
	{
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ABOUT,false);
	}

	if(app_state == APP_EDIT_DYNAMIC_OBJECTS_SCRIPT)
	{
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DYNAMIC_OBJECTS_EDIT_SCRIPT,true);
	}
	else
	{
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DYNAMIC_OBJECTS_EDIT_SCRIPT,false);
	}

	if(app_state == APP_EDIT_CHARACTER)
	{
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DYNAMIC_PLAYER_EDIT,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_EDIT_CHARACTER,false);
		Player::getInstance()->setHighLight(true);
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,false);
	}
	else
	{
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DYNAMIC_PLAYER_EDIT,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_EDIT_CHARACTER,true);

		Player::getInstance()->setHighLight(false);
	}

	if(app_state == APP_EDIT_SCRIPT_GLOBAL)
	{
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_EDIT_SCRIPT_GLOBAL,false);
		GUIManager::getInstance()->setEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT,scriptGlobal);
		GUIManager::getInstance()->setEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT_CONSOLE,"");
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DYNAMIC_OBJECTS_EDIT_SCRIPT,true);
	}
	else
	{
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_EDIT_SCRIPT_GLOBAL,true);
	}

	if (app_state == APP_EDIT_PLAYER_SCRIPT)
	{
		IGUIWindow* win = ((IGUIWindow*)guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_DYNAMIC_OBJECTS_EDIT_SCRIPT,true));
		if (win)
		{
			win->setText(L"EDITING PLAYER SCRIPT");
		}
		GUIManager::getInstance()->setEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT,Player::getInstance()->getObject()->getScript());
		GUIManager::getInstance()->setEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT_CONSOLE,"");
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DYNAMIC_OBJECTS_EDIT_SCRIPT,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_PLAYER_EDIT_SCRIPT,false);
	}
	else
	{
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_PLAYER_EDIT_SCRIPT,true);
	}
#endif

	if(app_state == APP_GAMEPLAY_NORMAL)
	{

		if (this->isXEffectsEnabled())
			DynamicObjectsManager::getInstance()->displayShadow(false);

		GUIManager::getInstance()->setElementVisible(GUIManager::BT_ID_PLAY_GAME,false);
		GUIManager::getInstance()->setElementVisible(GUIManager::BT_ID_STOP_GAME,true);
		//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_SEGMENT,false);
		//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_PAINT_VEGETATION,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_TRANSFORM,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_SAVE_PROJECT,false);
		//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_LOAD_PROJECT,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_EDIT_CHARACTER,false);
		//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_EDIT_SCRIPT_GLOBAL,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_ABOUT,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_NEW_PROJECT,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_LOOT,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_PROPS,false);
		//This current button is for the console
		//GUIManager::getInstance()->setElementEnabled(BT_ID_HELP,false);
		GUIManager::getInstance()->setElementVisible(GUIManager::IMG_BAR,true);
		GUIManager::getInstance()->setElementVisible(GUIManager::BT_ID_VIEW_ITEMS,true);
#ifdef EDITOR
		guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_VIEW_MENU,true)->setVisible(false);
		//guienv->getRootGUIElement()->getElementFromId(GUIManager::CB_SNAPCOMBO,true)->setVisible(false); //Hide the snap box when playing
		//guienv->getRootGUIElement()->getElementFromId(GUIManager::CB_SCREENCOMBO,true)->setVisible(false);
#endif
	}
	else if(app_state < APP_STATE_CONTROL && app_state!=APP_EDIT_VIEWDRAG)
	{
		DynamicObjectsManager::getInstance()->displayShadow(true);
		GUIManager::getInstance()->setElementVisible(GUIManager::BT_ID_PLAY_GAME,true);
		GUIManager::getInstance()->setElementVisible(GUIManager::BT_ID_STOP_GAME,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_SAVE_PROJECT,true);
		//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_LOAD_PROJECT,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_ABOUT,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_NEW_PROJECT,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_HELP,true);
		GUIManager::getInstance()->setElementVisible(GUIManager::IMG_BAR,false);
		GUIManager::getInstance()->setElementVisible(GUIManager::BT_ID_VIEW_ITEMS,false);
		//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_LOOT,true);
		//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_PROPS,true);
		//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_PAINT_VEGETATION,true);
		//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_TRANSFORM,true);
#ifdef EDITOR
		guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_VIEW_MENU,true)->setVisible(true); ///Show the snap box when editingGCW_VIEW_MENU
		//guienv->getRootGUIElement()->getElementFromId(GUIManager::CB_SNAPCOMBO,true)->setVisible(true); ///Show the snap box when editing
		//guienv->getRootGUIElement()->getElementFromId(GUIManager::CB_SCREENCOMBO,true)->setVisible(true);
#endif
	} else if(app_state == APP_WAIT_DIALOG)
	{
		GUIManager::getInstance()->setElementVisible(GUIManager::BT_ID_VIEW_ITEMS,false);
		GUIManager::getInstance()->setElementVisible(GUIManager::IMG_BAR,false);
	}


#ifdef EDITOR
	if (app_state == APP_EDIT_VIEWDRAG)
	{
		GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_drag").c_str());
	} else
	{
		GUIManager::getInstance()->setStatusText(LANGManager::getInstance()->getText("info_dynamic_objects_mode").c_str());
	}
#endif // EDITOR

}


// Check for buttons pressed
void App::eventGuiButton(s32 id)
{
	DynamicObject* object=NULL;
#ifdef EDITOR
	DynamicObject* selectedObject=NULL;
#endif

	bool result = false;
	oldcampos = vector3df(0,0,0);
	vector3df oldrotation = vector3df(0,0,0);
	core::stringw oldscript = L"";

	//Used to have the camera modes toggle
	IGUIButton* button1 = NULL;
	IGUIButton* button2 = NULL;
	IGUIButton* button3 = NULL;

	// Containing the project window
	IGUIWindow* prj = NULL;

	MousePick the=getMousePosition3D(); //Will store the last mousepick when the gui button was pressed

	IGUIListBox* box = NULL; // Combo box pointer
	core::stringw listitem = L""; //Store listitems
	IGUIElement* elem = NULL; //Store a generic element;
	IGUIStaticText* text = NULL;

	IGUIEditBox* editbox = NULL;
	mapinfo map;

	stringc textc = "";
	stringw textw = L"";
	u32 value = 0;

	DynamicObject* loot=GUIManager::getInstance()->getActiveLootItem(); //Get the currently selected loot object

	switch (id)
	{

	case GUIManager::BT_ID_CREATE_PROJECT:
		result=createProjectData();
		if (result)
		{
			prj = (IGUIWindow*)guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_NEWPROJECT, true);
			if (prj)
			{
				guienv->setFocus(NULL);
				prj->remove();
				this->createNewMap();
			}
		}
		else
		{
			device->getGUIEnvironment()->addMessageBox(L"", stringw(L"You must enter the name for the project AND the first map!").c_str(), true);
		}

		break;


	case GUIManager::BT_ID_NEW_PROJECT:
		app_state = APP_EDIT_PRJ;
		GUIManager::getInstance()->createNewProjectGUI();
		break;

	case GUIManager::BT_ID_LOAD_PROJECT:
		//This button is now used in the CREATE PROJECT WINDOW.
		this->setAppState(APP_EDIT_LOOK);
		prj = (IGUIWindow*)guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_NEWPROJECT, true);
		if (prj)
			prj->setVisible(false); // Hide the new project window while work is being done

		elem = GUIManager::getInstance()->getGUIElement(GUIManager::LISTBOX_PROJECTS);
		if (elem)
		{
			box = (IGUIListBox*)elem;
			listitem = box->getListItem(box->getSelected());
			if (listitem.size() > 0)
			{
				currentProjectName = listitem;
				loadProjectFromXML();
			}
		}

		//Remove the window project if this is found. It require the listbox in that window.
		prj = (IGUIWindow*)guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_NEWPROJECT, true);
		if (prj)
		{
			guienv->setFocus(NULL);
			prj->remove();
		}

		break;

	case GUIManager::BT_ID_SAVE_PROJECT:
		//this->saveProjectDialog();
		this->setAppState(APP_EDIT_LOOK);
		saveProjectToXML();
		break;
#ifdef EDITOR


	case GUIManager::BT_ID_MAP_ADMIN:
		elem = GUIManager::getInstance()->getGUIElement(GUIManager::GCW_MAP_TOOLBAR);
		if (!elem)
			GUIManager::getInstance()->createMapAdminToolbar();
		break;

	case GUIManager::BT_MA_OPEN_MAP:
		elem = guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_MAP_TOOLBAR, true);
		if (elem)
		{
			box = (IGUIListBox*)guienv->getRootGUIElement()->getElementFromId(GUIManager::LISTBOX_MA_MAPS, true);
			this->currentMapName = mapinfos[box->getSelected()].mapname;
			this->currentMapDescription = mapinfos[box->getSelected()].mapdescription;
			textc = core::stringc(editorfunc->getProjectsPath()) + "/";
			textc += core::stringc(currentProjectName);
			textc += "/";
			textc += stringc(currentMapName.c_str());
			textc += "/";
			textc += stringc(currentMapName.c_str());
			textc += ".map";
			currentMapNo = box->getSelected();
			elem->remove();
			createNewMap(); //Clear everything
			loadMapFromXML(textc);


		}

	case GUIManager::BT_MA_UPDATE_DESC:
		elem = guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_MA_DESC, true);
		if (elem)
		{

			value = GUIManager::getInstance()->getListBox(GUIManager::LISTBOX_MA_MAPS)->getSelected();
			text = (IGUIStaticText*)elem;

			mapinfos[value].mapdescription = (core::stringw)text->getText();
			currentMapDescription = (core::stringw)text->getText();
			device->getGUIEnvironment()->addMessageBox(L"Information:", stringw(L"Description updated!").c_str(), true);
		}
		break;

	case GUIManager::BT_MA_CLEAR_MAP:
		guienv->addMessageBox(L"WARNING!", L"This will reset all the content of this map, do you still want to proceed?", true, EMBF_NO | EMBF_YES);
		askedClearMap = true;
		break;

	case GUIManager::BT_MA_CREATE_MAP:
		GUIManager::getInstance()->createNewMapRequest();
		break;

	case GUIManager::BT_MA_RENAME_MAP:
		box = (IGUIListBox*)guienv->getRootGUIElement()->getElementFromId(GUIManager::LISTBOX_MA_MAPS, true);
		GUIManager::getInstance()->createRenameRequest(mapinfos[box->getSelected()].mapname);
		break;

	case GUIManager::BT_MAPREQUEST_CREATE:
		editbox = (IGUIEditBox*)GUIManager::getInstance()->getGUIElement(GUIManager::TB_MAPREQUEST_MAP);
		if (editbox)
			map.mapname = editbox->getText();
		editbox = (IGUIEditBox*)GUIManager::getInstance()->getGUIElement(GUIManager::TB_MAPREQUEST_DESC);
		if (editbox)
			map.mapdescription = editbox->getText();

		if (map.mapname.size() > 0)
		{
			mapinfos.push_back(map);
			currentMapNo++;
			currentMapName = map.mapname;
			currentMapDescription = map.mapdescription;
			elem = GUIManager::getInstance()->getGUIElement(GUIManager::GCW_MAP_TOOLBAR);
			if (elem)
				elem->remove();
			elem = GUIManager::getInstance()->getGUIElement(GUIManager::GCW_REQUEST_MAP_INFO);
			if (elem)
				elem->remove();

			createNewMap();
		}



		break;

	case GUIManager::BT_MAPREQUEST_CANCEL:
		elem = GUIManager::getInstance()->getGUIElement(GUIManager::GCW_MAP_TOOLBAR);
		if (elem)
			elem->setVisible(true);
		elem = GUIManager::getInstance()->getGUIElement(GUIManager::GCW_REQUEST_MAP_INFO);
		if (elem)
			elem->remove();
		break;

	//Action to do when the user press the Cancel button on the rename map request
	case GUIManager::BT_REQUEST_MAPRENAME_CANCEL:
		elem = GUIManager::getInstance()->getGUIElement(GUIManager::GCW_MAP_TOOLBAR);
		if (elem)
			elem->setVisible(true);
		elem = GUIManager::getInstance()->getGUIElement(GUIManager::GCW_REQUEST_RENAME);
		if (elem)
			elem->remove();
		break;

	//Action to do when the user press the RENAME button on the rename map request
	case GUIManager::BT_REQUEST_MAPRENAME:
		editbox = (IGUIEditBox*)GUIManager::getInstance()->getGUIElement(GUIManager::TB_MR_RENAME_NAME);
		this->renameMap(currentMapName, editbox->getText(), true);
		this->renameMap(currentMapName, editbox->getText(), false);
		currentMapName = editbox->getText();
		mapinfos[currentMapNo].mapname = currentMapName;
		elem = GUIManager::getInstance()->getGUIElement(GUIManager::GCW_MAP_TOOLBAR);
		if (elem)
			elem->remove();
		elem = GUIManager::getInstance()->getGUIElement(GUIManager::GCW_REQUEST_RENAME);
		if (elem)
			elem->remove();
		saveProjectToXML();
		break;

	case GUIManager::BT_ID_TERRAIN_ADD_SEGMENT:
		this->setAppState(APP_EDIT_TERRAIN_SEGMENTS);
		break;

	case GUIManager::BT_ID_TERRAIN_ADD_EMPTY_SEGMENT:
		this->setAppState(APP_EDIT_TERRAIN_EMPTY_SEGMENTS);
		break;

	case GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT:
		GUIManager::getInstance()->UpdateCollections(GUIManager::LIST_SEGMENT);
		currentObject=LIST_SEGMENT;
		GUIManager::getInstance()->UpdateGUIChooser(GUIManager::LIST_SEGMENT);
		GUIManager::getInstance()->updateCurrentCategory(GUIManager::LIST_SEGMENT);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_LOOT,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_PROPS,true);
		DynamicObjectsManager::getInstance()->setActiveObject(GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_DYNAMIC_OBJECT_OBJ_CHOOSER));
		this->setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
		if (toolstate==TOOL_NONE)
			toolstate=old_do_state;
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECTS_PROPS:
		this->setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
		if (toolstate==TOOL_NONE)
			toolstate=old_do_state;

		GUIManager::getInstance()->UpdateCollections(GUIManager::LIST_PROP);
		currentObject=LIST_PROPS;
		GUIManager::getInstance()->UpdateGUIChooser(GUIManager::LIST_PROP);
		GUIManager::getInstance()->updateCurrentCategory(GUIManager::LIST_PROP);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_LOOT,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_PROPS,false);
		DynamicObjectsManager::getInstance()->setActiveObject(GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_DYNAMIC_OBJECT_OBJ_CHOOSER));

		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECTS_LOOT:
		this->setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
		if (toolstate==TOOL_NONE)
			toolstate=old_do_state;

		GUIManager::getInstance()->UpdateCollections(GUIManager::LIST_LOOT);
		currentObject=LIST_LOOT;
		GUIManager::getInstance()->UpdateGUIChooser(GUIManager::LIST_LOOT);
		GUIManager::getInstance()->updateCurrentCategory(GUIManager::LIST_LOOT);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,true);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_LOOT,false);
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_PROPS,true);
		DynamicObjectsManager::getInstance()->setActiveObject(GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_DYNAMIC_OBJECT_OBJ_CHOOSER));

		break;

	case GUIManager::BT_ID_TERRAIN_PAINT_VEGETATION:
		this->setAppState(APP_EDIT_TERRAIN_PAINT_VEGETATION);
		break;

	case GUIManager::BT_ID_TERRAIN_TRANSFORM:
		this->setAppState(APP_EDIT_TERRAIN_TRANSFORM);
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE:
		{
			currentObject=LIST_OBJ;
			GUIManager::getInstance()->UpdateCollections(GUIManager::LIST_NPC);
			GUIManager::getInstance()->UpdateGUIChooser();
			GUIManager::getInstance()->updateCurrentCategory();
			DynamicObjectsManager::getInstance()->setActiveObject(GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_DYNAMIC_OBJECT_OBJ_CHOOSER));
			this->setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,false);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TERRAIN_ADD_CUSTOM_SEGMENT,true);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_LOOT,true);
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_PROPS,true);
			if (toolstate==TOOL_NONE)
				toolstate=old_do_state;
		}
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_BT_CANCEL:
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_BT_SPAWN: // Create a new item from the last selected item in the dynamic object
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,false);

		DynamicObjectsManager::getInstance()->createActiveObjectAt(lastMousePick.pickedPos);
		GUIManager::getInstance()->buildSceneObjectList(current_listfilter);
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_BT_REPLACE: // Will replace the model with one from the file selector
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,false);
		loadProject(DF_MODEL);
		GUIManager::getInstance()->buildSceneObjectList(current_listfilter);

	break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_BT_REPLACE2: // Will replace the model with the one selected in the item template

		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,false);


		//----
		if (lastMousePick.pickedNode)
		{
			core::stringc nodeName = lastMousePick.pickedNode->getName();
			if( stringc( nodeName.subString(0,14)) == "dynamic_object" || nodeName.subString(0,16) == "dynamic_walkable" )
			{
				//Tell the dynamic Objects Manager to remove the node
				if (lastMousePick.pickedNode)
				{
					lastMousePick.pickedNode->setDebugDataVisible(0);
					selectedNode=NULL;
				}

				// Keep the "good stuff"
				oldrotation = lastMousePick.pickedNode->getRotation();
				oldscript = DynamicObjectsManager::getInstance()->getScript(lastMousePick.pickedNode->getName());

				DynamicObjectsManager::getInstance()->removeObject(lastMousePick.pickedNode->getName());
				// remove the object for the selection
				lastScannedPick.pickedNode=NULL;
				lastMousePick.pickedNode=NULL;

				// Create the new object from the template and put the old values back in.
				object = DynamicObjectsManager::getInstance()->createActiveObjectAt(lastMousePick.pickedPos);
				object->setScript(oldscript);
				object->setRotation(oldrotation);

				//Reselect the replaced object (in the other modes, in ADD mode selection is done automatically)
				if (toolstate!=TOOL_DO_ADD)
				{
					selectedNode=object->getNode();
					selectedNode->setDebugDataVisible(true ? EDS_BBOX | EDS_SKELETON : EDS_OFF);
				}

				//Once the change is done rebuild the list
				GUIManager::getInstance()->buildSceneObjectList(current_listfilter);
			}
			else //Wrong node type selected
			{
				guienv->addMessageBox(L"No object selected",(L"You need to select an object to replace it."),true);
			}
		}
		else //Nothing selected
		{
			guienv->addMessageBox(L"No object selected",(L"You need to select an object to replace it."),true);
		}
		if (app_state!=APP_EDIT_DYNAMIC_OBJECTS_MODE)
			setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_BT_EDITSCRIPTS:

		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,false);

		if (lastMousePick.pickedNode)
		{
			core::stringc nodeName = lastMousePick.pickedNode->getName();

			if( stringc( nodeName.subString(0,14)) == "dynamic_object" || nodeName.subString(0,16) == "dynamic_walkable" )
			{
				selectedObject = DynamicObjectsManager::getInstance()->getObjectByName( stringc(nodeName) );
				scriptNode = selectedObject->getNode();
				GUIManager::getInstance()->setEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT,"");
				GUIManager::getInstance()->setEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT,selectedObject->getScript());
				GUIManager::getInstance()->setEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT_CONSOLE,"");
				IGUIWindow* win = ((IGUIWindow*)guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_DYNAMIC_OBJECTS_EDIT_SCRIPT,true));
				if (win)
				{
					core::stringw title = core::stringw("EDITING OBJECT SCRIPT: ");
					title.append(nodeName);
					win->setText(title.c_str());
				}
				this->setAppState(APP_EDIT_DYNAMIC_OBJECTS_SCRIPT);
			}
			else //Wrong node type selected
			{
				guienv->addMessageBox(L"No object selected",(L"You need to select an object to edit it's script."),true);
			}
		}
		else //Nothing selected
		{
			guienv->addMessageBox(L"No object selected",(L"You need to select an object to edit it's script."),true);
		}


		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_LOAD_SCRIPT_TEMPLATE:
		{
			stringw newScript = "";

			wchar_t out[4096];
			stringc filename = "../media/scripts/";
			filename += GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_DYNAMIC_OBJECT_LOAD_SCRIPT_TEMPLATE);

			std::string line;
			ifstream fileScript (filename.c_str());
			if (fileScript.is_open())
			{
				while (! fileScript.eof() )
				{
					getline (fileScript,line);
					line+='\n';
					utf8ToWchar(line.c_str(), out, 4096);
					newScript += stringw(out);

				}
				fileScript.close();
			}

			GUIManager::getInstance()->setEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT,newScript);
		}
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_BT_REMOVE:
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,false);
		if (lastMousePick.pickedNode)
		{
			core::stringc nodeName = lastMousePick.pickedNode->getName();
			if( stringc( nodeName.subString(0,14)) == "dynamic_object" || nodeName.subString(0,16) == "dynamic_walkable" )
			{
				//Tell the dynamic Objects Manager to remove the node
				if (lastMousePick.pickedNode)
				{
					lastMousePick.pickedNode->setDebugDataVisible(0);
					selectedNode=NULL;
				}
				DynamicObjectsManager::getInstance()->removeObject(lastMousePick.pickedNode->getName());
				// remove the object for the selection
				lastScannedPick.pickedNode=NULL;
				lastMousePick.pickedNode=NULL;
				GUIManager::getInstance()->buildSceneObjectList(current_listfilter);
			}
			else //Wrong node type selected
			{
				guienv->addMessageBox(L"No object selected",(L"You need to select an object to remove it."),true);
			}
		}
		else //Nothing selected
		{
			guienv->addMessageBox(L"No object selected",(L"You need to select an object to remove it."),true);
		}
		if (app_state!=APP_EDIT_DYNAMIC_OBJECTS_MODE)
			setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
		break;

		//Center the view on the selected object
	case GUIManager::BT_ID_DYNAMIC_OBJECT_BT_CENTER:
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,false);
		if (lastMousePick.pickedNode)
		{
			core::vector3df pos = lastMousePick.pickedNode->getPosition();
			core::vector3df offset = CameraSystem::getInstance()->getNode()->getPosition();
			core::vector3df calc = pos + (pos - offset);

			CameraSystem::getInstance()->setPosition(calc);
			CameraSystem::getInstance()->getNode()->setTarget(pos);
		}
		else
			guienv->addMessageBox(L"No object selected",(L"You need to select the object to center the view on it."),true);

		break;
	//Center the view on the selected object, from a button on tool panel
	case GUIManager::BT_ID_DYNAMIC_VIEW_BT_CENTER:
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU1,false);

		if (the.pickedNode)
		{
			core::vector3df pos = the.pickedPos;
			core::vector3df offset = CameraSystem::getInstance()->getNode()->getPosition();
			core::vector3df calc = pos + (pos - offset);

			CameraSystem::getInstance()->setPosition(calc);
			CameraSystem::getInstance()->getNode()->setTarget(pos);
			// re-center the cursor also
			device->getCursorControl()->setPosition(vector2df(0.5f,0.5f));
		}
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_BT_MOVEROTATE:
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,false);
		setAppState(APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE);
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_VALIDATE_SCRIPT:
		LuaGlobalCaller::getInstance()->doScript(GUIManager::getInstance()->getEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT));
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_SCRIPT_CLOSE:
		//Will save the script if the window is closed
		if(app_state == APP_EDIT_DYNAMIC_OBJECTS_SCRIPT)
		{
			if (scriptNode)
				DynamicObjectsManager::getInstance()->getObjectByName(scriptNode->getName())->setScript(GUIManager::getInstance()->getEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT));

			setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
		}
		else if(app_state == APP_EDIT_PLAYER_SCRIPT)
		{
			Player::getInstance()->getObject()->setScript(GUIManager::getInstance()->getEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT));
			setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
		}
		else if(app_state == APP_EDIT_SCRIPT_GLOBAL)
		{
			scriptGlobal = GUIManager::getInstance()->getEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT);
			setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
		}
		//GUIManager::getInstance()->setWindowVisible(GCW_DYNAMIC_OBJECTS_EDIT_SCRIPT,false);
		break;

	case GUIManager::BT_ID_EDIT_CHARACTER:
		this->setAppState(APP_EDIT_CHARACTER);
		break;

	case GUIManager::BT_ID_PLAYER_EDIT_SCRIPT:
		this->setAppState(APP_EDIT_PLAYER_SCRIPT);
		break;

	case GUIManager::BT_ID_EDIT_SCRIPT_GLOBAL:
		this->setAppState(APP_EDIT_SCRIPT_GLOBAL);
		break;

	case GUIManager::BT_ID_CONFIG:
		GUIManager::getInstance()->showConfigWindow();
		break;
#endif
	case GUIManager::BT_ID_PLAY_GAME:
		DynamicObjectsManager::getInstance()->buildInteractiveList();
		playGame();
		break;

	case GUIManager::BT_ID_STOP_GAME:
		stopGame();
		break;

	case GUIManager::BT_ID_CLOSE_PROGRAM:
		prj = (IGUIWindow*)(guienv->getRootGUIElement()->getElementFromId(GUIManager::GCW_NEWPROJECT,true));
		if (prj)
		{
			guienv->setFocus(NULL);
			prj->remove();
		}

		this->shutdown();
		break;

	case GUIManager::BT_ID_HELP:
		this->displayGuiConsole();
		break;

	case GUIManager::BT_ID_ABOUT:
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ABOUT,true);
		setAppState(APP_EDIT_ABOUT);
		break;

	case GUIManager::BT_ID_ABOUT_WINDOW_CLOSE:
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ABOUT,false);
		setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
		break;

	case GUIManager::BT_ID_VIEW_ITEMS:
		setAppState(APP_GAMEPLAY_VIEW_ITEMS);
		DynamicObjectsManager::getInstance()->freezeAll();
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_GAMEPLAY_ITEMS,true);
		GUIManager::getInstance()->drawPlayerStats();
		break;

	case GUIManager::BT_ID_USE_ITEM:
		//Will remove the item from the bag after use and destry it if the flag is set
		if (loot)
		{
			loot->notifyUse(); //Call the lua callback onUse() in the object

			if (loot->isDestroyedAfterUse)
			{
				Player::getInstance()->getObject()->removeLoot(loot);
				loot->setLife(0); //kill it
				loot->getNode()->setParent(smgr->getRootSceneNode());
				loot->setPosition(Player::getInstance()->getNode()->getPosition());
				loot->isInBag=false;
			}
			GUIManager::getInstance()->updateItemsList();
		}
		break;

	case GUIManager::BT_ID_DROP_ITEM:
		//"loot" dynamic object will be taken from the selection list and put here
		if (loot)
		{
			loot->getNode()->setParent(smgr->getRootSceneNode());
			loot->getNode()->setPosition(Player::getInstance()->getNode()->getPosition());
			loot->getNode()->setVisible(true);
			Player::getInstance()->getObject()->removeLoot(loot);
			loot->isInBag=false;
		}
		GUIManager::getInstance()->updateItemsList();
		break;

	case GUIManager::BT_ID_CLOSE_ITEMS_WINDOW:
		setAppState(APP_GAMEPLAY_NORMAL);
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_GAMEPLAY_ITEMS,false);
		DynamicObjectsManager::getInstance()->unFreezeAll();
		break;

	case GUIManager::BT_ID_DIALOG_YES:
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DIALOG,false);
		if (app_state> APP_STATE_CONTROL)
		{
			//Player::getInstance()->getObject()->notifyAnswer(true);
			if (DynamicObjectsManager::getInstance()->getDialogCaller())
				DynamicObjectsManager::getInstance()->getDialogCaller()->notifyAnswer(true);
			setAppState(APP_GAMEPLAY_NORMAL);
			GUIManager::getInstance()->stopDialogSound();
		}
		break;

	case GUIManager::BT_ID_DIALOG_CANCEL:
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DIALOG,false);
		if (app_state> APP_STATE_CONTROL)
		{
			if (DynamicObjectsManager::getInstance()->getDialogCaller())
				DynamicObjectsManager::getInstance()->getDialogCaller()->notifyAnswer(false);
			setAppState(APP_GAMEPLAY_NORMAL);
			GUIManager::getInstance()->stopDialogSound();
		}
		break;

	case GUIManager::BT_ID_DYNAMIC_OBJECT_INFO: // Expand/Retract the pane for the info of the dynamic object with the button
		GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_DYNAMIC_OBJECT_INFO,false);
		break;

	case GUIManager::BT_ID_TILE_ROT_LEFT: // User pressed the rotate tile left toggle button
		if (toolstate != TOOL_TILEROTATE_LEFT)
		{
			toolstate = TOOL_TILEROTATE_LEFT;
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TILE_ROT_RIGHT,false);
		}
		else
			toolstate = TOOL_NONE;
		break;

	case GUIManager::BT_ID_TILE_ROT_RIGHT: // User pressed the rotate tile right toggle button
		if (toolstate != TOOL_TILEROTATE_RIGHT)
		{
			toolstate = TOOL_TILEROTATE_RIGHT;
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_TILE_ROT_LEFT,false);
		}
		else
			toolstate = TOOL_NONE;
		break;

	case GUIManager::BT_ID_DO_ADD_MODE:
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_ADD_MODE,true);
		toolstate = TOOL_DO_ADD;
		old_do_state = toolstate;
		if (selectedNode)
		{
			selectedNode->setDebugDataVisible(0);
			selectedNode=NULL;
		}
		toolactivated=false;
		moveupdown=false;
		break;

	case GUIManager::BT_ID_DO_SEL_MODE:
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_SEL_MODE,true);
		toolstate = TOOL_DO_SEL;
		old_do_state = toolstate;
		if (selectedNode)
			GUIManager::getInstance()->updateNodeInfos(selectedNode);
		toolactivated=false;
		moveupdown=false;
		break;

	case GUIManager::BT_ID_DO_MOV_MODE:
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_MOV_MODE,true);
		toolstate = TOOL_DO_MOV;
		old_do_state = toolstate;
		if (selectedNode)
			GUIManager::getInstance()->updateNodeInfos(selectedNode);
		toolactivated=false;
		moveupdown=false;
		break;

	case GUIManager::BT_ID_DO_ROT_MODE:
		toolstate = TOOL_DO_ROT;
		old_do_state = toolstate;
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_ROT_MODE,true);
		if (selectedNode)
			GUIManager::getInstance()->updateNodeInfos(selectedNode);
		toolactivated=false;
		moveupdown=false;
		break;

	case GUIManager::BT_ID_DO_SCA_MODE:
		GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_SCA_MODE,true);
		if (selectedNode)
			GUIManager::getInstance()->updateNodeInfos(selectedNode);
		toolstate = TOOL_DO_SCA;
		old_do_state = toolstate;
		toolactivated=false;
		moveupdown=false;
		break;

	case GUIManager::BT_PLAYER_START:
		this->playGame();
		break;

	case GUIManager::BT_PLAYER_CONFIG:
		GUIManager::getInstance()->showConfigWindow();

		break;

	case GUIManager::BT_CAMERA_RTS:
		button1 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RTS,true));
		button2 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RPG,true));
		button3 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_FPS,true));
		if (button1)
			button1->setPressed(true);
		if (button2)
			button2->setPressed(false);
		if (button3)
			button3->setPressed(false);

		CameraSystem::getInstance()->setViewType(CameraSystem::VIEW_RTS);
		break;

	case GUIManager::BT_CAMERA_RPG:
		button1 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RTS,true));
		button2 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RPG,true));
		button3 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_FPS,true));
		if (button1)
			button1->setPressed(false);
		if (button2)
			button2->setPressed(true);
		if (button3)
			button3->setPressed(false);

		CameraSystem::getInstance()->setViewType(CameraSystem::VIEW_RPG);
		break;

	case GUIManager::BT_CAMERA_FPS:
		button1 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RTS,true));
		button2 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RPG,true));
		button3 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_FPS,true));
		if (button1)
			button1->setPressed(false);
		if (button2)
			button2->setPressed(false);
		if (button3)
			button3->setPressed(true);

		CameraSystem::getInstance()->setViewType(CameraSystem::VIEW_FPS);
		break;

	default:
		break;
	}
	// Since there was a tool used, the tool should be put to off after use.
	toolactivated=false;
}

// Stuff in editor only
#ifdef EDITOR

void App::hideEditGui()
{
	GUIManager::getInstance()->setConsoleText(L"Console ready!",SColor(255,0,0,255));
}

std::vector<stringw> App::getAbout()
{
	return LANGManager::getInstance()->getAboutText();
}

//Checkbox events
void App::eventGuiCheckbox(s32 id)
{
	int index = 0;
	vector<bool> enabled;
	IGUICheckBox* vegecheckbox = ((IGUICheckBox*)guienv->getRootGUIElement()->getElementFromId(GUIManager::VEGE_CHECKBOX,true));
	switch (id)
	{
		case GUIManager::CB_ID_POS_X:
			break;

		case GUIManager::CB_ID_POS_Y:
			break;

		case GUIManager::CB_ID_POS_Z:
			break;

		case GUIManager::CB_ID_ROT_X:
			break;

		case GUIManager::CB_ID_ROT_Y:
			break;

		case GUIManager::CB_ID_ROT_Z:
			break;

		case GUIManager::CB_ID_SCA_X:
			break;

		case GUIManager::CB_ID_SCA_Y:
			break;

		case GUIManager::CB_ID_SCA_Z:
			break;

		case GUIManager::VEGE_CHECKBOX:
			printf("checkbox was triggered");
			index = ((IGUIListBox*)guienv->getRootGUIElement()->getElementFromId(GUIManager::VEGE_LISTBOX,true))->getSelected();
			enabled=(TerrainManager::getInstance()->getVegetationEnabled());
			enabled[index]=vegecheckbox->isChecked();
			TerrainManager::getInstance()->setVegetationEnabled(enabled);

			break;

		default:
		break;
	}
}

//Check events coming from combo boxes AND LIST BOXES
void App::eventGuiCombobox(s32 id)
{
	s32 index = 0;
	core::stringw item = L"";
	DynamicObject * object = NULL;

	IGUIListBox* selected=NULL; //Used to get the item in a listbox
	IGUIComboBox* selectedbox = NULL;

	IGUIStaticText* text = NULL;

	std::vector<stringw> list;
	list.clear();

	//Required by the vegetation panel listbox
	IGUIElement* box = guienv->getRootGUIElement()->getElementFromId(GUIManager::VEGE_LISTBOX,true);
	vector<stringw> names = TerrainManager::getInstance()->getVegetationNames();
	vector<stringc> thumb = TerrainManager::getInstance()->getVegetationThumb();
	int total = TerrainManager::getInstance()->getVegetationTypes();
	stringc thumbnail = "../media/vegetation/";

	switch (id)
	{

	case GUIManager::LISTBOX_MA_MAPS:
		index = GUIManager::getInstance()->getListBox(GUIManager::LISTBOX_MA_MAPS)->getSelected();
		text = (IGUIStaticText*)GUIManager::getInstance()->getGUIElement(GUIManager::TXT_MA_DESC);
		if (text)
			text->setText(mapinfos[index].mapdescription.c_str());


		break;
	// Selection in the list from the dynamic object selection
	case GUIManager::CO_ID_DYNAMIC_OBJECT_OBJ_CHOOSER:
		DynamicObjectsManager::getInstance()->setActiveObject(GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_DYNAMIC_OBJECT_OBJ_CHOOSER));
		GUIManager::getInstance()->getInfoAboutModel();
		GUIManager::getInstance()->updateDynamicObjectPreview();
		break;

	case GUIManager::CO_ID_DYNAMIC_OBJECT_OBJLIST_CATEGORY:
		if (currentObject==LIST_OBJ)
		{
			GUIManager::getInstance()->updateCurrentCategory();
			GUIManager::getInstance()->getInfoAboutModel();
		} else if (currentObject==LIST_SEGMENT)
		{
			GUIManager::getInstance()->updateCurrentCategory(GUIManager::LIST_SEGMENT);
			GUIManager::getInstance()->getInfoAboutModel(GUIManager::LIST_SEGMENT);
		} else if (currentObject==LIST_PROPS)
		{
			GUIManager::getInstance()->updateCurrentCategory(GUIManager::LIST_PROP);
			GUIManager::getInstance()->getInfoAboutModel(GUIManager::LIST_PROP);
		}
		else if (currentObject==LIST_LOOT)
		{

			GUIManager::getInstance()->updateCurrentCategory(GUIManager::LIST_LOOT);
			GUIManager::getInstance()->getInfoAboutModel(GUIManager::LIST_LOOT);
		}
		break;

	case GUIManager::CO_ID_DYNAMIC_OBJECT_OBJ_CATEGORY:
		if (currentObject==LIST_OBJ)
		{
			GUIManager::getInstance()->UpdateGUIChooser();
			GUIManager::getInstance()->getInfoAboutModel();
		} else if (currentObject==LIST_SEGMENT)
		{
			GUIManager::getInstance()->UpdateGUIChooser(GUIManager::LIST_SEGMENT);
			GUIManager::getInstance()->getInfoAboutModel(GUIManager::LIST_SEGMENT);
		} else if (currentObject==LIST_PROPS)
		{
			GUIManager::getInstance()->UpdateGUIChooser(GUIManager::LIST_PROP);
			GUIManager::getInstance()->getInfoAboutModel(GUIManager::LIST_PROP);
		} else if (currentObject==LIST_LOOT)
		{
			GUIManager::getInstance()->UpdateGUIChooser(GUIManager::LIST_LOOT);
			GUIManager::getInstance()->getInfoAboutModel(GUIManager::LIST_LOOT);
		}

		DynamicObjectsManager::getInstance()->setActiveObject(GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_DYNAMIC_OBJECT_OBJ_CHOOSER));
		GUIManager::getInstance()->updateDynamicObjectPreview();
		break;

	// Selection in the list from the custom segment selection
	/*case GUIManager::CO_ID_CUSTOM_SEGMENT_OBJ_CHOOSER:
		DynamicObjectsManager::getInstance()->setActiveObject(GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_CUSTOM_SEGMENT_OBJ_CHOOSER));
		GUIManager::getInstance()->getInfoAboutModel(GUIManager::LIST_SEGMENT);
		//GUIManager::getInstance()->updateDynamicObjectPreview();
		break;

	case GUIManager::CO_ID_CUSTOM_TILES_OBJLIST_CATEGORY:
		GUIManager::getInstance()->updateCurrentCategory(GUIManager::LIST_SEGMENT);
		GUIManager::getInstance()->getInfoAboutModel(GUIManager::LIST_SEGMENT);
		break;

	case GUIManager::CO_ID_CUSTOM_SEGMENT_CATEGORY:
		GUIManager::getInstance()->UpdateGUIChooser(GUIManager::LIST_SEGMENT);
		DynamicObjectsManager::getInstance()->setActiveObject(GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_CUSTOM_SEGMENT_OBJ_CHOOSER));
		GUIManager::getInstance()->getInfoAboutModel(GUIManager::LIST_SEGMENT);
		//GUIManager::getInstance()->updateDynamicObjectPreview();
		break;*/

	case GUIManager::CO_ID_ACTIVE_SCENE_LIST:
		index = GUIManager::getInstance()->getListBox(GUIManager::CO_ID_ACTIVE_SCENE_LIST)->getSelected();
		if (selectedNode)
		{
			selectedNode->setDebugDataVisible(0);
			selectedNode=NULL;
		}

		list = DynamicObjectsManager::getInstance()->getObjectsSceneList(current_listfilter);
		if (list.size()>0)
		{
			object = DynamicObjectsManager::getInstance()->getObjectByName(list[index]);
			if (object)
			{
				selectedNode = object->getNode();
				lastMousePick.pickedNode = object->getNode();
			} else
				GUIManager::getInstance()->setConsoleText(core::stringw(L"Failed to retrieve this object: ").append(core::stringw(item)));

			if (selectedNode)
			{
				selectedNode->setDebugDataVisible(true ? EDS_BBOX | EDS_SKELETON : EDS_OFF);
			}
		}
		break;

	case GUIManager::CO_ID_ACTIVE_LIST_FILTER: // User activate a item filter to get a new list of objects to select
		item = GUIManager::getInstance()->getComboBoxItem(GUIManager::CO_ID_ACTIVE_LIST_FILTER);
		current_listfilter = DynamicObject::OBJECT_TYPE_NONE;
		if (item == core::stringc("NPC"))
			current_listfilter = DynamicObject::OBJECT_TYPE_NPC;
		if (item == core::stringc("Props"))
			current_listfilter = DynamicObject::OBJECT_TYPE_NON_INTERACTIVE;
		if (item == core::stringc("Interactive Props"))
			current_listfilter = DynamicObject::OBJECT_TYPE_INTERACTIVE;
		if (item == core::stringc("Walkables"))
			current_listfilter = DynamicObject::OBJECT_TYPE_WALKABLE;
		if (item == core::stringc("Loot"))
			current_listfilter = DynamicObject::OBJECT_TYPE_LOOT;

		GUIManager::getInstance()->buildSceneObjectList(current_listfilter);
		break;

	case GUIManager::CB_SCREENCOMBO:

		toolactivated=true;
		guienv->setFocus(guienv->getRootGUIElement());
		selectedbox = ((IGUIComboBox*)guienv->getRootGUIElement()->getElementFromId(GUIManager::CB_SCREENCOMBO,true));
		if (selectedbox)
		{


			//toolactivated=true; //Tell the system that the button has been activated
			vector3df initpos = CameraSystem::getInstance()->editCamMaya->getAbsolutePosition();
			vector3df inittar = CameraSystem::getInstance()->editCamMaya->getTarget();
			f32 initdist = initpos.getDistanceFrom(inittar);
			u32 value=selectedbox->getItemData(selectedbox->getSelected());
			vector3df newpos = inittar;

			core::matrix4 projMat; //MAtrix projection if changing the perspective

			switch (value)
			{
			case 1: //TOP
				newpos.Y+=initdist;
				newpos.Z-=0.05f;
				CameraSystem::getInstance()->setMAYAPos(newpos);
				CameraSystem::getInstance()->setMAYATarget(inittar);
				break;
			case 2: //Bottom
				newpos.Y-=initdist;
				newpos.Z+=0.05f;
				CameraSystem::getInstance()->setMAYAPos(newpos);
				CameraSystem::getInstance()->setMAYATarget(inittar);
				break;
			case 3: //Left
				newpos.X-=initdist;
				CameraSystem::getInstance()->setMAYAPos(newpos);
				CameraSystem::getInstance()->setMAYATarget(inittar);
				break;
			case 4: // right
				newpos.X+=initdist;
				CameraSystem::getInstance()->setMAYAPos(newpos);
				CameraSystem::getInstance()->setMAYATarget(inittar);
				break;
			case 5: //Front
				newpos.Z-=initdist;
				CameraSystem::getInstance()->setMAYAPos(newpos);
				CameraSystem::getInstance()->setMAYATarget(inittar);
				break;
			case 6: // Back
				newpos.Z+=initdist;
				CameraSystem::getInstance()->setMAYAPos(newpos);
				CameraSystem::getInstance()->setMAYATarget(inittar);
				break;
			case 7: // Orthographic view
				//Will need the change the distance moving since there is no perspective
				projMat.buildProjectionMatrixOrthoLH((f32)device->getVideoDriver()->getScreenSize().Width,(f32)device->getVideoDriver()->getScreenSize().Height,1,15000);
				CameraSystem::getInstance()->getNode()->setProjectionMatrix(projMat,true);
				break;
			case 8: // Back to perspective
				projMat.buildProjectionMatrixPerspectiveFovRH((f32)device->getVideoDriver()->getScreenSize().Width,(f32)device->getVideoDriver()->getScreenSize().Height,1,15000);
				CameraSystem::getInstance()->getNode()->setProjectionMatrix(projMat,false);
				CameraSystem::getInstance()->getNode()->setFOV(0.45f);
				break;

			default:
				break;
			}
		}

		break;

	case GUIManager::CB_SNAPCOMBO: // Get the combo box data to set the snap distance
		guienv->setFocus(guienv->getRootGUIElement());
		toolactivated=true;
		printf("The combo for grid was used\n");
		selectedbox = ((IGUIComboBox*)guienv->getRootGUIElement()->getElementFromId(GUIManager::CB_SNAPCOMBO,true));
		if (selectedbox)
		{

			//toolactivated=true; // Tell the system that the tool as been activated
			currentsnapping=(f32)selectedbox->getItemData(selectedbox->getSelected());
			if (currentsnapping==0) // if 0 is selected, the snapping is back to default
				currentsnapping=64;

			TerrainManager::getInstance()->setEmptyTileGridScale(currentsnapping);
		}

		break;

	case GUIManager::LB_ID_PLAYER_ITEMS:


		selected = ((IGUIListBox*)guienv->getRootGUIElement()->getElementFromId(GUIManager::LB_ID_PLAYER_ITEMS,true));
		if (selected)
		{
			index = selected->getSelected();

			if (index<0)
				break;

			item = selected->getListItem(index);

			vector <DynamicObject*> result=Player::getInstance()->getObject()->getLootItems();
			core::stringc result2=result[index]->getName();

			core::stringc name = result[index]->getThumbnail();


			core::stringc name1="../media/dynamic_objects/";
			name1.append(name);

			printf("Here is the name of the thumbnail! %s: %s\n",result2.c_str(),name1.c_str());

			ITexture* imgresult = driver->getTexture(name1.c_str());
			if(imgresult)
				((IGUIImage*)guienv->getRootGUIElement()->getElementFromId(GUIManager::IMG_LOOT,true))->setImage(imgresult);

			//Put the description in the field
			((IGUIStaticText*)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_LOOT_DESCRIPTION,true))->setText(result[index]->getDescription().c_str());
		}
		else
			((IGUIStaticText*)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_LOOT_DESCRIPTION,true))->setText(L"");
		break;

	case GUIManager::VEGE_LISTBOX:
		if (box)
		{
			int index = ((IGUIListBox*)box)->getSelected();
			if (index>-1 && index<total+1)
			{
				IGUIImage* image = ((IGUIImage*)guienv->getRootGUIElement()->getElementFromId(GUIManager::VEGE_IMAGE,true));
				if (image)
				{
					thumbnail.append(thumb[index]);
					image->setImage(smgr->getVideoDriver()->getTexture(thumbnail.c_str()));
				}

				gui::IGUICheckBox* check = ((IGUICheckBox*)guienv->getRootGUIElement()->getElementFromId(GUIManager::VEGE_CHECKBOX,true));
				check->setChecked((TerrainManager::getInstance()->getVegetationEnabled())[index]);
			}

		} else
		{ printf("Listbox cannot be retrieved!\n");}
		break;


	default:
		break;

	}

	//If a tool was active, put it down once the list was used.
	toolactivated=false;

}

//Check the ENTER events coming from edit boxes
void App::eventGuiEditBox(s32 id)
{
	core::stringc text="";
	core::vector3df newposition=vector3df(0,0,0);
	core::vector3df newrotation=vector3df(0,0,0);
	core::vector3df newscale=vector3df(1,1,1);
	if (selectedNode)
	{
		newposition=selectedNode->getPosition();
		newrotation=selectedNode->getRotation();
		newscale=selectedNode->getScale();
	}

	/*
	switch (id)
	{

	default:
		break;
	}*/

}



#endif

void App::eventGuiSpinbox(s32 id)
{
	f32 value=0.0f;
	core::vector3df newposition=vector3df(0,0,0);
	core::vector3df newrotation=vector3df(0,0,0);
	core::vector3df newscale=vector3df(1,1,1);
	if (selectedNode)
	{
		newposition=selectedNode->getPosition();
		newrotation=selectedNode->getRotation();
		newscale=selectedNode->getScale();
	}

	switch (id)
	{
	case GUIManager::TI_ID_POS_X:
		value=((IGUISpinBox *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TI_ID_POS_X,true))->getValue();
		newposition.X=value;
		if (selectedNode)
		{
			selectedNode->setPosition(newposition);
		}
		break;

	case GUIManager::TI_ID_POS_Y:

		value=((IGUISpinBox *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TI_ID_POS_Y,true))->getValue();
		newposition.Y=value;
		if (selectedNode)
		{
			selectedNode->setPosition(newposition);
		}
		break;

	case GUIManager::TI_ID_POS_Z:
		value=((IGUISpinBox *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TI_ID_POS_Z,true))->getValue();
		newposition.Z=value;
		if (selectedNode)
		{
			selectedNode->setPosition(newposition);
		}
		break;

	case GUIManager::TI_ID_ROT_X:
		value=((IGUISpinBox *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TI_ID_ROT_X,true))->getValue();
		newrotation.X=value;
		if (selectedNode)
		{
			selectedNode->setRotation(newrotation);
		}
		break;

	case GUIManager::TI_ID_ROT_Y:
		value=((IGUISpinBox *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TI_ID_ROT_Y,true))->getValue();
		newrotation.Y=value;
		if (selectedNode)
		{
			selectedNode->setRotation(newrotation);
		}
		break;

	case GUIManager::TI_ID_ROT_Z:
		value=((IGUISpinBox *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TI_ID_ROT_Z,true))->getValue();
		newrotation.Z=value;
		if (selectedNode)
		{
			selectedNode->setRotation(newrotation);
		}
		break;

	case GUIManager::TI_ID_SCA_X:
		value=((IGUISpinBox *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TI_ID_SCA_X,true))->getValue();
		newscale.X=value;
		//in case the user enter strange input (should get 0). 0 as a scale is not good.
		//if (newscale.X==0)
		//	newscale.X=1.0f;

		if (selectedNode)
		{
			selectedNode->setScale(newscale);
		}
		break;

	case GUIManager::TI_ID_SCA_Y:
		value=((IGUISpinBox *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TI_ID_SCA_Y,true))->getValue();
		newscale.Y=value;
		if (selectedNode)
		{
			selectedNode->setScale(newscale);
		}
		break;

	case GUIManager::TI_ID_SCA_Z:
		value=((IGUISpinBox *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TI_ID_SCA_Z,true))->getValue();
		newscale.Z=value;
		if (selectedNode)
		{
			selectedNode->setScale(newscale);
		}
		break;

	default:
		break;
	}

}

void App::setScreenSize(dimension2d<u32> size)
{
	if (device->run())
	{
		GUIManager::getInstance()->updateGuiPositions(size);
		screensize = size;
		CameraSystem::getInstance()->fixRatio(driver);
	}
}

dimension2d<u32> App::getScreenSize()
{
	return screensize;
}

void App::eventKeyPressed(s32 key)
{


	//Don't check the keyboard while editing scripts (done in the editor gui not here)
	if (app_state == APP_EDIT_DYNAMIC_OBJECTS_SCRIPT || app_state == APP_EDIT_PLAYER_SCRIPT || app_state == APP_EDIT_SCRIPT_GLOBAL)
		return;

	if (app_state == APP_GAMEPLAY_NORMAL)
	{
		CameraSystem::getInstance()->eventsKeyboard(key); //forward the event when playing
		Player::getInstance()->getObject()->notifyOnKeypressed();
		return;
	}

	MousePick the = this->getMousePosition3D();

	bool visible=false; //Used to display hide the gameplay toolbar
	switch (key)
	{


	case KEY_KEY_Q:
		if (app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
		{
			toolstate=TOOL_DO_ADD;
			old_do_state = toolstate;
			if (selectedNode)
			{
				selectedNode->setDebugDataVisible(0);
				selectedNode=NULL;
				toolactivated=false;
				moveupdown=false;
			}
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_ADD_MODE,true);
		}
		break;

	case KEY_KEY_W:
		if (app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
		{
			toolstate=TOOL_DO_SEL;
			old_do_state = toolstate;
			toolactivated=false;
			moveupdown=false;
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_SEL_MODE,true);
		}
		break;

	case KEY_KEY_E:
		if (app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
		{
			toolstate=TOOL_DO_MOV;
			old_do_state = toolstate;
			toolactivated=false;
			moveupdown=false;
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_MOV_MODE,true);
		}
		break;

	case KEY_KEY_R:
		if (app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
		{
			toolstate=TOOL_DO_ROT;
			old_do_state = toolstate;
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_ROT_MODE,true);
			toolactivated=false;
			moveupdown=false;
		}
		break;

	case KEY_KEY_T:
		if (app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
		{
			toolstate=TOOL_DO_SCA;
			old_do_state = toolstate;
			GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DO_SCA_MODE,true);
			toolactivated=false;
			moveupdown=false;
		}
		break;

	case KEY_KEY_C:
		{
			if (the.pickedNode && !isKeyPressed(key))
			{
				//Check if there is a element on focus and won't do it if it focused.
				IGUIElement* elem=guienv->getFocus();
				if (elem && elem->getTypeName() != "window")
					break;

				core::vector3df pos = the.pickedPos;
				core::vector3df offset = CameraSystem::getInstance()->getNode()->getPosition();
				core::vector3df calc = pos + (pos - offset);

				CameraSystem::getInstance()->setPosition(calc);
				CameraSystem::getInstance()->getNode()->setTarget(pos);
				// re-center the cursor also
				device->getCursorControl()->setPosition(vector2df(0.5f,0.5f));
			}
		}
		break;
	case KEY_F10: // Clear the test rays
		if (raytester)
			raytester->clearAll();
		break;
	case KEY_F9:
		if (raytester)
			raytester->enable(true);
		break;
	case KEY_F8:
		if (raytester)
			raytester->enable(false);
		break;

	case KEY_F1: //REset the height of object (vegetation or dynamic objects)
		if (app_state<=this->APP_EDIT_TERRAIN_PAINT_VEGETATION && app_state>this->APP_EDIT_LOOK)
			TerrainManager::getInstance()->resetVegetationHeight();
		if (app_state==this->APP_EDIT_DYNAMIC_OBJECTS_MODE)
		{
			if (currentObject==LIST_PROPS)
			{
				DynamicObjectsManager::getInstance()->resetObjectsHeight(DynamicObject::OBJECT_TYPE_NON_INTERACTIVE);
				DynamicObjectsManager::getInstance()->resetObjectsHeight(DynamicObject::OBJECT_TYPE_INTERACTIVE);
			}
			if (currentObject==LIST_OBJ)
				DynamicObjectsManager::getInstance()->resetObjectsHeight(DynamicObject::OBJECT_TYPE_NPC);
		}
		break;

	case KEY_F5:
		if(app_state == APP_EDIT_DYNAMIC_OBJECTS_SCRIPT) //&& !isKeyPressed(key)
			LuaGlobalCaller::getInstance()->doScript(GUIManager::getInstance()->getEditBoxText(GUIManager::EB_ID_DYNAMIC_OBJECT_SCRIPT));

		 //Hide the ocean
		TerrainManager::getInstance()->setOceanVisible(false);
		break;

	case KEY_F6: //Show the ocean

		TerrainManager::getInstance()->setOceanVisible(true);
		break;


	case KEY_RETURN:
		if (app_state == APP_WAIT_DIALOG)
			break;


	case KEY_LCONTROL:

		if (isKeyPressed(KEY_LCONTROL))
			snapfunction=true;
		else
			snapfunction=false;
		break;

	case KEY_ESCAPE:
#ifndef EDITOR
		if (isKeyPressed(KEY_ESCAPE))
		{
			visible=guienv->getRootGUIElement()->getElementFromId(GUIManager::WIN_GAMEPLAY,true)->isVisible();
			guienv->getRootGUIElement()->getElementFromId(GUIManager::WIN_GAMEPLAY,true)->setVisible(!visible);
		}
#endif

		break;

	case KEY_PLUS:
		// Go back;
		CameraSystem::getInstance()->setCameraHeight(-0.5);
		break;

	case KEY_ADD:
		// Go back;
		CameraSystem::getInstance()->setCameraHeight(-0.5);
		break;

	case KEY_MINUS:
		// Go back;
		CameraSystem::getInstance()->setCameraHeight(0.5);
		break;

	case KEY_SUBTRACT:
		// Go back;
		CameraSystem::getInstance()->setCameraHeight(0.5);
		break;


	default:
		break;
	}
}

bool App::isKeyPressed(int key)
{
	return EventReceiver::getInstance()->isKeyPressed(key);
}

bool App::isMousePressed(int mb)
{
	return EventReceiver::getInstance()->isMousePressed(mb);
}

// Behaviors with the detected mouse button
// This is called from mouse events
// (08/02/13) This will need to be split in methods as the code is getting too big here.
void App::eventMousePressed(s32 mouse)
{
	IGUIElement* elem = NULL;
	if (app_state == APP_GAMEPLAY_NORMAL)
	{
		if (cursorIsInEditArea())
			CameraSystem::getInstance()->eventsMouseKey(mouse); //forward the event when playing
		return;
	}

	s32 id = 0;
	bool item1 = false;
	bool item2 = false;

	switch(mouse)
	{///TODO: colocar acoes mais comuns acima e menos comuns nos elses

	case EMIE_LMOUSE_LEFT_UP:
		{
			if (toolstate ==TOOL_DO_ADD || toolstate==TOOL_DO_MOV || toolstate==TOOL_DO_ROT || toolstate==TOOL_DO_SCA)
			{
				//Deactivate the tool if the mouse buttons are released
				if (toolactivated)
					toolactivated=false;

			}
		}
		break;

	case EMIE_RMOUSE_LEFT_UP:
		{
			if (toolstate==TOOL_DO_MOV || toolstate==TOOL_DO_ROT || toolstate==TOOL_DO_SCA)
			{
				//Deactivate the tool if the mouse buttons are released
				toolactivated=false;
				moveupdown=false;



			}
		}
		break;

	case EMIE_LMOUSE_PRESSED_DOWN://Left button (default)

		//Check if the combo box are in focus and prevent the mouse click to add anything
		//The GUI that send the problem is the LISTBOX when it is clicked. It send an event.
		//elem = device->getGUIEnvironment()->getFocus();
		elem = NULL;
		if (elem && app_state != APP_EDIT_PRJ)
		{
			id = elem->getID();


			if (elem->getParent()->getID()==GUIManager::CB_SCREENCOMBO)
				item1=true;

			if (elem->getParent()->getID()==GUIManager::CB_SNAPCOMBO)
				item2=true;
		}


		if( cursorIsInEditArea() && !item1 && !item2)
		{

			if(app_state == APP_EDIT_TERRAIN_SEGMENTS)
			{
				TerrainManager::getInstance()->createSegment(this->getMousePosition3D().pickedPos / TerrainManager::getInstance()->getScale());
				return;
			}
			else if(app_state == APP_EDIT_TERRAIN_EMPTY_SEGMENTS)
			{
				TerrainManager::getInstance()->createSegment(this->getMousePosition3D().pickedPos / TerrainManager::getInstance()->getScale(), true);
				return;
			}
			else if(app_state == APP_EDIT_TERRAIN_CUSTOM_SEGMENTS)
			{
				if (toolstate==TOOL_NONE)
				{
					core::stringc meshfile=DynamicObjectsManager::getInstance()->getActiveObject()->meshFile;
					ISceneNode * nod = TerrainManager::getInstance()->createCustomSegment(this->getMousePosition3D().pickedPos / TerrainManager::getInstance()->getScale(),meshfile);

					if (DynamicObjectsManager::getInstance()->getActiveObject()->materials.size()>0 && nod)
					{
						ShaderCallBack::getInstance()->setMaterials(nod,DynamicObjectsManager::getInstance()->getActiveObject()->materials);
					}
					return;
				}
				if (toolstate==TOOL_TILEROTATE_LEFT)
				{
					TerrainManager::getInstance()->rotateLeft(this->getMousePosition3D().pickedPos / TerrainManager::getInstance()->getScale());
					return;
				}
				if (toolstate==TOOL_TILEROTATE_RIGHT)
				{
					TerrainManager::getInstance()->rotateRight(this->getMousePosition3D().pickedPos / TerrainManager::getInstance()->getScale());
					return;
				}
			}
			else if(app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
			{

				if (toolactivated && toolstate==TOOL_DO_ADD) //Return if the tool is already activated
				{
					return;
				}

				if (toolstate==TOOL_DO_ADD)
					addItemToScene();

				if (toolstate==TOOL_DO_SEL || toolstate==TOOL_DO_MOV || toolstate==TOOL_DO_ROT || toolstate==TOOL_DO_SCA) // Enable selection for theses modes
					selectItem();

			}
			else if(app_state == APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE)
			{
				setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
				return;
			}
		}
		break;

	// Right button (Action the same as the left button)
	case EMIE_RMOUSE_PRESSED_DOWN:
		if( cursorIsInEditArea())
		{
			if(app_state == APP_EDIT_TERRAIN_SEGMENTS)
			{
				TerrainManager::getInstance()->removeSegment(this->getMousePosition3D().pickedPos / TerrainManager::getInstance()->getScale());
				return;
			}
			else if(app_state == APP_EDIT_TERRAIN_CUSTOM_SEGMENTS)
			{
				if (toolstate==TOOL_NONE)
					TerrainManager::getInstance()->removeSegment(this->getMousePosition3D().pickedPos / TerrainManager::getInstance()->getScale(), true);
				return;
			}
			else if(app_state == APP_EDIT_TERRAIN_EMPTY_SEGMENTS)
			{
				TerrainManager::getInstance()->removeEmptySegment(this->getMousePosition3D().pickedPos / TerrainManager::getInstance()->getScale());
				return;
			}
			else if(app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
			{
				MousePick mousePick = getMousePosition3D();

				lastMousePick = mousePick;
				stringc nodeName = "";
				// Check for a node to prevent a crash (need to get the name of the node)


				if (mousePick.pickedNode != NULL && (toolstate==TOOL_DO_ADD || toolstate==TOOL_DO_SEL)) // Add mode right button functionnality
				{
					nodeName = mousePick.pickedNode->getName();
					lastMousePick = mousePick;

					//if you click on a Dynamic Object then open his properties
					if( stringc( nodeName.subString(0,14)) == "dynamic_object" || nodeName.subString(0,16) == "dynamic_walkable" )
					{
						cout << "PROP:" << nodeName.c_str() << endl;

						// Toggle the context menu (main)
						GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,
							!GUIManager::getInstance()->isGUIVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU));
					} else
					{
						// Toggle the context menu (alternate)
						GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU1, true);
					}
					return;
				}

				if ((toolstate==TOOL_DO_MOV || toolstate==TOOL_DO_ROT || toolstate==TOOL_DO_SCA))
				{

					// Since we don't have multi section implemented as of now, the previous node should be NULL
					if (selectedNode) // There was a node selected before
					{
						selectedNode->setDebugDataVisible(0); // Unselect it
					}
					selectedNode=NULL;

					// Create the selected node if there was a node picked
					if (mousePick.pickedNode)
					{
						nodeName = mousePick.pickedNode->getName();

						// Need to filter some nodes names as "terrain" or other objects might be selected.
						if( stringc( nodeName.subString(0,14)) == "dynamic_object" || nodeName.subString(0,16) == "dynamic_walkable" )
						{
							selectedNode=mousePick.pickedNode;
							selectedNode->setDebugDataVisible(true ? EDS_BBOX | EDS_SKELETON : EDS_OFF);
							GUIManager::getInstance()->updateNodeInfos(selectedNode); //Put infos

						}
						else //Invalid node for selection
						{
							selectedNode=NULL;
							GUIManager::getInstance()->updateNodeInfos(selectedNode); //Put 0 in the node infos
							// Toggle the context menu (alternate)
							GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU1,true);
						}
					} else //Clicked outside on nothing
					{
						selectedNode=NULL;
						GUIManager::getInstance()->updateNodeInfos(selectedNode); //Put 0 in the node infos
					}

					if (selectedNode)
					{
						initialposition=selectedNode->getPosition();
						mousepos=device->getCursorControl()->getPosition();
						toolactivated=true;
						moveupdown=true;
					}
					return;
				}


			}
			else if(app_state == APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE)
			{
				// Move the object up/down using the mouse Y axis
				// Pressing back the button release the mode
				mousepos=device->getCursorControl()->getPosition();
				moveupdown=!moveupdown;
				if (!moveupdown)
					setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
			}
		}
		break;


	default:
		break;
	}
}

void App::eventMouseWheel(f32 value)
{

	// In a game
	if (app_state>APP_STATE_CONTROL)
	{
		CameraSystem::getInstance()->eventMouseWheel(value);
		return;
	}

	if(app_state == APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE)
	{
		vector3df oldRot = lastMousePick.pickedNode->getRotation();
		lastMousePick.pickedNode->setRotation(vector3df(0,value*5,0)+oldRot);
	}

	if(app_state == APP_EDIT_CHARACTER)
	{
		vector3df oldRot = Player::getInstance()->getObject()->getRotation();
		Player::getInstance()->getObject()->setRotation(vector3df(0,value*5,0)+oldRot);
	}
	// This will allow zoom in/out in editor mode
	if	(app_state != APP_EDIT_CHARACTER &&
		app_state != APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE &&
		app_state != APP_EDIT_ABOUT &&
		app_state != APP_EDIT_DYNAMIC_OBJECTS_SCRIPT &&
		app_state != APP_EDIT_SCRIPT_GLOBAL &&
		app_state != APP_EDIT_PLAYER_SCRIPT &&
		app_state != APP_EDIT_WAIT_GUI &&
		app_state != APP_WAIT_FILEREQUEST &&
		cursorIsInEditArea())
	{
		if (app_state < APP_STATE_CONTROL)
		{
			// not in viewdrag mode then enable the cam, then set the camera height then disable it again
			if (app_state != APP_EDIT_VIEWDRAG)
			{
				// Go directly
				CameraSystem::getInstance()->setCameraHeight(-value);
			}
			else// in viewdrag mode
				CameraSystem::getInstance()->setCameraHeight(-value);

		}

	}
}

void App::eventMessagebox(gui::EGUI_EVENT_TYPE type)
{

	// When editing terrain segment, on "yes" on the message box will
	// Delete the "tagged" terrain segment
	if (type==EGET_MESSAGEBOX_YES && app_state==APP_EDIT_TERRAIN_SEGMENTS)
		TerrainManager::getInstance()->deleteTaggedSegment();

	//When the user confirm that he want to clear the current map
	if (type == EGET_MESSAGEBOX_YES && askedClearMap)
	{
		askedClearMap = false;
		createNewMap();
	}




}

// This will display the "packsack" wiith the inventory of the player during gameplay
void App::openItemsPanel()
{
	setAppState(APP_GAMEPLAY_VIEW_ITEMS);
	DynamicObjectsManager::getInstance()->freezeAll();
	GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_GAMEPLAY_ITEMS,true);
	GUIManager::getInstance()->drawPlayerStats();
}

App* App::getInstance()
{
	static App *instance = 0;
	if (!instance) instance = new App();
	return instance;
}
//! Get the 3D mouse coordinate on the ground or object (ray test)
App::MousePick App::getMousePosition3D(int id)
{

	// Initialize the data
	MousePick result;
	result.pickedNode=NULL;
	result.pickedPos=vector3df(0,0,0);

	core::vector3df intersection=vector3df(0,0,0);
	core::triangle3df hitTriangle=triangle3df(vector3df(0,0,0),vector3df(0,0,0),vector3df(0,0,0));

	ISceneNode* tempNode=NULL;

	// Get the cursor 2D coordinates
	position2d<s32> pos=device->getCursorControl()->getPosition();

	// For the ray test, we should hide the player (And the decors element that we don't want to select)
	Player::getInstance()->getObject()->getNode()->setVisible(false);
	if (app_state== APP_GAMEPLAY_NORMAL)
		DynamicObjectsManager::getInstance()->setObjectsVisible(DynamicObject::OBJECT_TYPE_NON_INTERACTIVE, false);

	line3df ray = smgr->getSceneCollisionManager()->getRayFromScreenCoordinates(pos, smgr->getActiveCamera());
	tempNode = smgr->getSceneCollisionManager()->getSceneNodeAndCollisionPointFromRay(ray,
		intersection,
		hitTriangle,
		id);

	// Show back the player once the ray test is done
	Player::getInstance()->getObject()->getNode()->setVisible(true);

	if (app_state == APP_GAMEPLAY_NORMAL)
		DynamicObjectsManager::getInstance()->setObjectsVisible(DynamicObject::OBJECT_TYPE_NON_INTERACTIVE, true);


	if(tempNode!=NULL)
	{
		// Ray test passed, returning the results
		result.pickedPos = intersection;
		result.pickedNode = tempNode;
		raytester->addRay(ray,true);
		return result;

	}
	else
	{
		// Failed the ray test, returning the previous results
		if (intersection!=vector3df(0,0,0))
			result.pickedPos = intersection;
		else
			result.pickedPos = lastMousePick.pickedPos;

		result.pickedNode = NULL;

		// Send the ray to the raytester (drawing lines to see the failed ray)
		raytester->addRay(ray,false);

		//printf ("Failed the screen ray test! Picking old values., ray len is: %f \n",len);
		return result;
	}
}

void App::addItemToScene()
{
	MousePick mousePick = getMousePosition3D();

	lastMousePick = mousePick;
	core::stringc nodeName = "";

	if (selectedNode) //Unselect and remove the selected node in mode changes
	{
		selectedNode->setDebugDataVisible(0); //Remove selection
	}
	selectedNode=NULL;

	// To add an object, the picked node need to return at least a tile.
	// If pickednode is empty then the user clicked outside the area
	if (mousePick.pickedNode)
	{
		nodeName = mousePick.pickedNode->getName();
		//if you click on a Dynamic Object then open the context menu, and select the object
		if( stringc( nodeName.subString(0,14)) == "dynamic_object" || nodeName.subString(0,16) == "dynamic_walkable" )
		{
			selectedNode=mousePick.pickedNode;
#ifdef DEBUG
				cout << "PROP:" << nodeName.c_str() << endl;
#endif
			// Toggle the context menu
			GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,
				!GUIManager::getInstance()->isGUIVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU));

			toolactivated=true;
		}
		else//create a new copy of active dynamic object at the clicked position
		{
			// If the context menu is still open close it since we want to create a object
			if (GUIManager::getInstance()->isGUIVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU))
				GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU,false);

			DynamicObject* tmpDObj = DynamicObjectsManager::getInstance()->createActiveObjectAt(mousePick.pickedPos);

			GUIManager::getInstance()->buildSceneObjectList(current_listfilter); //Update the scene list
			toolactivated=false;
#ifdef DEBUG
			cout << "DEBUG : DYNAMIC_OBJECTS : NEW " << tmpDObj->getName().c_str() << " CREATED!"  << endl;
#endif

		}
	}
}

void App::selectItem()
{
	MousePick mousePick = getMousePosition3D();

	lastMousePick = mousePick;
	core::stringc nodeName = "";

	// Since we don't have multi section implemented as of now, the previous node should be NULL
	if (selectedNode) // There was a node selected before
	{
		selectedNode->setDebugDataVisible(0); // Unselect it
	}
	selectedNode=NULL;

	// Create the selected node if there was a node picked
	if (mousePick.pickedNode)
	{
		nodeName = mousePick.pickedNode->getName();

		// Need to filter some nodes names as "terrain" or other objects might be selected.
		if( stringc( nodeName.subString(0,14)) == "dynamic_object" || nodeName.subString(0,16) == "dynamic_walkable" )
		{
			selectedNode=mousePick.pickedNode;
			selectedNode->setDebugDataVisible(true ? EDS_BBOX | EDS_SKELETON : EDS_OFF);
			GUIManager::getInstance()->updateNodeInfos(selectedNode); //Put infos
		}
		else //Invalid node for selection
		{
			selectedNode=NULL;
			GUIManager::getInstance()->updateNodeInfos(selectedNode); //Put 0 in the node infos
			// Toggle the context menu (alternate)
		}
	} else //Clicked outside on nothing
	{
		selectedNode=NULL;
		GUIManager::getInstance()->updateNodeInfos(selectedNode); //Put 0 in the node infos
		// Toggle the context menu (alternate)
		// GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU1,true);

	}
	if (selectedNode)
	{
		initialposition=selectedNode->getPosition();
		initialrotation=selectedNode->getRotation();
		initialscale=selectedNode->getScale();
		mousepos=device->getCursorControl()->getPosition();
		toolactivated=!toolactivated; //Toggle the state of the tool
	}

}

core::stringw App::renameMap(core::stringw source,core::stringw dest, bool file)
{
	if (file)
	{
		//Return the full path of the map
		core::stringw text = L"";
		text = editorfunc->getProjectsPath() + "/";
		text += currentProjectName;
		text += "/";
		text += source;
		text += "/";
		text += source;
		text += ".map";

		core::stringw text2 = L"";
		text2 = editorfunc->getProjectsPath() + "/";
		text2 += currentProjectName;
		text2 += "/";
		text2 += source;
		text2 += "/";
		text2 += dest;
		text2 += ".map";

		printf(">>>>> Here is the result of the query: %s\n", core::stringc(text).c_str());
		printf(">>>>> Here is the result of the query: %s\n", core::stringc(text2).c_str());
		rename(core::stringc(text).c_str(), core::stringc(text2).c_str());
		return text;
	}
	else
	{
		//Return the full path of the map
		core::stringw text = L"";
		text = editorfunc->getProjectsPath() + "/";
		text += currentProjectName;
		text += "/";
		text += source;


		core::stringw text2 = L"";
		text2 = editorfunc->getProjectsPath() + "/";
		text2 += currentProjectName;
		text2 += "/";
		text2 += dest;


		printf(">>>>> Here is the result of the query: %s\n", core::stringc(text).c_str());
		printf(">>>>> Here is the result of the query: %s\n", core::stringc(text2).c_str());
		rename(core::stringc(text).c_str(), core::stringc(text2).c_str());
		return text;

	}
}

//Will be used to delete a map. Not implemented yet. (july 2015)
core::stringw deleteMap(core::stringw mapname)
{
    #ifdef win32
	std::tr2::sys::remove_all("allo");
	#endif
	return L"";

}

//! Display a "debug" box over a selected node
void App::setPreviewSelection()
{
	// Will get a toggle selection
	MousePick mousePick = getMousePosition3D();
	stringc nodeName = "";

	// Check for a node to prevent a crash (need to get the name of the node)
	if (mousePick.pickedNode != NULL)
	{
		if(app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE)
		{
			nodeName = mousePick.pickedNode->getName();
			//If the mouse hover the object it will be toggled in debug data (bounding box, etc)
			//Should be able to select the walkable in editor mode
			if( stringc( nodeName.subString(0,14)) == "dynamic_object" || nodeName.subString(0,16) == "dynamic_walkable" )
			{
				selectedNode=mousePick.pickedNode;
				if (nodeName!=lastPickedNodeName && lastScannedPick.pickedNode!=NULL)
					lastScannedPick.pickedNode->setDebugDataVisible(0);

				lastScannedPick = mousePick;
				lastPickedNodeName=nodeName;
				if (mousePick.pickedNode!=NULL)
					mousePick.pickedNode->setDebugDataVisible(true ? EDS_BBOX | EDS_SKELETON : EDS_OFF);
			}
			//if the mouse is not over the object anymore then it will "deselect" it
			else if (lastScannedPick.pickedNode!=NULL)
			{
				lastScannedPick.pickedNode->setDebugDataVisible(0);
				selectedNode=NULL;
			}
		}
	}
}

bool App::loadConfig()
{

	screensize.Height = 768;
	screensize.Width = 1024;

	fullScreen = false;
	resizable = false;
	vsync = false;
	antialias = false;
	silouette = false;

	language = "en-us";
	TerrainManager::getInstance()->setTileMeshName("../media/baseTerrain.obj");
	TerrainManager::getInstance()->setTerrainTexture(0,"../media/L0.jpg");
	TerrainManager::getInstance()->setTerrainTexture(1,"../media/L1.jpg");
	TerrainManager::getInstance()->setTerrainTexture(2,"../media/L2.jpg");
	TerrainManager::getInstance()->setTerrainTexture(3,"../media/L3.jpg");
	TerrainManager::getInstance()->setTerrainTexture(4,"../media/L4.jpg");
	// Define a default mapname only for the player application
#ifndef EDITOR
	mapname="";
#endif

#ifdef EDITOR
	// File to load if it's the editor
	core::stringc path=editorfunc->getAppDataPath().c_str();
	path.append("/config.xml");

	bool result;
	core::stringc pathfinal="";

	result = editorfunc->checkPath(path.c_str());
	printf(">>> Get the configuration at this place: %s\n", path.c_str());
	if (result)
	{
		printf(">>>Configuration file found at the proper path!!!\n");
		pathfinal = path;
	}
	else
	{
		printf(">>>Failed to get the configuration!!!\n");
		editorfunc->copyConfiguration(); //Copy the config from the application path (distribution) to the appdata path.
		pathfinal = "config.xml";
	}

	TiXmlDocument doc(pathfinal.c_str());

#else
	// File to load if it's the player build
	TiXmlDocument doc("config.xml");
#endif

	if (!doc.LoadFile()) return false; ///TODO: create the config default file if does not exist

#ifdef DEBUG
	cout << "DEBUG : XML : LOADING CONFIGURATION : " << endl;
#endif

	TiXmlElement* root = doc.FirstChildElement( "IrrRPG_Builder_Config" );

	if ( root )
	{
		if (atof(root->Attribute("version")) * 100 != APP_VERSION )
		{
#ifdef DEBUG
			cout << "DEBUG : XML : INCORRECT VERSION!" << endl;
#endif

			//return false;
		}

		TiXmlElement* resXML = root->FirstChildElement( "screen" );
		if ( resXML )
		{
			screensize.Width = atoi(resXML->ToElement()->Attribute("screen_width"));
			screensize.Height = atoi(resXML->ToElement()->Attribute("screen_height"));
			stringc result = resXML->ToElement()->Attribute("camera");
			if (result.size()>0)
			{
				if (result == std::string("rts").c_str())
				{
					defaultview=VIEW_RTS;
				}
				if (result == std::string("rpg").c_str())
				{
					defaultview=VIEW_RPG;
				}
				if (result == std::string("fps").c_str())
				{
					defaultview=VIEW_FPS;
				}
			} else
				defaultview=VIEW_RTS;




			stringc full = resXML->ToElement()->Attribute("fullscreen");
			if (full=="true")
			{
				fullScreen=true;
			}
			stringc resize = resXML->ToElement()->Attribute("resizeable");
			if (resize=="true")
				resizable=true;

			stringc vsyncresult = resXML->ToElement()->Attribute("vsync");
			if (vsyncresult=="true")
				vsync=true;

			stringc antialiasresult = resXML->ToElement()->Attribute("antialias");
			if (antialiasresult=="true")
				antialias=true;

			stringc silouetteresult = resXML->ToElement()->Attribute("silouette");
			if (silouetteresult=="true")
				silouette=true;

			stringc xeffect = resXML->ToElement()->Attribute("xeffects");
			if (xeffect=="true")
			{
				xeffectenabler=true;
			}

			if (resizable && fullScreen)
			{
				IrrlichtDevice * tempdevice = createDevice(EDT_NULL,dimension2d<u32>(640,480), 16, false, false, false, 0);
				screensize = tempdevice->getVideoModeList()->getDesktopResolution();
				tempdevice->closeDevice();
				tempdevice->drop();
			}

		}
		//Language
		TiXmlElement* langXML = root->FirstChildElement( "language" );
		if ( langXML )
		{
			language=stringc(langXML->ToElement()->Attribute("type")).c_str();

		}
		TiXmlElement* groundXML = root->FirstChildElement( "terrain" );
		if ( groundXML )
		{
			stringc result=groundXML->ToElement()->Attribute("density");
			u32 den = atoi(result.c_str());
			if (den>9 && den<251)
				terraindensity=den;
			else
				terraindensity=100;

			stringc meshname = groundXML->ToElement()->Attribute("mesh");
			TerrainManager::getInstance()->setTileMeshName(meshname);
			stringc layer0 = groundXML->ToElement()->Attribute("layer0");
			stringc layer1 = groundXML->ToElement()->Attribute("layer1");
			stringc layer2 = groundXML->ToElement()->Attribute("layer2");
			stringc layer3 = groundXML->ToElement()->Attribute("layer3");
			stringc layer4 = groundXML->ToElement()->Attribute("layer4");
			stringc model = groundXML->ToElement()->Attribute("model");
			f32 scale = (f32)atof(groundXML->ToElement()->Attribute("scale"));
			TerrainManager::getInstance()->setTerrainTexture(0,layer0);
			TerrainManager::getInstance()->setTerrainTexture(1,layer1);
			TerrainManager::getInstance()->setTerrainTexture(2,layer2);
			TerrainManager::getInstance()->setTerrainTexture(3,layer3);
			TerrainManager::getInstance()->setTerrainTexture(4,layer4);
			TerrainManager::getInstance()->setScale(scale);
			TerrainManager::getInstance()->setEmptyTileGridScale(currentsnapping);
			if (model.size()>0)
				tileformat=model;

		}
		TiXmlElement* waterXML = root->FirstChildElement( "ocean" );
		if ( waterXML )
		{
			stringc meshname = waterXML->ToElement()->Attribute("mesh");
			stringc normalmap = waterXML->ToElement()->Attribute("normalmap");
			stringc reflection = waterXML->ToElement()->Attribute("reflection");
			///TODO: we are just loading ocean seetings, we need to set it!
		}
		// Player app. Load the default map from "gameconfig.xml"

		TiXmlElement* mapXML = root->FirstChildElement( "map" );
		if ( mapXML )
		{
			mapname = mapXML->ToElement()->Attribute("name");
#ifdef DEBUG
			printf("The map name is: %s\n",mapname.c_str());
#endif
			///TODO: we are just loading ocean seetings, we need to set it!

			stringc gamelogo = mapXML->ToElement()->Attribute("logo");
			if (gamelogo.size()>0)
			{
				logoimage=gamelogo;
			}
		}

	}
	else
	{
#ifdef DEBUG
		cout << "DEBUG : XML : THIS FILE IS NOT A IRRRPG BUILDER PROJECT!" << endl;
#endif

		return false;
	}

#ifdef DEBUG
	cout << "DEBUG : XML : PROJECT LOADED! "<< endl;
#endif

	///TODO:CLEAR PROJECT IF NOT RETURN TRUE ON LOAD PROJECT FROM XML

	return true;
}

void App::setupDevice(IrrlichtDevice* IRRdevice)
{

	loadConfig();
	irr::SIrrlichtCreationParameters deviceConfig;

	if (!IRRdevice)
	{
		if (antialias) // Set 4x antialias mode if supported
			deviceConfig.AntiAlias = 4;
		else
			deviceConfig.AntiAlias = 0;

		deviceConfig.Bits = 32;
		deviceConfig.DriverType = EDT_OPENGL;
		deviceConfig.Fullscreen = fullScreen;
		deviceConfig.Vsync = vsync;
		deviceConfig.WindowSize = screensize;
		deviceConfig.ZBufferBits = 16;

		device = createDeviceEx(deviceConfig);

		this->device->setResizable(resizable);

		device->setWindowCaption(appname.c_str());
	} else
		device = IRRdevice;

	driver = device->getVideoDriver();
	smgr = device->getSceneManager();
	guienv = device->getGUIEnvironment();

	#ifndef win32
	//device->maximizeWindow();
	#endif

	device->setEventReceiver(EventReceiver::getInstance());
	timer = device->getTimer()->getRealTime();
	timer2 = device->getTimer()->getRealTime();
	timer3 = device->getTimer()->getRealTime();
	LANGManager::getInstance()->setDefaultLanguage(language);

	path = device->getFileSystem()->getWorkingDirectory();
	projectpath = "../projects";
	quickUpdate();


}

IrrlichtDevice* App::getDevice()
{
	if(!device)
	{
#ifdef DEBUG
		printf("ERROR: Device is NULL, please call SetupDevice first!");
#endif
		exit(0);
	}
	return device;
}


void App::playGame()
{

	if (app_state<APP_STATE_CONTROL)
	{
#ifndef EDITOR
	bool visible=false;
	visible=guienv->getRootGUIElement()->getElementFromId(GUIManager::WIN_LOADER,true)->isVisible();
	if (visible)
		guienv->getRootGUIElement()->getElementFromId(GUIManager::WIN_LOADER,true)->setVisible(false);

	visible=guienv->getRootGUIElement()->getElementFromId(GUIManager::WIN_GAMEPLAY,true)->isVisible();
	if (visible)
		guienv->getRootGUIElement()->getElementFromId(GUIManager::WIN_GAMEPLAY,true)->setVisible(false);
#endif
		//EffectsManager::getInstance()->updateSkydome();
		TerrainManager::getInstance()->setEmptyTileVisible(false);
		//oldcampos = Player::getInstance()->getObject()->getPosition();
		oldcampos = CameraSystem::getInstance()->editCamMaya->getPosition();
		oldcamtar = CameraSystem::getInstance()->editCamMaya->getTarget();
		LuaGlobalCaller::getInstance()->storeGlobalParams();

		CameraSystem::getInstance()->setCamera(CameraSystem::CAMERA_GAME);
		// setback the fog as before (will need to check with LUA)
		driver->setFog(SColor(0,220,220,255),EFT_FOG_LINEAR,300,5000);
		smgr->getActiveCamera()->setFarValue(7500.0f);

		old_state = app_state;
		this->setAppState(APP_GAMEPLAY_NORMAL);
		DynamicObjectsManager::getInstance()->showDebugData(false); //DynamicObjectsManager::getInstance()->showDebugData(false);

		// Execute the scripts in the dynamic objects
		DynamicObjectsManager::getInstance()->initializeAllScripts();

		//LuaGlobalCaller::getInstance()->useGlobalFunction("onLoad");

		// Need to evaluate if it's needed to have displaying debug data for objects (could be done with selection instead)
		//DynamicObjectsManager::getInstance()->showDebugData(false);
		//TerrainManager::getInstance()->showDebugData(false);

		// Reset the last "walk target" as the game restart.
		Player::getInstance()->getObject()->setWalkTarget(Player::getInstance()->getObject()->getPosition());
		DynamicObjectsManager::getInstance()->resetObjectsWalkTarget(DynamicObject::OBJECT_TYPE_NPC);


		GUIManager::getInstance()->setElementVisible(GUIManager::ST_ID_PLAYER_LIFE,true);
		//LuaGlobalCaller::getInstance()->doScript(scriptGlobal);

	}
}

void App::stopGame()
{
	if (app_state>APP_STATE_CONTROL)
	{
		EffectsManager::getInstance()->SetPostFX("none"); //Revert post fx to none.
		EffectsManager::getInstance()->turnOffSkydome();
		DynamicObjectsManager::getInstance()->objectsToIdle();
		LuaGlobalCaller::getInstance()->restoreGlobalParams();
		GlobalMap::getInstance()->clearGlobals();


		TerrainManager::getInstance()->setEmptyTileVisible(true);

		DynamicObjectsManager::getInstance()->clearAllScripts(); //Clear all the scripts but also reposition the Objects at the proper position (pre-game)
		//DynamicObjectsManager::getInstance()->displayShadow(false);
		// Need to evaluate if it's needed to have displaying debug data for objects (could be done with selection instead)
		// DynamicObjectsManager::getInstance()->showDebugData(true);
		// TerrainManager::getInstance()->showDebugData(true);
		DynamicObjectsManager::getInstance()->getTarget()->getNode()->setVisible(false);


		SoundManager::getInstance()->stopSounds();
		GUIManager::getInstance()->setElementVisible(GUIManager::ST_ID_PLAYER_LIFE,false);

		this->setAppState(old_state); //APP_EDIT_LOOK


		CameraSystem::getInstance()->editCamMaya->setUpVector(vector3df(0,1,0));
		CameraSystem::getInstance()->setCamera(CameraSystem::CAMERA_EDIT);
		CameraSystem::getInstance()->editCamMaya->setPosition(vector3df(0.0f,1000.0f,-1000.0f));
		CameraSystem::getInstance()->editCamMaya->setTarget(vector3df(0.0f,0.0f,0.0f));
		CameraSystem::getInstance()->editCamMaya->setFarValue(90000.0f);
		//CameraSystem::getInstance()->setPosition(vector3df(oldcampos));

		driver->setFog(SColor(0,255,255,255),EFT_FOG_LINEAR,300,999100);
		DynamicObjectsManager::getInstance()->objectsToIdle();

		//Show back cursor in editor if it was hidden by other controls
		if(!device->getCursorControl()->isVisible())
			device->getCursorControl()->setVisible(true);

		//REmove the objects that were generated ingame.
		DynamicObjectsManager::getInstance()->removeGenerated();


	}
}

void App::update()
{

	// Attempt to do automatic rezise detection
	if (screensize != driver->getScreenSize())
		this->setScreenSize(driver->getScreenSize());

	//This could be put outside in the app state changes.
	if (app_state<APP_STATE_CONTROL)
	{
#ifdef EDITOR
		background=SColor(0,160,160,160);// Background color in editor
		EffectsManager::getInstance()->setClearColor(SColor(0,160,160,160));
#else
		//Player application back color when loading the game (Default is black)
		background=SColor(0,0,0,0);// Background color in editor
		EffectsManager::getInstance()->setClearColor(SColor(0,0,0,0));
#endif
	}
	else
	{
		//EffectsManager::getInstance()->setClearColor(ingamebackground);
		//background=ingamebackground; // Background color ingame
	}


	driver->beginScene(true, true, background);

	// Terrain transform mode MUSt use all the CPU/Refresh it can get for performance
	if(app_state < APP_STATE_CONTROL)
	{

		if (app_state!=APP_EDIT_TERRAIN_TRANSFORM)
			device->yield();

#ifdef EDITOR
		updateEditMode();//editMode
#endif
	}
	else
	{
		// Do not update the gameplay if we "paused" the game for a reason
		if(app_state < APP_GAMEPLAY_VIEW_ITEMS)
		{
			updateGameplay();

			// This will calculate the animation blending for the nodes
			DynamicObjectsManager::getInstance()->updateAnimationBlend();
		}
	}


	// Check for events of the logger
	GUIManager::getInstance()->setConsoleLogger(textevent);

	//Update the camera
	if (app_state==APP_GAMEPLAY_NORMAL)
		CameraSystem::getInstance()->updateGameCamera();

	// Prepare the post FX before rendering all
	EffectsManager::getInstance()->preparePostFX(false);

#ifdef EDITOR
	//Will redraw the scene normally unless we use XEffect.
	if (EffectsManager::getInstance()->isXEffectsEnabled() && (app_state > APP_STATE_CONTROL))
		EffectsManager::getInstance()->update();
	else
		smgr->drawAll();
#else
	if (app_state>APP_STATE_CONTROL)
	{
		//Will redraw the scene normally unless we use XEffect.
	if (EffectsManager::getInstance()->isXEffectsEnabled() && (app_state > APP_STATE_CONTROL))
		EffectsManager::getInstance()->update();
	else
		smgr->drawAll();

	}
#endif
	if (raytester)
		raytester->update();

	// PostFX - render the player in silouette if he's occluded
	// Work with the current model but the code should be improved to support more models (with more than one texture)

	
	if (silouette) //  && (app_state > APP_STATE_CONTROL))
	{
		driver->runAllOcclusionQueries(false);
		driver->updateAllOcclusionQueries();
		overdraw=driver->getOcclusionQueryResult(Player::getInstance()->getNode())>0;
		overdraw=!overdraw;
		if (overdraw)
		{
			// Draw the player over the rendering so it's not occluded by the scenery
			Player::getInstance()->getNode()->setMaterialTexture(0, tex_occluded);
			Player::getInstance()->getNode()->setMaterialFlag(EMF_ZBUFFER,false);
			Player::getInstance()->getNode()->setMaterialFlag(EMF_LIGHTING,false);
			Player::getInstance()->getNode()->render();
			Player::getInstance()->getNode()->setMaterialFlag(EMF_ZBUFFER,true);
			Player::getInstance()->getNode()->setMaterialFlag(EMF_LIGHTING,true);
		}
		else
		{
			Player::getInstance()->getNode()->setMaterialTexture(0, tex_normal);
		}
	}
	
	// Tries to do an post FX
		EffectsManager::getInstance()->update();

	guienv->drawAll();

	if (app_state < APP_STATE_CONTROL)
		draw2DImages();

	driver->endScene();

	
}

void App::quickUpdate()
{
#ifdef EDITOR
	driver->beginScene(true, true, SColor(0,160,160,160));
	smgr->drawAll();
#else
	driver->beginScene(true, true, SColor(0,0,0,0));
#endif

	guienv->drawAll();
	driver->endScene();
}

void App::run()
{

	// Start the post process in the FX Manager
	EffectsManager::getInstance()->initPostProcess();
	EffectsManager::getInstance()->skydomeVisible(false); //Force the skydome to appear when the application is initialised; (Default state)

	//Add the player to the shadow nodes of XEffects
	if (EffectsManager::getInstance()->isXEffectsEnabled())
	{
		Player::getInstance()->getNode()->setMaterialFlag(EMF_LIGHTING, false);
		EffectsManager::getInstance()->addShadowToNode(Player::getInstance()->getNode());
		ISceneNode* shadow=Player::getInstance()->getObject()->getShadow();
		if (!shadow)
			printf("Failed to retrieve the fake shadow node!");
		else
			shadow->setVisible(false);
			//->setVisible(false);
	}

	//Apply the defined material to the player
	ShaderCallBack::getInstance()->setMaterials(DynamicObjectsManager::getInstance()->getPlayer()->getNode(),
		DynamicObjectsManager::getInstance()->getPlayer()->getMaterials());

	// Set the proper state if in the EDITOR or only the player application
#ifdef EDITOR
	//this->setAppState(APP_EDIT_LOOK); // old default state
	//this->setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
	app_state = APP_EDIT_PRJ;
	GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE,false);

	// Update the info panel with the current "active object"
	GUIManager::getInstance()->getInfoAboutModel();
	// Loading is complete
	GUIManager::getInstance()->getGUIElement(GUIManager::WIN_LOADER)->setVisible(false);

	CameraSystem::getInstance()->editCamMaya->setUpVector(vector3df(0,1,0));
	CameraSystem::getInstance()->setCamera(CameraSystem::CAMERA_EDIT);
	CameraSystem::getInstance()->editCamMaya->setPosition(vector3df(0.0f,1000.0f,-1000.0f));
	CameraSystem::getInstance()->editCamMaya->setTarget(vector3df(0.0f,0.0f,0.0f));
	CameraSystem::getInstance()->editCamMaya->setFarValue(50000.0f);
		//CameraSystem::getInstance()->setPosition(vector3df(oldcampos));
#else
	//EffectsManager::getInstance()->skydomeVisible(true); //Force the skydome to appear when the application is initialised; (Default state)
	//this->setAppState(APP_EDIT_WAIT_GUI);
	this->loadMapFromXML(mapname);
	//oldcampos = Player::getInstance()->getObject()->getPosition();
	//CameraSystem::getInstance()->setCamera(CameraSystem::CAMERA_GAME);
	//this->setAppState(APP_GAMEPLAY_NORMAL);
	//Player::getInstance()->getObject()->doScript();
	//LuaGlobalCaller::getInstance()->storeGlobalParams();
	//DynamicObjectsManager::getInstance()->initializeAllScripts();
	//DynamicObjectsManager::getInstance()->showDebugData(false);
	//TerrainManager::getInstance()->showDebugData(false);
	//GUIManager::getInstance()->setElementVisible(ST_ID_PLAYER_LIFE,true);
//	LuaGlobalCaller::getInstance()->doScript(scriptGlobal);

#endif



	// Hide the fog in the editor
	driver->setFog(SColor(0,255,255,255),EFT_FOG_LINEAR,300,999100);

	// Define the occlusion texture for the player (occlusion query)
	tex_occluded=driver->getTexture("../media/player/swordman_red.png");
	tex_normal=Player::getInstance()->getNode()->getMaterial(0).getTexture(0);
	//tex_normal=driver->getTexture("../media/player/swordman.png");

	// Occlusing query
	//driver->addOcclusionQuery(Player::getInstance()->getNode(), ((scene::IMeshSceneNode*)Player::getInstance()->getNode())->getMesh());
	driver->addOcclusionQuery(Player::getInstance()->getNode());

	// Instanciate the ray test class (debug purpose)
	raytester=new raytest();
	raytester->init(device);

	selectedNode=NULL;


	int lastFPS = -1;
	//	u32 timer = device->getTimer()->getRealTime();
	//	u32 timer2 = device->getTimer()->getRealTime();
	bool activated=false;

	// This is the core loop
	while(device->run())
	{
		this->update();
		// display frames per second in window title
		s32 fps = driver->getFPS();
		if (lastFPS != fps)
		{
			core::stringw str = appname;
			if (app_state>APP_STATE_CONTROL)
			{
				str += " FPS:";
				str += fps;
			}

			str += " PROJECT: ";
			str += currentProjectName;
			str += " MAP: ";
			str += currentMapName;
			//GUIManager::getInstance()->setStatusText(str.c_str());
			device->setWindowCaption(str.c_str());
			lastFPS = fps;
		}
	}
}


// Stuff needed only in the editor
#ifdef EDITOR
void App::updateEditMode()
{
	timer = device->getTimer()->getRealTime();

	if (guienv->getFocus()!=guienv->getRootGUIElement())  // DEBUG: REset the focus. Problem loosing focus. Need to fix the problem but hack does it.
	{
		if (app_state == APP_EDIT_VIEWDRAG) // Viewdrag mode will reset the focus to the root (pressing spacebar)
		{
			//if (cursorIsInEditArea())
			//	guienv->setFocus(guienv->getRootGUIElement());
#ifdef DEBUG
			//printf("In viewdrag mode\n");
#endif
		}

		if (!guienv->getFocus()) // Focus is pointer to an invalid pointer. Reset it.
		{
//#ifdef DEBUG
//printf("We lost the focus!\n");
//#endif
			guienv->setFocus(guienv->getRootGUIElement());
		}

	}

	// If the app state edit the terrain, then update the terrain
	if(app_state == APP_EDIT_TERRAIN_PAINT_VEGETATION || app_state == APP_EDIT_TERRAIN_TRANSFORM)
		TerrainManager::getInstance()->update();

	if (selectedNode  && app_state!=APP_EDIT_CHARACTER)
		GUIManager::getInstance()->updateEditCameraString(selectedNode);
	else
		GUIManager::getInstance()->updateEditCameraString(NULL);

	if (app_state==APP_EDIT_CHARACTER)
		GUIManager::getInstance()->updateEditCameraString(Player::getInstance()->getNode());

	// Trie to display the node as we go with the mouse cursor in edit mode
	if((app_state == APP_EDIT_DYNAMIC_OBJECTS_MODE || app_state==APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE))
	{

		if (toolstate==TOOL_DO_ADD) //Will "preselect" an item only in ADD mode
			setPreviewSelection();
	}

	GUIManager::getInstance()->update(); // Update the GUI when editing objects
	// Enter the refresh after a timer duration OR if the terrain transform is used
	if ((timer2-timer)>17) // (17)1/60th second refresh interval
	{
		timer2 = device->getTimer()->getRealTime();
		if(app_state < APP_STATE_CONTROL)
		{

			// --- Drag the view when the spacebar is pressed
			if (app_state != APP_EDIT_DYNAMIC_OBJECTS_SCRIPT
				&& app_state != APP_EDIT_WAIT_GUI
				&& app_state != APP_EDIT_PLAYER_SCRIPT
				&& app_state != APP_EDIT_SCRIPT_GLOBAL
				// && app_state != APP_EDIT_CHARACTER
				//&& app_state != APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE
				)
			{
				// Activate "Viewdrag" mode
				// This state is checked by the MAYA camera
				// Will allow to move and rotate the view by the mouse when it's in that mode
				if(isKeyPressed(KEY_SPACE))
				{
					if (app_state != APP_EDIT_VIEWDRAG)
					{
						old_state = app_state;
						setAppState(APP_EDIT_VIEWDRAG);
#ifdef DEBUG
						//printf("Set camera settings...\n");
#endif
					}
				}
			}
			// Return the edit mode to normal after the spacebar is pressed (viewdrag)
			if ((app_state == APP_EDIT_VIEWDRAG) && !(isKeyPressed(KEY_SPACE)))
			{
				setAppState(old_state);
				//guienv->setFocus(guienv->getRootGUIElement()); // reset the focus when we release the spacebar
				return;
			}
			// --- End of code for drag of view


			//Move the player update code
			if(app_state == APP_EDIT_CHARACTER)
			{
				if(isMousePressed(0) && cursorIsInEditArea())
					Player::getInstance()->getObject()->setPosition(getMousePosition3D(100).pickedPos);
				return;
			}

			//Feature: Take a note of the position of the camera or object into the console
			//Check for the pressing of CTRL+C
			if(app_state < APP_STATE_CONTROL)
			{
				if (isKeyPressed(KEY_LCONTROL) && isKeyPressed(KEY_KEY_C))
				{
					if (!keytoggled)
					{
						core::stringw text = L"";
						if (selectedNode && (app_state==APP_EDIT_DYNAMIC_OBJECTS_MODE || app_state==APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE))
							text = core::stringw(L"Note: ").append(GUIManager::getInstance()->getEditCameraString(selectedNode));
						else
							text = core::stringw(L"Note: ").append(GUIManager::getInstance()->getEditCameraString(NULL));

						GUIManager::getInstance()->setConsoleText (text.c_str(), video::SColor(255,11,120,13));
						keytoggled=true;
					}
				}
				else
				{
					keytoggled=false;
					//CameraSystem::getInstance()->getNode()
				}
			}




			// Move the selected object in ADD mode
			if(app_state == APP_EDIT_DYNAMIC_OBJECTS_MOVE_ROTATE && cursorIsInEditArea())
			{
				if (!moveupdown)
				{
					// Change the ID of the moved mesh so it's won't collision with the ray.
					irr::s32 oldID=lastMousePick.pickedNode->getID();
					lastMousePick.pickedNode->setID(0x0010);

					vector3df newposition = vector3df(0,0,0);

					if (snapfunction) // If snapping is activated use the function
						newposition=calculateSnap(getMousePosition3D(100).pickedPos,currentsnapping);
					else
						newposition=getMousePosition3D(100).pickedPos;

					//newposition=lastMousePick.pickedNode->getPosition()+(newposition-lastMousePick.pickedNode->getPosition());

					lastMousePick.pickedNode->setPosition(newposition);
					lastMousePick.pickedNode->setID(oldID);
					initialposition=lastMousePick.pickedNode->getPosition();
					return;
				}
				else
				{
					position2d<s32> mousepos2=device->getCursorControl()->getPosition();
					core::vector3df newpos = initialposition;
					//lastMousePick.pickedNode->getPosition();
					newpos.Y=newpos.Y+((mousepos.Y-mousepos2.Y));

					if (snapfunction) // If snapping is activated use the function
						lastMousePick.pickedNode->setPosition(calculateSnap(newpos,currentsnapping));
					else
						lastMousePick.pickedNode->setPosition(newpos);

					return;

				}
			}

			//Refresh the infos about the selections
			if (app_state==APP_EDIT_DYNAMIC_OBJECTS_MODE && toolstate==TOOL_DO_SEL)
			{
				if (selectedNode)
				{
					IGUIStaticText * text = NULL;
					DynamicObject * object = NULL;
					core::stringw objtype = "";
					core::stringw templatename = "";

					templatename = DynamicObjectsManager::getInstance()->getActiveObject()->getName();

					// Get the selected dynamic object
					object = DynamicObjectsManager::getInstance()->getObjectByName(((core::stringc)selectedNode->getName()).c_str());

					if (object)
					{
						DynamicObject::TYPE obj = object->getType();
						switch (obj)
						{

							case DynamicObject::OBJECT_TYPE_PLAYER:
								objtype = LANGManager::getInstance()->getText("objtype_player").c_str();
								break;

							case DynamicObject::OBJECT_TYPE_NPC:
								objtype = LANGManager::getInstance()->getText("objtype_NPC").c_str();
								break;

							case DynamicObject::OBJECT_TYPE_LOOT:
								objtype = LANGManager::getInstance()->getText("objtype_loot").c_str();
								break;

							case DynamicObject::OBJECT_TYPE_INTERACTIVE:
								objtype = LANGManager::getInstance()->getText("objtype_int").c_str();
								break;

							case DynamicObject::OBJECT_TYPE_NON_INTERACTIVE:
								objtype = LANGManager::getInstance()->getText("objtype_nint").c_str();
								break;

							case DynamicObject::OBJECT_TYPE_WALKABLE:
								objtype = LANGManager::getInstance()->getText("objtype_walkable").c_str();
								break;


							default:
							break;
						}
					}

					text = ((IGUIStaticText *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_SELOBJECT,true));
					if (text)
						text->setText(((core::stringw)selectedNode->getName()).c_str());

					text = ((IGUIStaticText *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_SELOBJECT_TYPE,true));
					if (text)
						text->setText(objtype.c_str());

					text = ((IGUIStaticText *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_OBJ_SCRIPT,true));
					if (text && object)
					{
						if (object->getScript().size()>0)
							text->setText(LANGManager::getInstance()->getText("bt_dialog_yes").c_str());
						else
							text->setText(LANGManager::getInstance()->getText("bt_dialog_no").c_str());
					}

					text = ((IGUIStaticText *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_CUR_TEMPLATE,true));
					if (text)
						text->setText(templatename.c_str());
				} else
				{

					core::stringw templatename = "";
					IGUIStaticText * text = NULL;

					templatename = DynamicObjectsManager::getInstance()->getActiveObject()->getName();

					text = ((IGUIStaticText *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_SELOBJECT,true));
					if (text)
						text->setText(LANGManager::getInstance()->getText("panel_sel_sel1").c_str());

					text = ((IGUIStaticText *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_SELOBJECT_TYPE,true));
					if (text)
						text->setText(LANGManager::getInstance()->getText("panel_sel_sel1").c_str());

					text = ((IGUIStaticText *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_OBJ_SCRIPT,true));
					if (text)
						text->setText(LANGManager::getInstance()->getText("panel_sel_sel1").c_str());

					text = ((IGUIStaticText *)guienv->getRootGUIElement()->getElementFromId(GUIManager::TXT_ID_CUR_TEMPLATE,true));
					if (text)
						text->setText(templatename.c_str());

				}
			}

			// Tools mode refresh
			if (app_state==APP_EDIT_DYNAMIC_OBJECTS_MODE && toolstate!=TOOL_NONE && toolactivated)
			{
				if (toolstate==TOOL_DO_MOV && toolactivated) // Will move the object
				{
					if (!moveupdown && selectedNode)
					{
						// Change the ID of the moved mesh so it's won't collision with the ray.
						irr::s32 oldID=selectedNode->getID();
						selectedNode->setID(0x0010);

						vector3df newposition = vector3df(0,0,0);

						if (snapfunction) // If snapping is activated use the function
							newposition=calculateSnap(getMousePosition3D(100).pickedPos,currentsnapping);
						else
							newposition=getMousePosition3D(100).pickedPos;

						if (GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_POS_X))
							newposition.X = initialposition.X;

						if (GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_POS_Y))
							newposition.Y = initialposition.Y;

						if (GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_POS_Z))
							newposition.Z = initialposition.Z;

						selectedNode->setPosition(newposition);

						selectedNode->setID(oldID);
						initialposition=selectedNode->getPosition();
						GUIManager::getInstance()->updateNodeInfos(selectedNode);
					}
					else
					{
						if (selectedNode)
						{
							position2d<s32> mousepos2=device->getCursorControl()->getPosition();
							core::vector3df newposition = initialposition;
							//lastMousePick.pickedNode->getPosition();
							newposition.Y=newposition.Y+((mousepos.Y-mousepos2.Y));

							if (GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_POS_Y))
								return;

							if (snapfunction) // If snapping is activated use the function
								selectedNode->setPosition(calculateSnap(newposition,currentsnapping));
							else
								selectedNode->setPosition(newposition);

							GUIManager::getInstance()->updateNodeInfos(selectedNode);
						}

					}
					return;
				}

				if (toolstate==TOOL_DO_ROT && toolactivated && selectedNode) // Will rotate the object
				{
					if (!moveupdown)
					{
						position2d<s32> mousepos2=device->getCursorControl()->getPosition();
						vector3df newrotation = initialrotation;

						//Checkboxes define if the axis can be modified
						if (!GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_ROT_Y))
							newrotation.Y=initialrotation.Y+(mousepos.X-mousepos2.X);


						if (!GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_ROT_X))
							newrotation.X=initialrotation.X+(mousepos.Y-mousepos2.Y);

						selectedNode->setRotation(newrotation);
						GUIManager::getInstance()->updateNodeInfos(selectedNode);

					}
					else
					{
						position2d<s32> mousepos2=device->getCursorControl()->getPosition();
						vector3df newrotation = selectedNode->getRotation(); //initialrotation;
						if (!GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_ROT_Z))
							newrotation.Z=initialrotation.Z+(mousepos.X-mousepos2.X);

						selectedNode->setRotation(newrotation);
						GUIManager::getInstance()->updateNodeInfos(selectedNode);

					}

					return;
				}


				if (toolstate==TOOL_DO_SCA && toolactivated && selectedNode) // Will rotate the object
				{
					position2d<s32> mousepos2=device->getCursorControl()->getPosition();
					vector3df newscale = initialscale;

					f32 tscale=-f32(mousepos.X-mousepos2.X)/10.0f;

					if ((initialscale.X+tscale)<0.001f)
					{
						initialscale.X=0.001f;
						initialscale.Y=0.001f;
						initialscale.Z=0.001f;
					} else
					{
						if (!GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_SCA_X))
							newscale.X=initialscale.X+tscale;

						if (!GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_SCA_Y))
							newscale.Y=initialscale.Y+tscale;

						if (!GUIManager::getInstance()->getCheckboxState(GUIManager::CB_ID_SCA_Z))
							newscale.Z=initialscale.Z+tscale;
					}
					selectedNode->setScale(newscale);
					GUIManager::getInstance()->updateNodeInfos(selectedNode);


					return;
				}
			}
		}
	}
}

#endif


void App::updateGameplay()
{

	timer = device->getTimer()->getRealTime();
	delta = timer-timer3; //Get the difference between each frame in MS

	//Levelchange as been asked
	//This is used when we use the lua command to load another project
	if (levelchange && (timer_lua-timer)>1000)
	{
		Player::getInstance()->setTaggedTarget(NULL);
		stopGame();
		cleanWorkspace();
		this->loadMapFromXML(levelfilename);
		playGame();
		levelchange=false;
	}

	// Refresh the projectile manager
	Projectile::getInstance()->update();

	// Refresh the NPC loop
	// Update all the NPC on the map (including the player)
	DynamicObjectsManager::getInstance()->updateAll();

	// The timer delay should be directly in the NPC, as the check for animation event should not be delayed.
	if ((timer-timer3)>34) // (17 )1/60 second
	{
		// Update the NPc refresh
		timer3 = device->getTimer()->getRealTime();

		// Update the combat system (mostly for damage over time management (dot))
		Combat::getInstance()->update();

	}

}

void App::cleanWorkspace()
{
	TerrainManager::getInstance()->clean();

	DynamicObjectsManager::getInstance()->clean(false);

	CameraSystem::getInstance()->editCamMaya->setUpVector(vector3df(0,1,0));
	CameraSystem::getInstance()->setCamera(CameraSystem::CAMERA_EDIT);
	CameraSystem::getInstance()->editCamMaya->setPosition(vector3df(0.0f,1000.0f,-1000.0f));
	CameraSystem::getInstance()->editCamMaya->setTarget(vector3df(0.0f,0.0f,0.0f));
	CameraSystem::getInstance()->editCamMaya->setFarValue(90000.0f);
	driver->setFog(SColor(0,255,255,255),EFT_FOG_LINEAR,300,999100);

	smgr->getMeshCache()->clear(); //Clear the mesh cache
}

bool App::createProjectData()
{
	IGUIEditBox * box1 = NULL;
	IGUIEditBox * box2 = NULL;
	IGUIEditBox * box3 = NULL;

	mapinfos.clear(); //Clear the infos about the maps
	scriptGlobal = ""; //Clear the global script and data

	IGUIElement* elem = GUIManager::getInstance()->getGUIElement(GUIManager::TXT_ID_PROJECT_NAME);
	if (elem)
		box1 = (IGUIEditBox*)elem;
	elem = GUIManager::getInstance()->getGUIElement(GUIManager::TXT_ID_FIRST_MAP_NAME);
	if (elem)
		box2 = (IGUIEditBox*)elem;
	elem = GUIManager::getInstance()->getGUIElement(GUIManager::TXT_ID_FIRST_MAP_DESC);
	if (elem)
		box3 = (IGUIEditBox*)elem;

	core::stringw line1 = core::stringw(box1->getText());
	core::stringw line2 = core::stringw(box2->getText());
	core::stringw line3 = core::stringw(box3->getText());

	currentProjectName = line1;
	currentMapName = line2;
	currentMapDescription = line3;
	currentMapNo = 0;

	//Add this to the first map definition
	mapinfo map;
	map.mapname = currentMapName;
	map.mapdescription = currentMapDescription;
	mapinfos.push_back(map);

	//Check if the user has entered all the proper informations.
	printf("     >>> Calling project data!!\n   || %s\n|| %s\n|| %s\n", stringc(box1->getText()).c_str(), stringc(box2->getText()).c_str(), stringc(box3->getText()).c_str());
	if (line1.size() > 0 && line2.size() > 0)
		return true;
	else
		return false;
}

//This should be invoked when the content of the current map is being cleared and started as a new map
void App::createNewMap()
{

	lastScannedPick.pickedNode = NULL;
	if (selectedNode)
	{
		selectedNode->setDebugDataVisible(0);
		selectedNode = NULL;
	}
	GUIManager::getInstance()->setWindowVisible(GUIManager::GCW_ID_DYNAMIC_OBJECT_CONTEXT_MENU, false);

	// Initialize the camera (2) is maya type camera for editing
	CameraSystem::getInstance()->setCamera(CameraSystem::CAMERA_EDIT);

	APP_STATE old_state = getAppState();

	this->cleanWorkspace();

	CameraSystem::getInstance();

	TerrainManager::getInstance()->createEmptySegmentMatrix(50,50);

	Player::getInstance();

	CameraSystem::getInstance()->editCamMaya->setPosition(vector3df(0,1000,-1000));
	CameraSystem::getInstance()->editCamMaya->setTarget(vector3df(0,0,0));
	CameraSystem::getInstance()->setCameraHeight(0); // Refresh the camera

	Player::getInstance()->getNode()->setPosition(vector3df(0.0f,0.0f,0.0f));
	Player::getInstance()->getNode()->setRotation(vector3df(0.0f,0.0f,0.0f));

	// Put back the player object in the list of the dynamic objects
	DynamicObjectsManager::getInstance()->setPlayer();

	//this->setAppState(APP_EDIT_DYNAMIC_OBJECTS_MODE);
	//GUIManager::getInstance()->setElementEnabled(GUIManager::BT_ID_DYNAMIC_OBJECTS_MODE, false);

}

void App::loadProject(DIALOG_FUNCTION function)
{

	old_state = getAppState();

	setAppState(APP_EDIT_WAIT_GUI);
	if (lastScannedPick.pickedNode!=NULL)
	{
			lastScannedPick.pickedNode=NULL;
			selectedNode=NULL;
		}

	// Store the dialog function value and remember it.
	df = function;

	// Have to rethink how to do it. It used the gameplay dialog.
	//bool ansSave = GUIManager::getInstance()->showDialogQuestion(stringc(LANGManager::getInstance()->getText("msg_override_project")).c_str());
	//GUIManager::getInstance()->flush();
	/*bool ansSave=false;
	if(ansSave)
	{
	stringc filename = "../projects/";
	filename += currentProjectName;
	saveMapToXML(filename);
	GUIManager::getInstance()->showDialogQuestion(LANGManager::getInstance()->getText("msg_saved_ok").c_str());
	GUIManager::getInstance()->flush();
	}

	stringc name = GUIManager::getInstance()->showInputQuestion(LANGManager::getInstance()->getText("msg_new_project_name").c_str());
	GUIManager::getInstance()->flush();

	this->cleanWorkspace();

	stringc filename = "../projects/";
	filename += name;
	filename += ".XML";

	currentProjectName = name;
	currentProjectName += ".XML";

	setAppState(APP_EDIT_WAIT_GUI);
	if(this->loadMapFromXML(filename))
	GUIManager::getInstance()->showDialogMessage(LANGManager::getInstance()->getText("msg_loaded_ok"));
	else
	GUIManager::getInstance()->showDialogMessage(LANGManager::getInstance()->getText("msg_loaded_error"));

	//this->loadProject("");
	setAppState(old_state);
	//setAppState(APP_STATE_CONTROL);*/


	// (2/12/12) new filerequester method
	// The event manager will receive the event and load the file.
	setAppState(APP_WAIT_FILEREQUEST);
	if (!selector)
	{

		// Create a load file selector
		selector = new CGUIFileSelector(getLangText("msg_prj_lp0").c_str(), guienv, guienv->getRootGUIElement(), 1, CGUIFileSelector::EFST_OPEN_DIALOG);
		// Create a base icon for the files
		selector->setCustomFileIcon(driver->getTexture("../media/art/file.png"));
		// Create a base icon for the folders
		selector->setCustomDirectoryIcon(driver->getTexture("../media/art/folder.png"));
		// Add a new file filters (Normally for what is required to load)
		//selector->setStartingPath(L"../");
		if (function == DF_PROJECT)
		{
			selector->addFileFilter((wchar_t *)LANGManager::getInstance()->getText("file_IRB_files").c_str(), L"xml", driver->getTexture("../media/art/wma.png"));
			// Create a "favorite places"
			selector->addPlacePaths((wchar_t *)LANGManager::getInstance()->getText("file_IRB_path").c_str(),L"../projects",driver->getTexture("../media/art/places_folder.png"));
			// Define in what path the request will open (it accept full or relative paths)

		}
		else
		{
			// Create a "favorite places"
			selector->addPlacePaths(L"Dynamic object folder",L"../media/dynamic_objects",driver->getTexture("../media/art/places_folder.png"));
			selector->addFileFilter(L"OBJ Model", L"obj", driver->getTexture("../media/art/wma.png"));
			selector->addFileFilter(L"3DS Model", L"3ds", driver->getTexture("../media/art/wma.png"));
			selector->addFileFilter(L"B3D Model", L"b3d", driver->getTexture("../media/art/wma.png"));
			selector->addFileFilter(L"DirectX Model", L"x", driver->getTexture("../media/art/wma.png"));
		}

		// This is required for the window stretching feature
		selector->setDevice(device);


#ifdef WIN32

		// Populate with standard windows favorites paths
		selector->populateWindowsFAV();
#else
		// Add some common linux paths
		selector->populateLinuxFAV();
#endif

		if (function == DF_PROJECT)
			selector->setStartingPath(L"../projects"); // Projects
		else
			selector->setStartingPath(L"../media/dynamic_objects"); // Replacing objects
	}

}

/*
void App::loadProject(stringc filename)
{
this->cleanWorkspace();
if(!this->loadMapFromXML("../projects/myProjectTiny.xml")) this->createNewProject("temp_project");
}
*/

// This will load the project contained in the selector
// Call is coming directly from the event manager
// Since we're using our new file selector and the method uses the events
// It will be useful to rename that method (obsolete name)

// This need to be reworked as it might be confusing (not only for projects)
// As it's been used for getting the filename and doing actions
void App::loadProjectFile(bool value)
{
	vector3df oldrotation = vector3df(0,0,0);
	core::stringw oldscript = L"";
	DynamicObject* object=NULL;

	if (df==DF_MODEL && value)
	{
		if (selector)
		{
			lastFilename=(core::stringc)selector->getFileName();
			GUIManager::getInstance()->setConsoleText(selector->getFileName());
			GUIManager::getInstance()->setConsoleText(selector->getOnlyFileName());
			// This is a file loader
			if (selector->isSaver()==false)
			{

				selector->setVisible(false); // Hide the file selector
				// Keep the "good stuff"
				oldrotation = lastMousePick.pickedNode->getRotation();
				oldscript = DynamicObjectsManager::getInstance()->getScript(lastMousePick.pickedNode->getName());

				//Tell the dynamic Objects Manager to remove the node
				DynamicObjectsManager::getInstance()->removeObject(lastMousePick.pickedNode->getName());

				// remove the object for the selection
				lastScannedPick.pickedNode=NULL;
				lastMousePick.pickedNode=NULL;
				selectedNode=NULL;

				// Create the new object from the template and put the old values back in.
				object = DynamicObjectsManager::getInstance()->createCustomObjectAt(lastMousePick.pickedPos, lastFilename);
				object->setScript(oldscript);
				object->setRotation(oldrotation);
			}
		}
	}

	// Project loading
	if (df==DF_PROJECT && value)
	{
		// Close and drop the file selector

		//Clean up the current world and load the scene

		// Here if it's the load file selector
		if (selector)
		{


            //return stringw(out);
#ifndef WIN32
            char out[255]; //Convert the text in UTF8
            core::wcharToUtf8(selector->getFileName(),out,255);
            core::stringc file=(core::stringc)out;
#else
            core::stringc file=(core::stringc)selector->getFileName();
#endif // WIN32



			// This is a file loader
			if (selector->isSaver()==false)
			{
				//printf("Loading project now!\n");
				cleanWorkspace();

				selector->setVisible(false);
				this->loadMapFromXML(file);

				//currentProjectName = file;

				//Recreate the empty tile matrix so the user can expand his loaded map.
				TerrainManager::getInstance()->createEmptySegmentMatrix(50,50);

				// Put back the player object in the list of the dynamic objects
				DynamicObjectsManager::getInstance()->setPlayer();
			}
			// This is a file saver
			if (selector->isSaver()==true)
			{
				selector->setVisible(false);
				//printf("Saving project now!\n");
				this->saveMapToXML(file);
			}
			guienv->setFocus(guienv->getRootGUIElement());
			//Destroy the selector
			selector->remove();
			selector=NULL;
			setAppState(old_state);
			//printf ("The returned string is %s\n",file);
		}

		// Here is the save file selector
		else if (saveselector)
		{


			#ifndef WIN32
            char out[255]; //Convert the text in UTF8
            core::wcharToUtf8(saveselector->getFileName(),out,255);
            core::stringc file=(core::stringc)out;
#else
            core::stringc file = (core::stringc)saveselector->getFileName();
#endif // WIN32

			// For windows put as backslash.
#ifdef WIN32
			file.replace('/','\\');
#endif
			saveselector->setVisible(false);

			this->saveMapToXML(file);

			guienv->setFocus(guienv->getRootGUIElement());
			saveselector->remove();
			saveselector=NULL;
			setAppState(old_state);
		}
	}

	else
		// User cancelled the file selector. remove them
	{
		setAppState(old_state);
		if (selector)
		{
			guienv->setFocus(guienv->getRootGUIElement());
			selector->remove();
			selector=NULL;
		}
		if (saveselector)
		{
			guienv->setFocus(guienv->getRootGUIElement());
			saveselector->remove();
			saveselector=NULL;
		}
	}

	// Set back the camera after loading the map (could be perhaps improved later, to select the proper camera after loading (ingame loading))
	CameraSystem::getInstance()->setCameraHeight(0); // Refresh the camera
	this->setAppState(APP_EDIT_LOOK); // Put back in "default edit state"
	//setAppState(old_state);
}

// Timed load of the new project, give a chance to LUA to close and start the new thing.
void App::loadProjectGame(irr::core::stringc filename)
{
	this->timer_lua=device->getTimer()->getRealTime();
	levelchange=true;
	levelfilename = filename;
}

void App::saveProjectDialog()
{
	//Save current state, disabled for now
	//APP_STATE old_state = getAppState();
	setAppState(APP_WAIT_FILEREQUEST);

	// Old method of request for save file (only a text input)
	/*
	if(currentProjectName == stringc("irb_temp_project"))
	{
	currentProjectName = GUIManager::getInstance()->showInputQuestion(LANGManager::getInstance()->getText("msg_new_project_name"));
	GUIManager::getInstance()->flush();
	EventReceiver::getInstance()->flushKeys();
	currentProjectName += ".XML";
	}

	stringc filename = "../projects/";
	filename += currentProjectName;
	this->saveMapToXML(filename);
	GUIManager::getInstance()->showDialogMessage(LANGManager::getInstance()->getText("msg_saved_ok"));
	GUIManager::getInstance()->flush();*/

	if (!saveselector)
	{
		// Create a save file selector // EFST_OPEN_DIALOG // EFST_SAVE_DIALOG
		saveselector = new CGUIFileSelector(getLangText("msg_prj_sp0").c_str(), guienv, guienv->getRootGUIElement(), 1, CGUIFileSelector::EFST_SAVE_DIALOG);
		// Create a base icon for the files
		saveselector->setCustomFileIcon(driver->getTexture("../media/art/file.png"));
		// Create a base icon for the folders
		saveselector->setCustomDirectoryIcon(driver->getTexture("../media/art/folder.png"));
		// Add a new file filters (Normally for what is required to load)
		saveselector->addFileFilter((wchar_t *)LANGManager::getInstance()->getText("file_IRB_files").c_str(), L"xml", driver->getTexture("../media/art/wma.png"));

		// This is required for the window stretching feature
		saveselector->setDevice(device);

		// Create a "favorite places"
		saveselector->addPlacePaths((wchar_t *)LANGManager::getInstance()->getText("file_IRB_path").c_str(),L"../projects",driver->getTexture("../media/art/places_folder.png"));
#ifdef WIN32

		// Populate with standard windows favorites paths
		saveselector->populateWindowsFAV();
#else
		// Add some common linux paths
		saveselector->populateLinuxFAV();
#endif

		// Define in what path the request will open (it accept full or relative paths)
		saveselector->setStartingPath(L"../projects");
	}


	//setAppState(old_state);
}

stringc App::getProjectName()
{
	return this->currentProjectName;
}

//This will save the project file to XML
//Then will then initialize the save of the current level
bool App::saveProjectToXML()
{
	core::stringc path = core::stringc(editorfunc->getProjectsPath());
	path += "/";
	path += core::stringc(currentProjectName);

	if (!device->getFileSystem()->existFile(path.c_str()))
		editorfunc->createFolder(path);

	printf("Project folder is now created: %s\n", path.c_str());
	core::stringc filename = core::stringc(currentProjectName);
	core::stringc mapname = core::stringc(currentMapName + L".map");

	//filename.replace("/", "\\");


	device->getFileSystem()->changeWorkingDirectoryTo(path.c_str());

	// XML saving of the data in the project
	TiXmlDocument doc1;
	TiXmlDeclaration* decl1 = new TiXmlDeclaration("1.0", "utf-8", "");
	TiXmlElement* irb_project = new TiXmlElement("IrrRPG_Builder_Project");
	irb_project->SetAttribute("version", "0.3");

	TiXmlElement* proj = new TiXmlElement("project");
	proj->SetAttribute("name", filename.c_str());
	irb_project->LinkEndChild(proj);


	for (u32 a = 0; a < mapinfos.size(); a++)
	{
		TiXmlElement* map = new TiXmlElement("map");

		char out[4096];
		wcharToUtf8(mapinfos[a].mapname.c_str(), out, 4096);
		map->SetAttribute("name", stringc(out).c_str());

		wcharToUtf8(mapinfos[a].mapdescription.c_str(), out, 4096);
		map->SetAttribute("desc", stringc(out).c_str());
		irb_project->LinkEndChild(map);
	}

	GUIManager::getInstance()->setTextLoader(L"Saving the global scripts");
	quickUpdate();

	TiXmlElement* globalScript = new TiXmlElement("global_script");
	globalScript->SetAttribute("script", scriptGlobal.c_str());
	irb_project->LinkEndChild(globalScript);

	// Closing the XML file
	doc1.LinkEndChild(decl1);
	doc1.LinkEndChild(irb_project);
	bool result = doc1.SaveFile("project.xml");

	//Save the XML data of the current map
	path = path.append("/").append(core::stringc(currentMapName.c_str()));
	if (!device->getFileSystem()->existFile(path.c_str()))
	{
		editorfunc->createFolder(path);
		printf("No map folder present! Creating this folder: %s\n", path.c_str());
	}

	device->getFileSystem()->changeWorkingDirectoryTo(path.c_str());
	saveMapToXML(mapname.c_str());

	//Set back the path to the original (application path)
	device->getFileSystem()->changeWorkingDirectoryTo(core::stringc(editorfunc->getApplicationPath()).c_str());

	return true;

}

//!This will load the project from XML
//!Then will load the default map
bool App::loadProjectFromXML()
{
	createNewMap(); //Clear map data
	mapinfos.clear(); //Clear previously infos about maps.
	core::stringc pathproj = core::stringc(editorfunc->getProjectsPath()) + "/";
	pathproj += core::stringc(currentProjectName) + "/project.xml";
	//device->getFileSystem()->changeWorkingDirectoryTo(pathproj.c_str());
	TiXmlDocument doc(pathproj.c_str());
	if (!doc.LoadFile()) return false;

	TiXmlElement* root = doc.FirstChildElement("IrrRPG_Builder_Project");

	if (root)
	{
		if (atof(root->Attribute("version")) * 100 != float(APP_VERSION))
		{
#ifdef DEBUG
			cout << "DEBUG : XML : INCORRECT VERSION!" << endl;
#endif

			//return false;
		}

		//Global scripts should be there. Not in the map xml file
		TiXmlElement* globalScriptXML = root->FirstChildElement("global_script");
		if (globalScriptXML)
		{
			scriptGlobal = globalScriptXML->ToElement()->Attribute("script");
		}

		TiXmlElement* projectname = root->FirstChildElement("project");
		if (projectname)
		{
			currentProjectName = core::stringw(projectname->ToElement()->Attribute("name"));
		}
		TiXmlNode*  mapdata = root->FirstChildElement("map");
		while (mapdata!=NULL)
		{
			//Convert the project from UTF8 (Saved in that format);
			core::stringc name = stringc(mapdata->ToElement()->Attribute("name"));
			core::stringc desc = stringc(mapdata->ToElement()->Attribute("desc"));
			wchar_t out[4096];
			utf8ToWchar(name.c_str(), out, 4096);

			currentMapName = core::stringw(out);

			utf8ToWchar(desc.c_str(), out, 4096);
 			currentMapDescription = core::stringw(out);

			mapinfo map;
			map.mapname = currentMapName;
			map.mapdescription = currentMapDescription;
			mapinfos.push_back(map);
			mapdata = root->IterateChildren("map", mapdata);
		}
		currentMapName = mapinfos[0].mapname;
		currentMapDescription = mapinfos[0].mapdescription;
		currentMapNo = 0;
		pathproj = core::stringc(editorfunc->getProjectsPath()) + "/";
		pathproj += core::stringc(currentProjectName);
		core::stringc mapname = pathproj + "/" + currentMapName + "/" + currentMapName + ".map";
		loadMapFromXML(mapname);

	}
	//Switch back to the base path once it's loaded
	device->getFileSystem()->changeWorkingDirectoryTo(core::stringc(editorfunc->getApplicationPath()).c_str());
	return true;
}

//!This will save the current map information into a XML file
//!
void App::saveMapToXML(stringc filename)
{

	this->filename=filename;
	GUIManager::getInstance()->getGUIElement(GUIManager::WIN_LOADER)->setVisible(true);
	TiXmlDocument doc;
	TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "ISO-8859-1", "" );

	TiXmlElement* irb_map = new TiXmlElement( "IrrRPG_Builder_MAP" );
	irb_map->SetAttribute("version","0.3");

	GUIManager::getInstance()->setTextLoader(L"Saving the terrain");
	quickUpdate();
	TerrainManager::getInstance()->saveToXML(irb_map);


	GUIManager::getInstance()->setTextLoader(L"Saving the active dynamic objects");
	quickUpdate();
	DynamicObjectsManager::getInstance()->saveToXML(irb_map);

		// Closing the XML file
	doc.LinkEndChild( decl );
	doc.LinkEndChild( irb_map );

	bool result = doc.SaveFile( filename.c_str() );

	//New: Will save the terrain tiles meshes separately. Caused some problems when doing this and saving the XML at the same time.
	TerrainManager::getInstance()->saveTerrainTiles();

	GUIManager::getInstance()->getGUIElement(GUIManager::WIN_LOADER)->setVisible(false);

	CameraSystem::getInstance()->setCameraHeight(0); // Refresh the camera

	filename.remove(".map");
	if (result)
		guienv->addMessageBox(LANGManager::getInstance()->getText("text_file_save_report").c_str(), core::stringw(LANGManager::getInstance()->getText("text_file_scene")
		.append(core::stringw(filename.c_str()).append(LANGManager::getInstance()->getText("msg_saved_ok").c_str()))).c_str()
		, true);
	else
		guienv->addMessageBox(L"Warning!",L"Warning, errors occured during saving of the map file!");


#ifdef DEBUG
	cout << "DEBUG : XML : PROJECT SAVED : " << filename.c_str() << endl;
#endif
}

//!This will load the current map information from a XML file
bool App::loadMapFromXML(stringc filename)
{
	IGUIWindow* window=(IGUIWindow*)GUIManager::getInstance()->getGUIElement(GUIManager::WIN_LOADER);
	window->setVisible(true);
	printf ("Trying to load this map: %s \n",filename.c_str());
	TiXmlDocument doc(filename.c_str());
	if (!doc.LoadFile()) return false;

#ifdef DEBUG
	cout << "DEBUG : XML : LOADING PROJECT : " << filename.c_str() << endl;
#endif

	TiXmlElement* root = doc.FirstChildElement( "IrrRPG_Builder_MAP" );

	if ( root )
	{
		if( atof(root->Attribute("version"))*100!=float(APP_VERSION) )
		{
#ifdef DEBUG
			cout << "DEBUG : XML : INCORRECT VERSION!" << endl;
#endif

			//return false;
		}

		TiXmlElement* terrain = root->FirstChildElement( "terrain" );
		if ( terrain )
		{
			GUIManager::getInstance()->setTextLoader(L"Loading the terrain");
#ifdef EDITOR
			quickUpdate();
#endif
			TerrainManager::getInstance()->loadFromXML(terrain);
		}

		TiXmlElement* dynamicObjs = root->FirstChildElement( "dynamic_objects" );
		if ( dynamicObjs )
		{
			GUIManager::getInstance()->setTextLoader(L"Loading the dynamic objects");
#ifdef EDITOR
			quickUpdate();
#endif
			DynamicObjectsManager::getInstance()->loadFromXML(dynamicObjs);
		}

		TiXmlElement* playerXML = root->FirstChildElement( "player" );
		if(playerXML)
		{
			// Player is a dynamic object now.
			// There is no need for now to load from this
		}
		//Rebuild the object list when loading a new scene
		GUIManager::getInstance()->buildSceneObjectList(current_listfilter);

		CameraSystem::getInstance()->setCameraHeight(0); // Refresh the camera
#ifndef EDITOR
		GUIManager::getInstance()->setTextLoader(L"");
		guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_PLAYER_START,true)->setVisible(true);
		guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_PLAYER_CONFIG,true)->setVisible(true);
#endif
#ifdef EDITOR
		GUIManager::getInstance()->getGUIElement(GUIManager::WIN_LOADER)->setVisible(false);
#endif

	}
	else
	{
#ifdef APP_DEBUG
		cout << "DEBUG : XML : THIS FILE IS NOT A IRRRPG BUILDER PROJECT!" << endl;
#endif
#ifdef EDITOR
		GUIManager::getInstance()->getGUIElement(GUIManager::WIN_LOADER)->setVisible(false);
#endif
		return false;
	}

#ifdef APP_DEBUG
	cout << "DEBUG : XML : PROJECT LOADED! "<< endl;
#endif

	///TODO:CLEAR PROJECT IF NOT RETURN TRUE ON LOAD PROJECT FROM XML

#ifdef EDITOR
	filename=device->getFileSystem()->getFileBasename(filename.c_str(), false);
	guienv->addMessageBox(LANGManager::getInstance()->getText("text_file_load_report").c_str(),core::stringw(LANGManager::getInstance()->getText("text_file_scene")
		.append(core::stringw(filename.c_str()).append(LANGManager::getInstance()->getText("msg_loaded_ok")))).c_str()
		,true);

	this->setAppState(APP_EDIT_LOOK); // Put back in default state
#endif
	TerrainManager::getInstance()->createEmptySegmentMatrix(50, 50);
	return true;
}

void App::initialize()
{

	// Initialize the sound engine
	SoundManager::getInstance();

	// Initialize the GUI class first
	GUIManager::getInstance();


#ifdef EDITOR
	GUIManager::getInstance()->setupEditorGUI();
#endif

	// Set the ambient light
	//smgr->setAmbientLight(SColorf(0.80f,0.85f,1.0f,1.0f));
	  smgr->setAmbientLight(SColorf(0.5f,0.60f,0.75f,1.0f));

	// Set the fog to be very far when not in gameplay
	driver->setFog(SColor(0,255,255,255),EFT_FOG_LINEAR,0,50000);

	//Create a sun light
	scene::ILightSceneNode * light=smgr->addLightSceneNode(0,vector3df(2500,25000,-50));

	if (xeffectenabler)
		EffectsManager::getInstance()->enableXEffects(true);

	//Set the light as shadow casting light with the light manager.
	EffectsManager::getInstance()->addShadowLight(vector3df(5.0f,185.0f,0.0f), vector3df(0,0,0), SColor(255,255,255,255));

	light->setLightType(ELT_DIRECTIONAL);
	//light->setLightType(ELT_POINT);
	light->setRadius(45000);
	//light->getLightData().SpecularColor=SColorf(0.4f,0.4f,0.5f,1.0f); //Some characters have too much specular on them. Limit the specular of the sun a little.
	//light->getLightData().DiffuseColor=SColor(1.0f,0,0,1.0f);


	//light->setRadius(45000);
	light->setRotation(vector3df(70.0f,30.0f,0.0f));


	screensize=driver->getScreenSize();

#ifdef EDITOR
	// Initialize the camera (2) is maya type camera for editing
	CameraSystem::getInstance()->setCamera(CameraSystem::CAMERA_EDIT);
//TerrainManager::getInstance()->createEmptySegment(vector3df(0,0,0));
	TerrainManager::getInstance()->createEmptySegmentMatrix(50,50);
	quickUpdate();
#endif


	GUIManager::getInstance()->setupGameplayGUI();
	quickUpdate();
	Player::getInstance();
	driver->setMinHardwareBufferVertexCount(0);
	core::stringc vendor = driver->getVendorInfo();
	printf ("Here is the vendor information: %s\n",vendor.c_str());
	this->currentProjectName = "";

	Projectile::getInstance(); //Instanciate the projectile class

	//Set the default view for player controls in gameplay
	//Must be done AFTER the camera system is initialized.
	if (defaultview==VIEW_RTS)
		this->setRTSView();
	if (defaultview==VIEW_RPG)
		this->setRPGView();
	if (defaultview==VIEW_FPS)
		this->setFPSView();

	//projectname = L"New IRB Project"; //Empty name for the project.

	projectpath = editorfunc->getUserDocumentsPath()+"/";
	bool result = device->getFileSystem()->existFile(projectpath.c_str());

	app_state = APP_EDIT_PRJ;
	GUIManager::getInstance()->createNewProjectGUI();
}

void App::shutdown()
{
	// Stuff to do when the device is closed.
	SoundManager::getInstance()->stopEngine();
	cleanWorkspace();
	DynamicObjectsManager::getInstance()->clean(true);
	device->closeDevice();
	device->run();
	if (device)
		device->drop();
	exit(0);

}

std::vector<stringw> App::getConsoleText()
{
	return console_event;
}

std::vector<SColor> App::getConsoleColor()
{
	return console_event_color;
}

void App::clearConsole()
{
	console_event.clear();
	console_event_color.clear();
	GUIManager::getInstance()->clearConsole();
}

stringw App::getLangText(irr::core::stringc node)
{
	return LANGManager::getInstance()->getText(node);
}


irr::f32 App::getBrushRadius(int number)
{
	f32 radius=0.0f;
#ifdef EDITOR

	if (number==0) //main radius
		radius = GUIManager::getInstance()->getScrollBarValue(GUIManager::SC_ID_TERRAIN_BRUSH_RADIUS);
	if (number==1) // inner radius
		radius = GUIManager::getInstance()->getScrollBarValue(GUIManager::SC_ID_TERRAIN_BRUSH_RADIUS2);
#endif
	return radius;
}

void App::setRTSView()
{

	IGUIButton* button1 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RTS,true));
	IGUIButton* button2 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RPG,true));
	IGUIButton* button3 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_FPS,true));
	if (button1)
		button1->setPressed(true);
	if (button2)
		button2->setPressed(false);
	if (button3)
		button3->setPressed(false);

	CameraSystem::getInstance()->setViewType(CameraSystem::VIEW_RTS);
}

void App::setRPGView()
{
	IGUIButton* button1 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RTS,true));
	IGUIButton* button2 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RPG,true));
	IGUIButton* button3 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_FPS,true));
	if (button1)
		button1->setPressed(false);
	if (button2)
		button2->setPressed(true);
	if (button3)
		button3->setPressed(false);

	CameraSystem::getInstance()->setViewType(CameraSystem::VIEW_RPG);
}

void App::setFPSView()
{
	IGUIButton* button1 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RTS,true));
	IGUIButton* button2 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_RPG,true));
	IGUIButton* button3 = ((IGUIButton *)guienv->getRootGUIElement()->getElementFromId(GUIManager::BT_CAMERA_FPS,true));
	if (button1)
		button1->setPressed(false);
	if (button2)
		button2->setPressed(false);
	if (button3)
		button3->setPressed(true);

	CameraSystem::getInstance()->setViewType(CameraSystem::VIEW_FPS);
}

// Snapping function
core::vector3df App::calculateSnap(vector3df input, f32 snapvalue)
{
	f32 X1=core::round32(input.X/snapvalue)+0.0f;
	f32 Y1=core::round32(input.Y/snapvalue)+0.0f;
	f32 Z1=core::round32(input.Z/snapvalue)+0.0f;

	X1=X1*snapvalue;
	Y1=Y1*snapvalue;
	Z1=Z1*snapvalue;

	core::vector3df result = core::vector3df(X1,Y1,Z1);
	return result;
}

irr::EKEY_CODE App::getKeycode(core::stringc text)
{
	//Check the string for each keyboard key (Could be simplified if we used values instead but would be less readable)
	//This could also me made in a utility function. (return a keycode from a string)
	text.make_lower();

	irr::EKEY_CODE key=irr::KEY_KEY_CODES_COUNT; //Keycount used as default;

	if (text=="a")
		key=KEY_KEY_A;
	if (text=="b")
		key=KEY_KEY_B;
	if (text=="c")
		key=KEY_KEY_C;
	if (text=="d")
		key=KEY_KEY_D;
	if (text=="e")
		key=KEY_KEY_E;
	if (text=="f")
		key=KEY_KEY_F;
	if (text=="g")
		key=KEY_KEY_G;
	if (text=="h")
		key=KEY_KEY_H;
	if (text=="i")
		key=KEY_KEY_I;
	if (text=="j")
		key=KEY_KEY_J;
	if (text=="k")
		key=KEY_KEY_K;
	if (text=="l")
		key=KEY_KEY_L;
	if (text=="m")
		key=KEY_KEY_M;
	if (text=="n")
		key=KEY_KEY_N;
	if (text=="o")
		key=KEY_KEY_O;
	if (text=="p")
		key=KEY_KEY_P;
	if (text=="q")
		key=KEY_KEY_Q;
	if (text=="r")
		key=KEY_KEY_R;
	if (text=="s")
		key=KEY_KEY_S;
	if (text=="t")
		key=KEY_KEY_T;
	if (text=="u")
		key=KEY_KEY_U;
	if (text=="v")
		key=KEY_KEY_V;
	if (text=="w")
		key=KEY_KEY_W;
	if (text=="x")
		key=KEY_KEY_X;
	if (text=="y")
		key=KEY_KEY_Y;
	if (text=="z")
		key=KEY_KEY_Z;
	if (text=="1")
		key=KEY_KEY_1;
	if (text=="2")
		key=KEY_KEY_2;
	if (text=="3")
		key=KEY_KEY_3;
	if (text=="4")
		key=KEY_KEY_4;
	if (text=="5")
		key=KEY_KEY_5;
	if (text=="6")
		key=KEY_KEY_6;
	if (text=="7")
		key=KEY_KEY_7;
	if (text=="8")
		key=KEY_KEY_8;
	if (text=="9")
		key=KEY_KEY_9;
	if (text=="0")
		key=KEY_KEY_0;
	if (text=="f1")
		key=KEY_F1;
	if (text=="f2")
		key=KEY_F2;
	if (text=="f3")
		key=KEY_F3;
	if (text=="f4")
		key=KEY_F4;
	if (text=="f5")
		key=KEY_F5;
	if (text=="f6")
		key=KEY_F6;
	if (text=="f7")
		key=KEY_F7;
	if (text=="f8")
		key=KEY_F8;
	if (text=="f9")
		key=KEY_F9;
	if (text=="f10")
		key=KEY_F10;
	if (text=="f11")
		key=KEY_F11;
	if (text=="f12")
		key=KEY_F12;


	return key;
}
