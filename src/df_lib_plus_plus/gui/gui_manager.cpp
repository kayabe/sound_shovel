// Own header
#include "gui_manager.h"

// Project headers
#include "preferences.h"
#include "threading.h"
#include "gui/container_hori.h"
#include "gui/container_vert.h"
#include "gui/drawing_primitives.h"
#include "gui/keyboard_shortcuts.h"
#include "gui/menu.h"
#include "gui/results_view.h"
#include "gui/status_bar.h"
#include "gui/tooltip_manager.h"
#include "gui/widget_history.h"

// Contrib headers
#include "df_bitmap.h"
#include "df_rgba_colour.h"
#include "df_input.h"
#include "df_message_dialog.h"
#include "df_text_renderer.h"
#include "df_window_manager.h"


GuiManager *g_guiManager = NULL;


// ****************************************************************************
//  Class GuiManager
// ****************************************************************************

void MouseUpdateHandler(int x, int y)
{
    if (g_guiManager)
    {
        g_guiManager->m_cursorManager.Render(x, y);
    }
}


GuiManager::GuiManager()
:   m_modalWidget(NULL),
    m_previouslyFocussedWidget(NULL),
    m_focussedWidget(NULL),
    m_mainContainer(NULL),
    m_exitRequested(false),
    m_exitAtEndOfFrame(false),
    Widget(GUI_MANAGER_NAME, NULL)
{
    g_widgetHistory = new WidgetHistory("widget_history.txt");
    g_keyboardShortcutManager = new KeyboardShortcutManager("data/config_keys.txt");

    SetColours();
    m_propFont = CreateTextRenderer("Tahoma", 8, 5);
    ReleaseAssert((int)m_propFont, "Couldn't load font 'Tahoma'");

    g_commandSender.RegisterReceiver(this);

    // Register the mouse update handler function with the input manager
    //    g_inputManager.RegisterMouseUpdateCallback(&MouseUpdateHandler);  // TODO - Implement RegisterMouseUpdateCallback

    // Create main container
    m_mainContainer = new ContainerVert("MainContainer", this);
    int SCREEN_X = 0;
    int SCREEN_Y = 0;
    int SCREEN_W = g_window->bmp->width;
    int SCREEN_H = g_window->bmp->height;
    SetRect(SCREEN_X, SCREEN_W, SCREEN_W, SCREEN_H);

    // Create MenuBar
    MenuBar *menuBar = new MenuBar(m_mainContainer);
    m_mainContainer->AddWidget(menuBar);
    menuBar->Initialise();

    ResultsView *resultsView = new ResultsView("ResultsView", m_mainContainer);
    m_mainContainer->AddWidget(resultsView);

    // Create StatusBar
    StatusBar *statusBar = new StatusBar(m_mainContainer);
    m_mainContainer->AddWidget(statusBar);

    SetRect(SCREEN_X, SCREEN_Y, SCREEN_W, SCREEN_H);

    SetFocussedWidget(resultsView);
}


RGBAColour GuiManager::GetColour(char const *name, RGBAColour const &defaultC)
{
    RGBAColour rv = defaultC;

    char const *str = NULL;// g_prefs->GetString(name); // TODO - re-enable the prefs system

    if (str)
        StringToColour(str, &rv);

    return rv;
}


void GuiManager::SetColours()
{
    m_backgroundColour = GetColour("GuiWindowBackgroundColour", Colour(50,50,50));

    m_frameColour2 = GetColour("GuiFrameColour", Colour(180, 180, 180));
    m_textColourNormal = GetColour("GuiTextColourNormal", Colour(200,200,200));
    m_selectionBlockColour = GetColour("GuiSelectionBlockColour", Colour(60, 60, 155, 64));
    m_highlightColour = GetColour("GuiHighlightColour", m_backgroundColour + Colour(15,15,15));
    m_strongHighlightColour = GetColour("GuiStrongHighlightColour", m_backgroundColour + Colour(35,35,35));
    m_focusBoxColour = GetColour("GuiFocusBoxColour", Colour(255,120,120));

    m_selectionBlockUnfocussedColour = m_selectionBlockColour;
    m_selectionBlockUnfocussedColour = m_selectionBlockUnfocussedColour * 0.5f + m_backgroundColour * 0.5f;

    m_frameColour1 = m_frameColour2 * 0.4f;
    m_frameColour3 = m_frameColour2 * 1.2f;
    m_frameColour4 = m_frameColour2 * 1.6f;
    m_frameColour5 = m_frameColour2 * 2.0f;

    m_textColourFrame = GetColour("GuiTextColourFrame", Colour(0,0,0));
}


#define APPLICATION_NAME "Sound Shovel" // TODO - make this a string passed in at run time to both the prefs system and this module, from the app (ie non-library) code.

unsigned long __stdcall AboutProc(void *data)
{
    MessageDialog("About " APPLICATION_NAME,
        "               " APPLICATION_NAME "\n\n"
        "              (Beta Version)\n"
        "               " __DATE__ "\n\n"
        " Created by Deadfrog Software  \n",
        MsgDlgTypeOk);

    return 0;
}


void GuiManager::About()
{
    StartThread(AboutProc, NULL);
}


bool GuiManager::StringToColour(char const *str, RGBAColour *col)
{
    col->r = atoi(str);
    str = strchr(str, ',');
    if (!str) return false;
    str++;
    col->g = atoi(str);
    str = strchr(str, ',');
    if (!str) return false;
    str++;
    col->b = atoi(str);

    return true;
}


void GuiManager::FillBackground(int x, int y, int w, int h, bool highlighted) const
{
    DrawOutlineBox(x, y, w, h, m_frameColour5);
    x++; y++;
    w -= 2; h -= 2;
//    RectFill(g_window->bmp, x, y, w, h, m_backgroundColour);
}


void GuiManager::DrawFrame(int x, int y, int w, int h) const
{
    RectFill(g_window->bmp, x-3, y-3, w+8, 3, m_frameColour2);
    RectFill(g_window->bmp, x-3, y+h, w+8, 4, m_frameColour2);
    RectFill(g_window->bmp, x-3, y, 4, h, m_frameColour2);
    RectFill(g_window->bmp, x+w, y, 4, h, m_frameColour2);
}


char *GuiManager::ExecuteCommand(char const *object, char const *command, char const *arguments)
{
    if (COMMAND_IS("About"))               About();
    else if (COMMAND_IS("Exit"))           RequestExit();

    return NULL;
}


void GuiManager::SetRect(int x, int y, int w, int h)
{
    if (x != m_left || y != m_top || m_width != w || m_height != h)
    {
        g_widgetHistory->SetInt("MainWindowWidth", w);
        g_widgetHistory->SetInt("MainWindowHeight", h);
        g_widgetHistory->SetInt("MainWindowLeft", x);
        g_widgetHistory->SetInt("MainWindowTop", y);
        m_width = w;
        m_height = h;
        m_left = x;
        m_top = y;
    }

    m_mainContainer->SetRect(0 + WIDGET_SPACER, 0 + WIDGET_SPACER,
                             w - (WIDGET_SPACER*2), h - (WIDGET_SPACER*2));
}


Widget *GuiManager::GetWidgetAtPos(int x, int y)
{
    return m_mainContainer->GetWidgetAtPos(x, y);
}


Widget *GuiManager::GetWidgetByName(char const *name)
{
    // Special case when looking for "CurrentDoc"
    if (m_focussedWidget)
    {
        Widget *rv = m_focussedWidget->GetWidgetByName(name);
        if (rv)
            return rv;
    }

    return m_mainContainer->GetWidgetByName(name);
}


void GuiManager::Show(char const *widgetToShow)
{
    m_mainContainer->Show(widgetToShow);
}


void GuiManager::RequestExit()
{
    m_exitRequested = true;
}


void GuiManager::SetFocussedWidget(Widget *w)
{
    m_focussedWidget = w;
    while (w != this)
    {
        w->m_hideState = HideStateShown;
        w = w->m_parent;
    }
}


void GuiManager::Advance()
{
    if (g_window->windowClosed)
    {
        g_window->windowClosed = false;
        g_guiManager->RequestExit();
    }

    // g_commandSender.ProcessDeferredCommands();

    // Update the size of all the widgets, unless we are minimised.
//     if (SCREEN_H > 100)
//         SetRect(SCREEN_X, SCREEN_Y, SCREEN_W, SCREEN_H);

    if (!m_modalWidget)
        g_keyboardShortcutManager->Advance();

    // Update exit request status
    m_exitAtEndOfFrame = m_exitRequested;
    m_exitRequested = false;

    // Update cursor bitmap
    m_cursorManager.Advance();

    if (m_modalWidget)
    {
        m_modalWidget->Advance();
    }
    else
    {
        // Update which widget is highlighted
        MenuBar *menu = (MenuBar*)GetWidgetByName(MENU_BAR_NAME);
        DebugAssert(menu);
        if ((g_inputManager.lmbClicked || g_inputManager.rmbClicked)
            && !menu->DoesClickHitMenu())
        {
            Widget *hit = GetWidgetAtPos(g_inputManager.mouseX, g_inputManager.mouseY);
            if (hit && hit->m_highlightable)
            {
                m_focussedWidget = hit;
            }
        }

        // Advance our sub-widgets. Only advance the menu if it
        // is currently highlighted
        if (m_focussedWidget == menu)
        {
            menu->Advance();
        }
        else
        {
            m_mainContainer->Advance();
        }
    }

    g_tooltipManager.Advance();
}


void GuiManager::Render()
{
    const int winW = g_window->bmp->width;
    const int winH = g_window->bmp->height;
//  const int docViewManagerWidth = winW / 5;

    HLine(g_window->bmp, 0, 0, winW, m_frameColour3);
    VLine(g_window->bmp, 0, 0, winH, m_frameColour3);

    m_mainContainer->Render();

#if PROFILER_ENABLED
    m_profileWindow->Render();
#endif

    // Draw a box around the highlighted widget, unless it is the menu bar
    {
        Widget const *hw = m_focussedWidget;
        // TODO - implement g_inputManager.m_windowHasFocus
//         if (g_inputManager.m_windowHasFocus && hw &&
//             stricmp(MENU_BAR_NAME, hw->m_name) != 0)
//         {
//             DrawOutlineBox(hw->m_left - 2, hw->m_top - 2,
//                 hw->m_width + 4, hw->m_height + 4,
//                 m_frameColour5);
//             DrawOutlineBox(hw->m_left - 1, hw->m_top - 1,
//                 hw->m_width + 2, hw->m_height + 2,
//                 m_focusBoxColour);
//         }
    }


    // Render the menu bar and any context menu
    Widget *menubar = GetWidgetByName(MENU_BAR_NAME);
    g_guiManager->DrawFrame(menubar->m_left, menubar->m_top, menubar->m_width, menubar->m_height-2);
    menubar->Render();

    if (m_modalWidget)
        m_modalWidget->Render();

    g_tooltipManager.Render();

    // Cursor
    g_guiManager->m_cursorManager.Render(g_inputManager.mouseX, g_inputManager.mouseY);
}


void GuiManager::SetModalWidget(Widget *w)
{
    if (w)
    {
        m_previouslyFocussedWidget = m_focussedWidget;
        m_focussedWidget = w;
    }
    else
    {
        m_focussedWidget = m_previouslyFocussedWidget;
        m_previouslyFocussedWidget = NULL;
    }

    m_modalWidget = w;
}
