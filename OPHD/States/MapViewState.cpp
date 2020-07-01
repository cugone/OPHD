// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "MapViewState.h"
#include "MainMenuState.h"

#include "../Constants.h"
#include "../DirectionOffset.h"
#include "../FontManager.h"
#include "../GraphWalker.h"
#include "../StructureCatalogue.h"
#include "../StructureTranslator.h"

#include "../Map/Tile.h"

#include "../Things/Robots/Robots.h"
#include "../Things/Structures/Structures.h"

#include <algorithm>
#include <sstream>
#include <vector>

// Disable some warnings that can be safely ignored.
#pragma warning(disable: 4244) // possible loss of data (floats to int and vice versa)


using namespace NAS2D;


const std::string MAP_TERRAIN_EXTENSION = "_a.png";
const std::string MAP_DISPLAY_EXTENSION = "_b.png";

extern NAS2D::Image* IMG_LOADING; /// \fixme Find a sane place for this.
extern NAS2D::Image* IMG_SAVING; /// \fixme Find a sane place for this.
extern Point<int> MOUSE_COORDS;
extern MainReportsUiState* MAIN_REPORTS_UI;


int ROBOT_ID_COUNTER = 0; /// \fixme Kludge

Rectangle<int> MENU_ICON;
Rectangle<int> RESOURCE_PANEL_PIN{0, 1, 8, 19};
Rectangle<int> POPULATION_PANEL_PIN{675, 1, 8, 19};

Rectangle<int> MOVE_NORTH_ICON;
Rectangle<int> MOVE_SOUTH_ICON;
Rectangle<int> MOVE_EAST_ICON;
Rectangle<int> MOVE_WEST_ICON;
Rectangle<int> MOVE_UP_ICON;
Rectangle<int> MOVE_DOWN_ICON;

std::string CURRENT_LEVEL_STRING;

std::map <int, std::string> LEVEL_STRING_TABLE = 
{
	{ constants::DEPTH_SURFACE, constants::LEVEL_SURFACE },
	{ constants::DEPTH_UNDERGROUND_1, constants::LEVEL_UG1 },
	{ constants::DEPTH_UNDERGROUND_2, constants::LEVEL_UG2 },
	{ constants::DEPTH_UNDERGROUND_3, constants::LEVEL_UG3 },
	{ constants::DEPTH_UNDERGROUND_4, constants::LEVEL_UG4 }
};


Font* MAIN_FONT = nullptr;


using namespace micropather;
extern MicroPather* pather;
extern std::vector<void*> path;


/**
 * C'Tor
 *
 * \param	savegame	Save game filename to load.
 */
MapViewState::MapViewState(const std::string& savegame) :
	mBackground("sys/bg1.png"),
	mUiIcons("ui/icons.png"),
	mLoadingExisting(true),
	mExistingToLoad(savegame)
{
	ccLocation() = CcNotPlaced;
	Utility<EventHandler>::get().windowResized().connect(this, &MapViewState::onWindowResized);
}


/**
 * C'Tor
 * 
 * \param	sm	Site map to load.
 * \param	t	Tileset to use.
 * \param	d	Depth of the site map.
 * \param	mc	Mine Count - Number of mines to generate.
 */
MapViewState::MapViewState(const std::string& siteMap, const std::string& tileSet, int depth, int mineCount, constants::PlanetHostility hostility) :
	mTileMap(new TileMap(siteMap, tileSet, depth, mineCount, hostility)),
	mBackground("sys/bg1.png"),
	mMapDisplay(siteMap + MAP_DISPLAY_EXTENSION),
	mHeightMap(siteMap + MAP_TERRAIN_EXTENSION),
	mUiIcons("ui/icons.png")
{
	ccLocation() = CcNotPlaced;
	Utility<EventHandler>::get().windowResized().connect(this, &MapViewState::onWindowResized);
}


/**
 * D'Tor
 */
MapViewState::~MapViewState()
{
	scrubRobotList();
	delete mTileMap;

	Utility<Renderer>::get().setCursor(PointerType::POINTER_NORMAL);

	EventHandler& e = Utility<EventHandler>::get();
	e.activate().disconnect(this, &MapViewState::onActivate);
	e.keyDown().disconnect(this, &MapViewState::onKeyDown);
	e.mouseButtonDown().disconnect(this, &MapViewState::onMouseDown);
	e.mouseButtonUp().disconnect(this, &MapViewState::onMouseUp);
	e.mouseDoubleClick().disconnect(this, &MapViewState::onMouseDoubleClick);
	e.mouseMotion().disconnect(this, &MapViewState::onMouseMove);
	e.mouseWheel().disconnect(this, &MapViewState::onMouseWheel);
	e.windowResized().disconnect(this, &MapViewState::onWindowResized);

	e.textInputMode(false);
}


/**
 * 
 */
void MapViewState::setPopulationLevel(PopulationLevel popLevel)
{
	mLandersColonist = static_cast<int>(popLevel);
	mLandersCargo = 2; ///\todo This should be set based on difficulty level.
}


/**
 * Initialize values, the UI and set up event handling.
 */
void MapViewState::initialize()
{
	// UI
	initUi();
	auto& renderer = Utility<Renderer>::get();

	renderer.setCursor(PointerType::POINTER_NORMAL);

	setupUiPositions(renderer.size());

	mPlayerResources.capacity(constants::BASE_STORAGE_CAPACITY);

	CURRENT_LEVEL_STRING = constants::LEVEL_SURFACE;

	mPopulationPool.population(&mPopulation);

	if (mLoadingExisting) { load(mExistingToLoad); }

	//Utility<Mixer>::get().fadeInMusic(mBgMusic);
	Utility<Renderer>::get().fadeIn(constants::FADE_SPEED);

	EventHandler& e = Utility<EventHandler>::get();

	e.activate().connect(this, &MapViewState::onActivate);
	e.keyDown().connect(this, &MapViewState::onKeyDown);
	e.mouseButtonDown().connect(this, &MapViewState::onMouseDown);
	e.mouseButtonUp().connect(this, &MapViewState::onMouseUp);
	e.mouseDoubleClick().connect(this, &MapViewState::onMouseDoubleClick);
	e.mouseMotion().connect(this, &MapViewState::onMouseMove);
	e.mouseWheel().connect(this, &MapViewState::onMouseWheel);

	e.textInputMode(true);

	MAIN_FONT = Utility<FontManager>::get().font(constants::FONT_PRIMARY, constants::FONT_PRIMARY_NORMAL);

	delete pather;
	pather = new MicroPather(mTileMap);
	path.clear();
}


void MapViewState::_activate()
{
	unhideUi();
}


void MapViewState::_deactivate()
{
	mGameOverDialog.enabled(false);
	mGameOptionsDialog.enabled(false);

	hideUi();
}


void MapViewState::focusOnStructure(Structure* s)
{
	if (!s) { return; }
	mTileMap->centerMapOnTile(Utility<StructureManager>::get().tileFromStructure(s));
}


/**
 * Updates the entire state of the game.
 */
State* MapViewState::update()
{
	auto& renderer = Utility<Renderer>::get();

	// Game's over, don't bother drawing anything else
	if (mGameOverDialog.visible())
	{
		renderer.drawBoxFilled(0, 0, renderer.width(), renderer.height(), 0, 0, 0, 255);
		mGameOverDialog.update();

		return this;
	}

	renderer.drawImageStretched(mBackground, 0, 0, renderer.width(), renderer.height());

	// explicit current level
	Font* font = Utility<FontManager>::get().font(constants::FONT_PRIMARY_BOLD, constants::FONT_PRIMARY_MEDIUM);
	renderer.drawText(*font, CURRENT_LEVEL_STRING, renderer.width() - font->width(CURRENT_LEVEL_STRING) - 5, mMiniMapBoundingBox.y() - font->height() - 12, 255, 255, 255);
	
	if (!modalUiElementDisplayed())
	{
		mTileMap->injectMouse(MOUSE_COORDS.x(), MOUSE_COORDS.y());
	}

	mTileMap->draw();

	// FIXME: Ugly / hacky
	if (modalUiElementDisplayed())
	{
		renderer.drawBoxFilled(0, 0, renderer.width(), renderer.height(), 0, 0, 0, 165);
	}

	drawUI();

	return this;
}


/**
 * Convenience function to get the amount of food currently in storage.
 */
int MapViewState::foodInStorage()
{
	int food_count = 0;

	const auto& structures = Utility<StructureManager>::get().structureList(Structure::StructureClass::CLASS_FOOD_PRODUCTION);

	for (auto structure : structures)
	{
		if (structure->operational() || structure->isIdle())
		{
			food_count += structure->storage().food();
		}
	}

	food_count += mPlayerResources.food();

	return food_count;
}


/**
 * Convenience function to get the total amount of food storage.
 */
int MapViewState::foodTotalStorage()
{
	int food_storage = 0;

	// Command Center has a limited amount of food storage for when colonists first land.
	if (ccLocation() != CcNotPlaced)
	{
		food_storage += constants::BASE_STORAGE_CAPACITY;
	}

	const auto& structures = Utility<StructureManager>::get().structureList(Structure::StructureClass::CLASS_FOOD_PRODUCTION);
	for (auto structure : structures)
	{
		if (structure->operational() || structure->isIdle())
		{
			food_storage += AGRIDOME_CAPACITY;
		}
	}

	return food_storage;
}


/**
 * Window activation handler.
 */
void MapViewState::onActivate(bool /*newActiveValue*/)
{
	mLeftButtonDown = false;
}


/**
 *
 */
void MapViewState::onWindowResized(int w, int h)
{
	setupUiPositions({w, h});
	mTileMap->initMapDrawParams({w, h});
}


/**
 * Key down event handler.
 */
void MapViewState::onKeyDown(EventHandler::KeyCode key, EventHandler::KeyModifier mod, bool /*repeat*/)
{
	if (!active()) { return; }

	// FIXME: Ugly / hacky
	if (modalUiElementDisplayed())
	{
		return;
	}

	if (key == EventHandler::KeyCode::KEY_F1)
	{
		mReportsUiCallback();
		return;
	}

	bool viewUpdated = false; // don't like flaggy code like this
	Point<int> pt = mTileMap->mapViewLocation();

	switch(key)
	{
		case EventHandler::KeyCode::KEY_w:
		case EventHandler::KeyCode::KEY_UP:
			viewUpdated = true;
			pt += DirectionNorth;
			break;

		case EventHandler::KeyCode::KEY_s:
		case EventHandler::KeyCode::KEY_DOWN:
			viewUpdated = true;
			pt += DirectionSouth;
			break;

		case EventHandler::KeyCode::KEY_a:
		case EventHandler::KeyCode::KEY_LEFT:
			viewUpdated = true;
			pt += DirectionWest;
			break;

		case EventHandler::KeyCode::KEY_d:
		case EventHandler::KeyCode::KEY_RIGHT:
			viewUpdated = true;
			pt += DirectionEast;
			break;

		case EventHandler::KeyCode::KEY_0:
			viewUpdated = true;
			changeViewDepth(0);
			break;

		case EventHandler::KeyCode::KEY_1:
			viewUpdated = true;
			changeViewDepth(1);
			break;

		case EventHandler::KeyCode::KEY_2:
			viewUpdated = true;
			changeViewDepth(2);
			break;

		case EventHandler::KeyCode::KEY_3:
			viewUpdated = true;
			changeViewDepth(3);
			break;

		case EventHandler::KeyCode::KEY_4:
			viewUpdated = true;
			changeViewDepth(4);
			break;

		case EventHandler::KeyCode::KEY_PAGEUP:
			viewUpdated = true;
			changeViewDepth(mTileMap->currentDepth() - 1);
			break;

		case EventHandler::KeyCode::KEY_PAGEDOWN:
			viewUpdated = true;
			changeViewDepth(mTileMap->currentDepth() + 1);
			break;


		case EventHandler::KeyCode::KEY_HOME:
			viewUpdated = true;
			changeViewDepth(0);
			break;

		case EventHandler::KeyCode::KEY_END:
			viewUpdated = true;
			changeViewDepth(mTileMap->maxDepth());
			break;

		case EventHandler::KeyCode::KEY_F10:
			if (Utility<EventHandler>::get().control(mod) && Utility<EventHandler>::get().shift(mod))
			{
				mPlayerResources.pushResource(ResourcePool::ResourceType::RESOURCE_COMMON_METALS, 1000, true);
				mPlayerResources.pushResource(ResourcePool::ResourceType::RESOURCE_COMMON_MINERALS, 1000, true);
				mPlayerResources.pushResource(ResourcePool::ResourceType::RESOURCE_RARE_METALS, 1000, true);
				mPlayerResources.pushResource(ResourcePool::ResourceType::RESOURCE_RARE_MINERALS, 1000, true);
			}
			break;

		case EventHandler::KeyCode::KEY_F2:
			mFileIoDialog.scanDirectory(constants::SAVE_GAME_PATH);
			mFileIoDialog.setMode(FileIo::FileOperation::FILE_SAVE);
			mFileIoDialog.show();
			break;

		case EventHandler::KeyCode::KEY_F3:
			mFileIoDialog.scanDirectory(constants::SAVE_GAME_PATH);
			mFileIoDialog.setMode(FileIo::FileOperation::FILE_LOAD);
			mFileIoDialog.show();
			break;

		case EventHandler::KeyCode::KEY_ESCAPE:
			clearMode();
			resetUi();
			break;

		case EventHandler::KeyCode::KEY_ENTER:
			if (mBtnTurns.enabled()) { nextTurn(); }
			break;

		default:
			break;
	}

	if (viewUpdated)
	{
		mTileMap->mapViewLocation(pt);
	}
}


/**
 * Mouse Down event handler.
 */
void MapViewState::onMouseDown(EventHandler::MouseButton button, int /*x*/, int /*y*/)
{
	if (!active()) { return; }

	if (modalUiElementDisplayed()) { return; }

	if (mWindowStack.pointInWindow(MOUSE_COORDS))
	{
		mWindowStack.updateStack(MOUSE_COORDS);
		return;
	}

	if (button == EventHandler::MouseButton::BUTTON_RIGHT)
	{
		if (mInsertMode != InsertMode::INSERT_NONE)
		{
			resetUi();
			return;
		}

		if (!mTileMap->tileHighlightVisible()) { return; }

		Tile* _t = mTileMap->getTile(mTileMap->tileMouseHover());
		if (!_t)
		{
			return;
		}
		else if (_t->empty() && mTileMap->boundingBox().contains(MOUSE_COORDS))
		{
			clearSelections();
			mTileInspector.tile(_t);
			mTileInspector.show();
			mWindowStack.bringToFront(&mTileInspector);
		}
		else if (_t->thingIsStructure())
		{
			Structure* structure = _t->structure();

			if (structure->isFactory() && (structure->operational() || structure->isIdle()))
			{
				mFactoryProduction.factory(static_cast<Factory*>(structure));
				mFactoryProduction.show();
				mWindowStack.bringToFront(&mFactoryProduction);
			}
			else if (structure->isWarehouse() && (structure->operational() || structure->isIdle()))
			{
				mWarehouseInspector.warehouse(static_cast<Warehouse*>(structure));
				mWarehouseInspector.show();
				mWindowStack.bringToFront(&mWarehouseInspector);
			}
			else if (structure->isMineFacility() && (structure->operational() || structure->isIdle()))
			{
				mMineOperationsWindow.mineFacility(static_cast<MineFacility*>(structure));
				mMineOperationsWindow.show();
				mWindowStack.bringToFront(&mMineOperationsWindow);
			}
			else
			{
				mStructureInspector.structure(structure);
				mStructureInspector.show();
				mWindowStack.bringToFront(&mStructureInspector);
			}
		}
	}

	if (button == EventHandler::MouseButton::BUTTON_LEFT)
	{
		mLeftButtonDown = true;

		Point<int> pt = mTileMap->mapViewLocation();

		if (MENU_ICON.contains(MOUSE_COORDS))
		{
			mGameOptionsDialog.show();
			resetUi();
			return;
		}

		if (RESOURCE_PANEL_PIN.contains(MOUSE_COORDS)) { mPinResourcePanel = !mPinResourcePanel; }
		if (POPULATION_PANEL_PIN.contains(MOUSE_COORDS)) { mPinPopulationPanel = !mPinPopulationPanel; }

		if (MOVE_NORTH_ICON.contains(MOUSE_COORDS))
		{
			mTileMap->mapViewLocation(pt + DirectionNorth);
		}
		else if (MOVE_SOUTH_ICON.contains(MOUSE_COORDS))
		{
			mTileMap->mapViewLocation(pt + DirectionSouth);
		}
		else if (MOVE_EAST_ICON.contains(MOUSE_COORDS))
		{
			mTileMap->mapViewLocation(pt + DirectionEast);
		}
		else if (MOVE_WEST_ICON.contains(MOUSE_COORDS))
		{
			mTileMap->mapViewLocation(pt + DirectionWest);
		}
		else if (MOVE_UP_ICON.contains(MOUSE_COORDS))
		{
			changeViewDepth(mTileMap->currentDepth() - 1);
		}
		else if (MOVE_DOWN_ICON.contains(MOUSE_COORDS))
		{
			changeViewDepth(mTileMap->currentDepth()+1);
		}

		// MiniMap Check
		if (mMiniMapBoundingBox.contains(MOUSE_COORDS) && !mWindowStack.pointInWindow(MOUSE_COORDS))
		{
			setMinimapView();
		}
		// Click was within the bounds of the TileMap.
		else if (mTileMap->boundingBox().contains(MOUSE_COORDS))
		{
			EventHandler& e = Utility<EventHandler>::get();
			if (mInsertMode == InsertMode::INSERT_STRUCTURE)
			{
				placeStructure();
			}
			else if (mInsertMode == InsertMode::INSERT_ROBOT)
			{
				placeRobot();
			}
			else if ( (mInsertMode == InsertMode::INSERT_TUBE) && e.query_shift())
			{
				placeTubeStart();
			}
			else if (mInsertMode == InsertMode::INSERT_TUBE)
			{
				placeTubes();
			}
		}
	}
}


void MapViewState::onMouseDoubleClick(EventHandler::MouseButton button, int /*x*/, int /*y*/)
{
	if (!active()) { return; }

	if (button == EventHandler::MouseButton::BUTTON_LEFT)
	{
		if (mWindowStack.pointInWindow(MOUSE_COORDS)) { return; }
		if (!mTileMap->tileHighlightVisible()) { return; }

		Tile* _t = mTileMap->getTile(mTileMap->tileMouseHover());
		if (_t && _t->thingIsStructure())
		{
			Structure* structure = _t->structure();

			if (structure->isFactory()) { MAIN_REPORTS_UI->selectFactoryPanel(structure); }
			else if (structure->isWarehouse()) { MAIN_REPORTS_UI->selectWarehousePanel(structure); }
			else if (structure->isMineFacility() || structure->structureClass() == Structure::StructureClass::CLASS_SMELTER) { MAIN_REPORTS_UI->selectMinePanel(structure); }
			else { return; } // avoids showing the full-screen UI on unhandled structures.

			mReportsUiCallback();
		}
	}
}


/**
* Mouse Up event handler.
*/
void MapViewState::onMouseUp(EventHandler::MouseButton button, int /*x*/, int /*y*/)
{
	if (button == EventHandler::MouseButton::BUTTON_LEFT)
	{
		mLeftButtonDown = false;
		EventHandler& e = Utility<EventHandler>::get();
		if ((mInsertMode == InsertMode::INSERT_TUBE) && e.query_shift())
		{
			placeTubeEnd();
		}
	}
}


/**
* Mouse motion event handler.
*/
void MapViewState::onMouseMove(int /*x*/, int /*y*/, int /*rX*/, int /*rY*/)
{
	if (!active()) { return; }


	if (mLeftButtonDown)
	{
		if (mMiniMapBoundingBox.contains(MOUSE_COORDS))
		{
			setMinimapView();
		}
	}

	mTileMapMouseHover = mTileMap->tileMouseHover();
}


/**
 * Mouse wheel event handler.
 */
void MapViewState::onMouseWheel(int /*x*/, int y)
{
	if (mInsertMode != InsertMode::INSERT_TUBE) { return; }

	y > 0 ? mConnections.decrementSelection() : mConnections.incrementSelection();
}


/**
 * Changes the current view depth.
 */
void MapViewState::changeViewDepth(int depth)
{
	mTileMap->currentDepth(depth);

	if (mInsertMode != InsertMode::INSERT_ROBOT) { clearMode(); }
	populateStructureMenu();
	updateCurrentLevelString(mTileMap->currentDepth());
}


/**
 * 
 */
void MapViewState::setMinimapView()
{
	int x = MOUSE_COORDS.x() - mMiniMapBoundingBox.x() - mTileMap->edgeLength() / 2;
	int y = MOUSE_COORDS.y() - mMiniMapBoundingBox.y() - mTileMap->edgeLength() / 2;

	mTileMap->mapViewLocation({x, y});
}


/**
 * Clears the build mode.
 */
void MapViewState::clearMode()
{
	mInsertMode = InsertMode::INSERT_NONE;
	Utility<Renderer>::get().setCursor(PointerType::POINTER_NORMAL);

	mCurrentStructure = StructureID::SID_NONE;
	mCurrentRobot = RobotType::ROBOT_NONE;

	clearSelections();
}


/**
 * 
 */
void MapViewState::insertTube(ConnectorDir dir, int depth, Tile* tile)
{

	if (dir == ConnectorDir::CONNECTOR_VERTICAL)
	{
		throw std::runtime_error("MapViewState::insertTube() called with invalid ConnectorDir paramter.");
	}

	Utility<StructureManager>::get().addStructure(new Tube(dir, depth != 0), tile);
}


/**
 * 
 */
void MapViewState::placeTubes()
{
	Tile* tile = mTileMap->getVisibleTile(mTileMapMouseHover, mTileMap->currentDepth());
	if (!tile) { return; }

	// Check the basics.
	if (tile->thing() || tile->mine() || !tile->bulldozed() || !tile->excavated()) { return; }

	/** \fixme	This is a kludge that only works because all of the tube structures are listed alphabetically.
	 *			Should instead take advantage of the updated meta data in the IconGridItem.
	 */
	auto cd = static_cast<ConnectorDir>(mConnections.selectionIndex() + 1);

	if (validTubeConnection(mTileMap, mTileMapMouseHover.x(), mTileMapMouseHover.y(), cd))
	{
		insertTube(cd, mTileMap->currentDepth(), mTileMap->getTile(mTileMapMouseHover));

		// FIXME: Naive approach -- will be slow with larger colonies.
		Utility<StructureManager>::get().disconnectAll();
		checkConnectedness();
	}
	else
	{
		doAlertMessage(constants::ALERT_INVALID_STRUCTURE_ACTION, constants::ALERT_TUBE_INVALID_LOCATION);
	}
}

/**
 *
 */
void MapViewState::placeTubeStart()
{
	tubeStart.height(0);
	int x = mTileMapMouseHover.x();
	int y = mTileMapMouseHover.y();

	Tile* tile = mTileMap->getVisibleTile(mTileMapMouseHover, mTileMap->currentDepth());
	if (!tile) { return; }

	// Check the basics.
	if (tile->thing() || tile->mine() || !tile->bulldozed() || !tile->excavated()) { return; }

	/** \fixme	This is a kludge that only works because all of the tube structures are listed alphabetically.
	 *			Should instead take advantage of the updated meta data in the IconGridItem.
	 */
	ConnectorDir cd = static_cast<ConnectorDir>(mConnections.selectionIndex() + 1);

	if (!validTubeConnection(mTileMap, x, y, cd))
	{
		doAlertMessage(constants::ALERT_INVALID_STRUCTURE_ACTION, constants::ALERT_TUBE_INVALID_LOCATION);
		return;
	}
	tubeStart.x(tile->x());
	tubeStart.y(tile->y());
	tubeStart.height(1);
}

/**
 *
 */
void MapViewState::placeTubeEnd()
{
	int x = mTileMapMouseHover.x();
	int y = mTileMapMouseHover.y();
	int incX = 0; int incY = 0;
	int xEnd = 0; int yEnd = 0;
	bool endReach = false;
	if (tubeStart.height() != 1) return;
	tubeStart.height(0); // the height is used as a boolean to indicate that we are
	Tile* tile = mTileMap->getVisibleTile(mTileMapMouseHover, mTileMap->currentDepth());
	if (!tile) { return; }

	/** \fixme	This is a kludge that only works because all of the tube structures are listed alphabetically.
	 *			Should instead take advantage of the updated meta data in the IconGridItem.
	 */
	ConnectorDir cd = static_cast<ConnectorDir>(mConnections.selectionIndex() + 1);

	switch (cd)
	{
	case ConnectorDir::CONNECTOR_INTERSECTION:

		if (abs(tubeStart.x() - tile->x()) >= abs(tubeStart.y() - tile->y())){
			incX = 1; // The sens will be on the longest spread on X or Y
		}else{
			incY = 1;
		}
		break;
	case ConnectorDir::CONNECTOR_RIGHT:
		incX = 1;
		break;
	case ConnectorDir::CONNECTOR_LEFT:
		incY = 1;
		break;
	default:
		return;
	}
	x = tubeStart.x();
	y = tubeStart.y();
	xEnd = tile->x();
	yEnd = tile->y();

	if (tubeStart.x() > tile->x())
	{
		incX = -incX;
		yEnd = tubeStart.y();
	}
	if (tubeStart.y() > tile->y())
	{
		incY = -incY;
		xEnd = tubeStart.x();
	}

	// 
	do {
		std::cout << "Tube " << x << "/" << y << std::endl;
		tile = mTileMap->getVisibleTile(tubeStart.startPoint(), mTileMap->currentDepth());
		if (!tile) {
			endReach = true;
		}else if (tile->thing() || tile->mine() || !tile->bulldozed() || !tile->excavated()){
			endReach = true;
		}else if (!validTubeConnection(mTileMap, x, y, cd)){
			endReach = true;
		}else{
			insertTube(cd, mTileMap->currentDepth(), mTileMap->getTile({x, y}));
			
			// FIXME: Naive approach -- will be slow with larger colonies.
			Utility<StructureManager>::get().disconnectAll();
			checkConnectedness();
		}

		if (y == yEnd && x == xEnd) endReach = true;
		x += incX;y += incY;
	} while (!endReach);
	
}

/**
 * 
 */
void MapViewState::placeRobot()
{
	Tile* tile = mTileMap->getVisibleTile();
	if (!tile) { return; }
	if (!mRobotPool.robotCtrlAvailable()) { return; }
	
	if (outOfCommRange(ccLocation(), mTileMap, tile))
	{
		doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_OUT_OF_COMM_RANGE);
		return;
	}

	if(mCurrentRobot == RobotType::ROBOT_DOZER)
	{
		Robot* robot = mRobotPool.getDozer();

		if (!tile->excavated() || (tile->thing() && !tile->thingIsStructure()))
		{
			return;
		}
		else if (tile->index() == TerrainType::TERRAIN_DOZED)
		{
			doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_TILE_BULLDOZED);
			return;
		}
		else if (tile->mine())
		{
			if (tile->mine()->depth() != mTileMap->maxDepth() || !tile->mine()->exhausted())
			{
				doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_MINE_NOT_EXHAUSTED);
				return;
			}

			mMineOperationsWindow.hide();
			mTileMap->removeMineLocation(mTileMap->tileMouseHover());
			tile->pushMine(nullptr);
			for (std::size_t i = 0; i <= static_cast<std::size_t>(mTileMap->maxDepth()); ++i)
			{
				Tile* _t = mTileMap->getTile(mTileMap->tileMouseHover(), static_cast<int>(i));
				Utility<StructureManager>::get().removeStructure(_t->structure());
			}
		}
		else if (tile->thingIsStructure())
		{
			if (mStructureInspector.structure() == tile->structure()) { mStructureInspector.hide(); }
			
			Structure* structure = tile->structure();

			if (structure->isMineFacility()) { return; }
			if (structure->structureClass() == Structure::StructureClass::CLASS_COMMAND)
			{
				doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_CANNOT_BULLDOZE_CC);
				return;
			}

			if (structure->structureClass() == Structure::StructureClass::CLASS_LANDER && structure->age() == 0)
			{
				doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_CANNOT_BULLDOZE_LANDING_SITE);
				return;
			}

			if (structure->isRobotCommand())
			{
				deleteRobotsInRCC(robot, static_cast<RobotCommand*>(structure), mRobotPool, mRobotList, tile);
			}

			if (structure->isFactory() && static_cast<Factory*>(structure) == mFactoryProduction.factory())
			{
				mFactoryProduction.hide();
			}

			if (structure->isWarehouse())
			{
				if (simulateMoveProducts(static_cast<Warehouse*>(structure))) { moveProducts(static_cast<Warehouse*>(structure)); }
				else { return; }
			}

			/**
			 * \fixme	Since the StructureTranslator class will be deprecated in the future, there needs to be a better
			 * 			way to determine this. I may go back to defining recycling values in the individual structures
			 * 			themselves but I'm still not sure I love that idea. Will have to think about that one a bit.
			 */
			ResourcePool resPool = StructureCatalogue::recyclingValue(StructureTranslator::translateFromString(structure->name()));
			mPlayerResources.pushResources(resPool);

			tile->connected(false);
			Utility<StructureManager>::get().removeStructure(structure);
			tile->deleteThing();
			Utility<StructureManager>::get().disconnectAll();
			static_cast<Robodozer*>(robot)->tileIndex(static_cast<std::size_t>(TerrainType::TERRAIN_DOZED));
			checkConnectedness();
		}

		int taskTime = tile->index() == 0 ? 1 : tile->index(); 
		robot->startTask(taskTime);
		mRobotPool.insertRobotIntoTable(mRobotList, robot, tile);
		static_cast<Robodozer*>(robot)->tileIndex(static_cast<std::size_t>(tile->index()));
		tile->index(TerrainType::TERRAIN_DOZED);

		if(!mRobotPool.robotAvailable(RobotType::ROBOT_DOZER))
		{
			mRobots.removeItem(constants::ROBODOZER);
			clearMode();
		}
	}
	else if(mCurrentRobot == RobotType::ROBOT_DIGGER)
	{
		// Keep digger within a safe margin of the map boundaries.
		if (mTileMapMouseHover.x() < 3 || mTileMapMouseHover.x() > mTileMap->width() - 4 || mTileMapMouseHover.y() < 3 || mTileMapMouseHover.y() > mTileMap->height() - 4)
		{
			doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_DIGGER_EDGE_BUFFER);
			return;
		}

		if (!tile->excavated()) { return; }

		// Check for obstructions underneath the the digger location.
		if (tile->depth() != mTileMap->maxDepth() && !mTileMap->getTile(tile->position(), tile->depth() + 1)->empty())
		{
			doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_DIGGER_BLOCKED_BELOW);
			return;
		}

		if (tile->hasMine())
		{
			if (!doYesNoMessage(constants::ALERT_DIGGER_MINE_TITLE, constants::ALERT_DIGGER_MINE)) { return; }

			const auto position = tile->position();
			std::cout << "Digger destroyed a Mine at (" << position.x() << ", " << position.y() << ")." << std::endl;
			mTileMap->removeMineLocation(position);
		}

		// Die if tile is occupied or not excavated.
		if (!tile->empty())
		{
			if (tile->depth() > constants::DEPTH_SURFACE)
			{
				if (tile->thingIsStructure() && tile->structure()->connectorDirection() != ConnectorDir::CONNECTOR_VERTICAL) //air shaft
				{
					doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_STRUCTURE_IN_WAY);
					return;
				}
				else if (tile->thingIsStructure() && tile->structure()->connectorDirection() == ConnectorDir::CONNECTOR_VERTICAL && tile->depth() == mTileMap->maxDepth())
				{
					doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_MAX_DIG_DEPTH);
					return;
				}
			}
			else
			{
				doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_STRUCTURE_IN_WAY);
				return;
			}
		}

		if (!tile->thing() && mTileMap->currentDepth() > 0) { mDiggerDirection.cardinalOnlyEnabled(); }
		else { mDiggerDirection.downOnlyEnabled(); }

		mDiggerDirection.setParameters(tile);

		// NOTE:	Unlike the Dozer and Miner, Digger's aren't removed here but instead
		//			are removed after responses to the DiggerDirection dialog.

		// If we're placing on the top level we can only ever go down.
		if (mTileMap->currentDepth() == constants::DEPTH_SURFACE)
		{
			mDiggerDirection.selectDown();
		}
		else
		{
			mDiggerDirection.show();
			mWindowStack.bringToFront(&mDiggerDirection);

			int x = MOUSE_COORDS.x() + 20;

			if (x + mDiggerDirection.width() > Utility<Renderer>::get().width())
			{
				x = MOUSE_COORDS.x() - mDiggerDirection.width() - 20;
			}

			mDiggerDirection.position(x, MOUSE_COORDS.y() - 32);
		}
	}
	else if(mCurrentRobot == RobotType::ROBOT_MINER)
	{
		if (tile->thing()) { doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_MINER_TILE_OBSTRUCTED); return; }
		if (mTileMap->currentDepth() != constants::DEPTH_SURFACE) { doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_MINER_SURFACE_ONLY); return; }
		if (!tile->mine()) { doAlertMessage(constants::ALERT_INVALID_ROBOT_PLACEMENT, constants::ALERT_MINER_NOT_ON_MINE); return; }

		Robot* robot = mRobotPool.getMiner();
		robot->startTask(constants::MINER_TASK_TIME);
		mRobotPool.insertRobotIntoTable(mRobotList, robot, tile);
		tile->index(TerrainType::TERRAIN_DOZED);

		if (!mRobotPool.robotAvailable(RobotType::ROBOT_MINER))
		{
			mRobots.removeItem(constants::ROBOMINER);
			clearMode();
		}
	}
}


/**
 * Checks the robot selection interface and if the robot is not available in it, adds
 * it back in.
 */
void MapViewState::checkRobotSelectionInterface(const std::string& rType, int sheetIndex, RobotType _rid)
{
	if (!mRobots.itemExists(rType))
	{
		mRobots.addItemSorted(rType, sheetIndex, _rid);
	}
}


/**
 * Places a structure into the map.
 */
void MapViewState::placeStructure()
{
	// StructureID::SID_NONE is a logic error and should fail as loudly as possible.
	if (mCurrentStructure == StructureID::SID_NONE) { throw std::runtime_error("MapViewState::placeStructure() called but mCurrentStructure == STRUCTURE_NONE"); }

	Tile* tile = mTileMap->getVisibleTile();
	if (!tile) { return; }

	// NOTE:	This function will never be called until the seed lander is deployed so there
	//			is no need to check that the CC Location is anything other than { 0, 0 }.
	if (!structureIsLander(mCurrentStructure) && !selfSustained(mCurrentStructure) &&
		(tile->distanceTo(mTileMap->getTile(ccLocation(), 0)) > constants::ROBOT_COM_RANGE))
	{
		doAlertMessage(constants::ALERT_INVALID_STRUCTURE_ACTION, constants::ALERT_STRUCTURE_OUT_OF_RANGE);
		return;
	}

	if (tile->mine())
	{
		doAlertMessage(constants::ALERT_INVALID_STRUCTURE_ACTION, constants::ALERT_STRUCTURE_MINE_IN_WAY);
		return;
	}
	
	if (tile->thing())
	{
		if (tile->thingIsStructure())
		{
			doAlertMessage(constants::ALERT_INVALID_STRUCTURE_ACTION, constants::ALERT_STRUCTURE_TILE_OBSTRUCTED);
		}
		else
		{
			doAlertMessage(constants::ALERT_INVALID_STRUCTURE_ACTION, constants::ALERT_STRUCTURE_TILE_THING);
		}
		return;
	}

	if((!tile->bulldozed() && !structureIsLander(mCurrentStructure)))
	{
		doAlertMessage(constants::ALERT_INVALID_STRUCTURE_ACTION, constants::ALERT_STRUCTURE_TERRAIN);
		return;
	}

	if (!tile->excavated())
	{
		doAlertMessage(constants::ALERT_INVALID_STRUCTURE_ACTION, constants::ALERT_STRUCTURE_EXCAVATED);
		return;
	}

	int tile_x = mTileMapMouseHover.x(), tile_y = mTileMapMouseHover.y();

	// Seed lander is a special case and only one can ever be placed by the player ever.
	if(mCurrentStructure == StructureID::SID_SEED_LANDER)
	{
		insertSeedLander(tile_x, tile_y);
	}
	else if (mCurrentStructure == StructureID::SID_COLONIST_LANDER)
	{
		if (!validLanderSite(tile)) { return; }

		ColonistLander* s = new ColonistLander(tile);
		s->deployCallback().connect(this, &MapViewState::deployColonistLander);
		Utility<StructureManager>::get().addStructure(s, tile);

		--mLandersColonist;
		if (mLandersColonist == 0)
		{
			clearMode();
			resetUi();
			populateStructureMenu();
		}
	}
	else if (mCurrentStructure == StructureID::SID_CARGO_LANDER)
	{
		if (!validLanderSite(tile)) { return; }

		CargoLander* _lander = new CargoLander(tile);
		_lander->deployCallback().connect(this, &MapViewState::deployCargoLander);
		Utility<StructureManager>::get().addStructure(_lander, tile);

		--mLandersCargo;
		if (mLandersCargo == 0)
		{
			clearMode();
			resetUi();
			populateStructureMenu();
		}
	}
	else
	{
		if (!validStructurePlacement(mTileMap, tile_x, tile_y) && !selfSustained(mCurrentStructure))
		{
			doAlertMessage(constants::ALERT_INVALID_STRUCTURE_ACTION, constants::ALERT_STRUCTURE_NO_TUBE);
			return;
		}

		// Check build cost
		if (!StructureCatalogue::canBuild(mPlayerResources, mCurrentStructure))
		{
			resourceShortageMessage(mPlayerResources, mCurrentStructure);
			return;
		}

		Structure* structure = StructureCatalogue::get(mCurrentStructure);
		if (!structure) { throw std::runtime_error("MapViewState::placeStructure(): NULL Structure returned from StructureCatalog."); }

		Utility<StructureManager>::get().addStructure(structure, tile);

		// FIXME: Ugly
		if (structure->isFactory())
		{
			static_cast<Factory*>(structure)->productionComplete().connect(this, &MapViewState::factoryProductionComplete);
			static_cast<Factory*>(structure)->resourcePool(&mPlayerResources);
		}

		mPlayerResources -= StructureCatalogue::costToBuild(mCurrentStructure);
	}
}


/**
 * Checks that the clicked tile is a suitable spot for the SEED Lander and
 * then inserts it into the the TileMap.
 */
void MapViewState::insertSeedLander(int x, int y)
{
	// Has to be built away from the edges of the map
	if (x > 3 && x < mTileMap->width() - 4 && y > 3 && y < mTileMap->height() - 4)
	{
		// check for obstructions
		if (!landingSiteSuitable(mTileMap, x, y))
		{
			return;
		}

		SeedLander* s = new SeedLander(x, y);
		s->deployCallback().connect(this, &MapViewState::deploySeedLander);
		Utility<StructureManager>::get().addStructure(s, mTileMap->getTile({x, y})); // Can only ever be placed on depth level 0

		clearMode();
		resetUi();

		mStructures.dropAllItems();
		mBtnTurns.enabled(true);
	}
	else
	{
		doAlertMessage(constants::ALERT_LANDER_LOCATION, constants::ALERT_SEED_EDGE_BUFFER);
	}
}


/**
 * Updates all robots.
 */
void MapViewState::updateRobots()
{
	auto robot_it = mRobotList.begin();
	while(robot_it != mRobotList.end())
	{
		robot_it->first->update();

		if (robot_it->first->dead())
		{
			std::cout << "dead robot" << std::endl;

			// \fixme	This is an awful way of doing this.
			if (robot_it->first->name() != constants::ROBOMINER)
			{
				const auto robotLocationText = std::to_string(robot_it->second->x()) + ", " + std::to_string(robot_it->second->y());
				const auto text = "Your " + robot_it->first->name() + " at location " + robotLocationText + " has broken down. It will not be able to complete its task and will be removed from your inventory.";
				doAlertMessage("Robot Breakdown", text);
				Robodozer* _d = dynamic_cast<Robodozer*>(robot_it->first);
				if (_d) { robot_it->second->index(static_cast<int>(_d->tileIndex())); }
			}

			if (robot_it->second->thing() == robot_it->first)
			{
				robot_it->second->removeThing();
			}

			/// \fixme	Brute force.
			for (auto rcc : Utility<StructureManager>::get().structureList(Structure::StructureClass::CLASS_ROBOT_COMMAND))
			{
				static_cast<RobotCommand*>(rcc)->removeRobot(robot_it->first);
			}

			mRobotPool.erase(robot_it->first);
			delete robot_it->first;
			robot_it = mRobotList.erase(robot_it);
		}
		else if(robot_it->first->idle())
		{
			if (robot_it->second->thing() == robot_it->first)
			{
				robot_it->second->removeThing();
			}

			robot_it = mRobotList.erase(robot_it);
		}
		else
		{
			++robot_it;
		}
	}

	updateRobotControl(mRobotPool);
}


/**
 * Checks and sets the current structure mode.
 */
void MapViewState::setStructureID(StructureID type, InsertMode mode)
{

	if (type == StructureID::SID_NONE)
	{
		clearMode();
		return;
	}

	mCurrentStructure = type;

	mInsertMode = mode;
	Utility<Renderer>::get().setCursor(PointerType::POINTER_PLACE_TILE);

}


/**
 * Checks the connectedness of all tiles surrounding
 * the Command Center.
 */
void MapViewState::checkConnectedness()
{
	if (ccLocation() == CcNotPlaced)
	{
		return;
	}

	// Assumes that the 'thing' at mCCLocation is in fact a Structure.
	Tile *t = mTileMap->getTile(ccLocation(), 0);
	Structure *cc = t->structure();

	if (!cc)
	{
		throw std::runtime_error("CC coordinates do not actually point to a Command Center.");
	}

	if (cc->state() == Structure::StructureState::UNDER_CONSTRUCTION)
	{
		return;
	}

	t->connected(true);

	// Start graph walking at the CC location.
	GraphWalker graphWalker(ccLocation(), 0, mTileMap);
}


/**
 * Removes deployed robots from the TileMap to
 * prevent dangling pointers. Yay for raw memory!
 */
void MapViewState::scrubRobotList()
{
	for (auto it : mRobotList)
	{
		it.second->removeThing();
	}
}


/**
 * Update the value of the current level string
 */
void MapViewState::updateCurrentLevelString(int currentDepth)
{
	CURRENT_LEVEL_STRING = LEVEL_STRING_TABLE[currentDepth];
}
