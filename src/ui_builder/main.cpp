#include "NAS2D/NAS2D.h"

#include "utility.h"

#include "../UI/UI.h"

#include <vector>


using namespace NAS2D;
using namespace std;


Point_2df MOUSE_POSITION;

bool running = true;


Renderer*	R = nullptr;


// Assets/Resources
Font*		TINY_FONT = nullptr;
Image*		MOUSE_POINTER = nullptr;

// Window to be worked on
Window*		WINDOW = nullptr;

// Editor UI
Menu*		CONTROLS_MENU = nullptr;
Button*		BTN_ADD_CONTROL = nullptr;
Control*	UI_CONTROL_EDIT = nullptr;		// Control currently being worked on.

// List of controls added to the Window.
vector<Control*>	CONTROL_LIST;


Rectangle_2d	WINDOW_HANDLE_TOP_L(0, 0, 10, 10);
Rectangle_2d	WINDOW_HANDLE_TOP_R(0, 0, 10, 10);
Rectangle_2d	WINDOW_HANDLE_BOT_L(0, 0, 10, 10);
Rectangle_2d	WINDOW_HANDLE_BOT_R(0, 0, 10, 10);


enum ResizeHandle
{
	RESIZE_TOP_LEFT,
	RESIZE_TOP_RIGHT,
	RESIZE_BOTTOM_LEFT,
	RESIZE_BOTTOM_RIGHT,
	RESIZE_NONE
};


bool		MOUSE_LEFT_DOWN = false;

ResizeHandle HANDLE = RESIZE_NONE;


ResizeHandle getHandle()
{
	// fixme: Kind of ugly.
	if (isPointInRect(MOUSE_POSITION, WINDOW_HANDLE_TOP_L))
		return RESIZE_TOP_LEFT;
	else if (isPointInRect(MOUSE_POSITION, WINDOW_HANDLE_TOP_R))
		return RESIZE_TOP_RIGHT;
	else if (isPointInRect(MOUSE_POSITION, WINDOW_HANDLE_BOT_L))
		return RESIZE_BOTTOM_LEFT;
	else if (isPointInRect(MOUSE_POSITION, WINDOW_HANDLE_BOT_R))
		return RESIZE_BOTTOM_RIGHT;

	return RESIZE_NONE;
}



bool mouseInHandles()
{
	return (WINDOW->hasFocus() &&
			(isPointInRect(MOUSE_POSITION, WINDOW_HANDLE_TOP_L) ||
			isPointInRect(MOUSE_POSITION, WINDOW_HANDLE_TOP_R) ||
			isPointInRect(MOUSE_POSITION, WINDOW_HANDLE_BOT_L) ||
			isPointInRect(MOUSE_POSITION, WINDOW_HANDLE_BOT_R)));
}


void onKeyDown(KeyCode key, KeyModifier mod, bool repeat)
{
	switch (key)
	{
	case KEY_ESCAPE:
		postQuitEvent();
		break;
	default:
		break;
	}
}


void resizeControl(Control* c, int dX, int dY)
{
	int x = c->positionX();
	int y = c->positionY();
	int width = c->width();
	int height = c->height();

	if (HANDLE == RESIZE_TOP_LEFT)
	{
		c->position(x + dX, y + dY);
		c->size(width - dX, height - dY);
	}

	if (HANDLE == RESIZE_TOP_RIGHT)
	{
		c->position(x, y + dY);
		c->size(width + dX, height - dY);
	}

	if (HANDLE == RESIZE_BOTTOM_LEFT)
	{
		c->position(x + dX, y);
		c->size(width - dX, height + dY);
	}

	if (HANDLE == RESIZE_BOTTOM_RIGHT)
	{
		c->size(width + dX, height + dY);
	}
}


void repositionControl(Control* c, int relX, int relY)
{
	c->position(c->positionX() + relX, c->positionY() + relY);
}


void onMouseMotion(int x, int y, int relX, int relY)
{
	MOUSE_POSITION(static_cast<float>(x), static_cast<float>(y));

	if (!UI_CONTROL_EDIT)
		return;

	if (MOUSE_LEFT_DOWN)
	{
		if (HANDLE != RESIZE_NONE)
			resizeControl(UI_CONTROL_EDIT, relX, relY);
		else
			repositionControl(UI_CONTROL_EDIT, relX, relY);
	}
}


void onMouseDown(MouseButton button, int x, int y)
{
	if (button == BUTTON_LEFT)
	{

		MOUSE_LEFT_DOWN = true;

		if (UI_CONTROL_EDIT && mouseInHandles())
		{
			HANDLE = getHandle();
			return;
		}
	
		UI_CONTROL_EDIT = nullptr;
		for (vector<Control*>::reverse_iterator it = CONTROL_LIST.rbegin(); it != CONTROL_LIST.rend(); ++it)
		{
			if (isPointInRect(MOUSE_POSITION, (*it)->rect()))
			{
				UI_CONTROL_EDIT = (*it);
				updateWindowHandlePositions(UI_CONTROL_EDIT, WINDOW_HANDLE_TOP_L, WINDOW_HANDLE_TOP_R, WINDOW_HANDLE_BOT_L, WINDOW_HANDLE_BOT_R);
				break;
			}
		}
	}
}


void onMouseUp(MouseButton button, int x, int y)
{
	if (button == BUTTON_LEFT)
	{
		MOUSE_LEFT_DOWN = false;
		HANDLE = RESIZE_NONE;
	}
}


void onQuit()
{
	running = false;
}


void initEventHandlers()
{
	cout << "Initializing event handlers... ";
	
	Utility<EventHandler>::get().keyDown().Connect(&onKeyDown);
	Utility<EventHandler>::get().mouseButtonDown().Connect(&onMouseDown);
	Utility<EventHandler>::get().mouseButtonUp().Connect(&onMouseUp);
	Utility<EventHandler>::get().mouseMotion().Connect(&onMouseMotion);
	Utility<EventHandler>::get().quit().Connect(&onQuit);

	cout << "done." << endl;
}


void drawHandles()
{
	R->drawBoxFilled(WINDOW_HANDLE_TOP_L, 255, 255, 255);
	R->drawBox(WINDOW_HANDLE_TOP_L, 0, 0, 0);

	R->drawBoxFilled(WINDOW_HANDLE_TOP_R, 255, 255, 255);
	R->drawBox(WINDOW_HANDLE_TOP_R, 0, 0, 0);

	R->drawBoxFilled(WINDOW_HANDLE_BOT_L, 255, 255, 255);
	R->drawBox(WINDOW_HANDLE_BOT_L, 0, 0, 0);

	R->drawBoxFilled(WINDOW_HANDLE_BOT_R, 255, 255, 255);
	R->drawBox(WINDOW_HANDLE_BOT_R, 0, 0, 0);
}


void updateHandles(float dX, float dY)
{
	updateRectPosition(WINDOW_HANDLE_TOP_L, static_cast<int>(dX), static_cast<int>(dY));
	updateRectPosition(WINDOW_HANDLE_TOP_R, static_cast<int>(dX), static_cast<int>(dY));
	updateRectPosition(WINDOW_HANDLE_BOT_L, static_cast<int>(dX), static_cast<int>(dY));
	updateRectPosition(WINDOW_HANDLE_BOT_R, static_cast<int>(dX), static_cast<int>(dY));
}


void controlResized(Control* c)
{
	updateWindowHandlePositions(c, WINDOW_HANDLE_TOP_L, WINDOW_HANDLE_TOP_R, WINDOW_HANDLE_BOT_L, WINDOW_HANDLE_BOT_R);
}


void addControlClicked()
{
	cout << "Adding control: " << CONTROLS_MENU->selectionText() << endl;

	if (CONTROLS_MENU->selectionText() == "Button")
	{
		Button* btn = new Button();
		btn->font(*TINY_FONT);
		btn->text("Button");
		btn->size(50, 20);
		
		WINDOW->addControl("Button", btn, 5, 25);
		CONTROL_LIST.push_back(btn);
		CONTROL_LIST.back()->resized().Connect(&controlResized);
		CONTROL_LIST.back()->moved().Connect(&updateHandles);
	}
}

void initUi()
{
	cout << "Setting up UI... ";

	WINDOW = new Window();
	WINDOW->size(250, 150);
	WINDOW->position(R->screenCenterX() - 125, R->screenCenterY() - 75);
	WINDOW->font(*TINY_FONT);
	WINDOW->text("Window Title");
	WINDOW->show();
	WINDOW->moved().Connect(&updateHandles);
	WINDOW->resized().Connect(&controlResized);
	WINDOW->anchored(true);

	CONTROL_LIST.push_back(WINDOW);

	updateWindowHandlePositions(WINDOW, WINDOW_HANDLE_TOP_L, WINDOW_HANDLE_TOP_R, WINDOW_HANDLE_BOT_L, WINDOW_HANDLE_BOT_R);

	CONTROLS_MENU = new Menu();
	CONTROLS_MENU->font(*TINY_FONT);

	CONTROLS_MENU->addItem("Button");
	CONTROLS_MENU->addItem("Image");
	CONTROLS_MENU->addItem("TextArea");
	CONTROLS_MENU->addItem("TextField");
	CONTROLS_MENU->size(100, 44);
	CONTROLS_MENU->position(2, 2);

	BTN_ADD_CONTROL = new Button();
	BTN_ADD_CONTROL->font(*TINY_FONT);
	BTN_ADD_CONTROL->text("Add Control");
	BTN_ADD_CONTROL->size(100, 17);
	BTN_ADD_CONTROL->position(CONTROLS_MENU->positionX(), CONTROLS_MENU->positionY() + CONTROLS_MENU->height() + 11);
	BTN_ADD_CONTROL->click().Connect(&addControlClicked);

	cout << "done." << endl;
}


void drawUi()
{
	CONTROLS_MENU->update();
	BTN_ADD_CONTROL->update();

	R->drawImage(*MOUSE_POINTER, MOUSE_POSITION.x(), MOUSE_POSITION.y());
}

int main(int argc, char *argv[])
{
	try
	{
		initNas2d(argv[0], "data", "builder.xml");

		R = &Utility<Renderer>::get();

		initEventHandlers();

		MOUSE_POINTER = new Image("ui/pointers/normal.png");
		TINY_FONT = new Font("fonts/ui-normal.png", 7, 9, -1);

		initUi();

		cout << "Entering main loop..." << endl << endl;

		// Main Loop


		while (running)
		{
			Utility<EventHandler>::get().pump();
			R->drawBoxFilled(0, 0, R->width(), R->height(), 30, 30, 30);

			WINDOW->update();

			if (UI_CONTROL_EDIT)
				drawHandles();


			drawUi();

			R->update();
		}

	}
	catch (Exception& e)
	{
		cout << "EXCEPTION (" << e.getBriefDescription() << "): " << e.getDescription() << endl;
	}

	delete MOUSE_POINTER;
	delete WINDOW;
	delete TINY_FONT;

	return 0;
}
