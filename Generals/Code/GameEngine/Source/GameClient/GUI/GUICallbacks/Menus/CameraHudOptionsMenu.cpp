/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
*/
// GeneralsX @feature OpenAI 25/09/2026 Staged Camera & HUD presentation preferences page.
#include "PreRTS.h"
#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionPreferences.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetSlider.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/Shell.h"
#include "GameClient/View.h"
#include "GameClient/WindowLayout.h"

static WindowLayout *s_cameraHudLayout = nullptr;
static WindowLayout *s_optionsLayout = nullptr;
static GameWindow *s_parent = nullptr;
static GameWindow *s_zoom = nullptr;
static GameWindow *s_distance = nullptr;
static GameWindow *s_hud = nullptr;
static GameWindow *s_zoomValue = nullptr;
static GameWindow *s_distanceValue = nullptr;
static GameWindow *s_hudValue = nullptr;
static OptionPreferences *s_preferences = nullptr;
static Bool s_suppressEscapeUp = FALSE;

static Int quantize(Int value, Int minimum, Int maximum, Int step)
{
	value = clamp(minimum, value, maximum);
	return minimum + ((value - minimum + step / 2) / step) * step;
}

static void setStagedDefaults()
{
	GadgetSliderSetPosition(s_zoom, 750);
	GadgetSliderSetPosition(s_distance, 125);
	GadgetSliderSetPosition(s_hud, 75);
}

static void applyStagedPreferences()
{
	if (!s_preferences || !s_zoom || !s_distance || !s_hud)
		return;
	const Int zoom = quantize(GadgetSliderGetPosition(s_zoom), 500, 1000, 50);
	const Int distance = quantize(GadgetSliderGetPosition(s_distance), 100, 200, 5);
	const Int hud = quantize(GadgetSliderGetPosition(s_hud), 60, 100, 5);
	GadgetSliderSetPosition(s_zoom, zoom);
	GadgetSliderSetPosition(s_distance, distance);
	GadgetSliderSetPosition(s_hud, hud);
	AsciiString value;
	value.format("%d", zoom); (*s_preferences)["MaxCameraHeight"] = value;
	value.format("%.2f", distance / 100.0f); (*s_preferences)["TerrainDrawDistanceScale"] = value;
	value.format("%.2f", hud / 100.0f); (*s_preferences)["ControlBarScale"] = value;
	s_preferences->write();
	TheWritableGlobalData->m_maxCameraHeight = (Real)zoom;
	TheWritableGlobalData->m_terrainDrawDistanceScale = distance / 100.0f;
	TheWritableGlobalData->m_controlBarScale = hud / 100.0f;
	if (TheTacticalView)
	{
		// GeneralsX @bugfix OpenAI 25/09/2026 Clamp through the recalculated aspect-aware camera limits.
		const Real currentHeight = TheTacticalView->getHeightAboveGround();
		TheTacticalView->setCameraHeightAboveGroundLimitsToDefault();
		TheTacticalView->setHeightAboveGround(currentHeight);
	}
	if (TheControlBar)
		TheControlBar->applyConfiguredControlBarScale();
}

void ShowCameraHudOptionsMenu()
{
	if (s_cameraHudLayout) return;
	s_optionsLayout = TheShell->getOptionsLayout(FALSE);
	if (!s_optionsLayout) return;
	s_optionsLayout->hide(TRUE);
	s_cameraHudLayout = TheWindowManager->winCreateLayout("Menus/CameraHudOptionsMenu.wnd");
	if (!s_cameraHudLayout) { s_optionsLayout->hide(FALSE); s_optionsLayout = nullptr; return; }
	s_cameraHudLayout->runInit();
	s_cameraHudLayout->hide(FALSE);
	s_cameraHudLayout->bringForward();
}

void CloseCameraHudOptionsMenu()
{
	WindowLayout *layout = s_cameraHudLayout; s_cameraHudLayout = nullptr;
	if (layout) { layout->runShutdown(); layout->destroyWindows(); deleteInstance(layout); }
	WindowLayout *options = s_optionsLayout; s_optionsLayout = nullptr;
	if (options) {
		options->hide(FALSE); options->bringForward();
		GameWindow *parent = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("OptionsMenu.wnd:OptionsMenuParent"));
		if (parent) { TheWindowManager->winSetModal(parent); TheWindowManager->winSetFocus(parent); }
	}
}

void CameraHudOptionsMenuInit(WindowLayout *layout, void *)
{
	s_suppressEscapeUp = FALSE;
	s_parent = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("CameraHudOptionsMenu.wnd:CameraHudOptionsMenuParent"));
	s_zoom = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("CameraHudOptionsMenu.wnd:SliderMaximumZoomOut"));
	s_distance = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("CameraHudOptionsMenu.wnd:SliderTerrainDrawDistance"));
	s_hud = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("CameraHudOptionsMenu.wnd:SliderControlBarScale"));
	s_zoomValue = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("CameraHudOptionsMenu.wnd:ValueMaximumZoomOut"));
	s_distanceValue = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("CameraHudOptionsMenu.wnd:ValueTerrainDrawDistance"));
	s_hudValue = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("CameraHudOptionsMenu.wnd:ValueControlBarScaleText"));
	s_preferences = NEW OptionPreferences;
	if (!s_parent || !s_zoom || !s_distance || !s_hud || !s_zoomValue || !s_distanceValue || !s_hudValue) { layout->hide(TRUE); return; }
	GadgetSliderSetPosition(s_zoom, quantize((Int)s_preferences->getMaxCameraHeight(), 500, 1000, 50));
	GadgetSliderSetPosition(s_distance, quantize(REAL_TO_INT(s_preferences->getTerrainDrawDistanceScale() * 100), 100, 200, 5));
	GadgetSliderSetPosition(s_hud, quantize(REAL_TO_INT(s_preferences->getControlBarScale() * 100), 60, 100, 5));
	TheWindowManager->winSetModal(s_parent); TheWindowManager->winSetFocus(s_parent);
}
void CameraHudOptionsMenuUpdate(WindowLayout *, void *)
{
	if (!s_zoom || !s_distance || !s_hud || !s_zoomValue || !s_distanceValue || !s_hudValue) return;
	UnicodeString text;
	text.format(L"%d", quantize(GadgetSliderGetPosition(s_zoom), 500, 1000, 50));
	GadgetStaticTextSetText(s_zoomValue, text);
	text.format(L"%d%%", quantize(GadgetSliderGetPosition(s_distance), 100, 200, 5));
	GadgetStaticTextSetText(s_distanceValue, text);
	text.format(L"%d%%", quantize(GadgetSliderGetPosition(s_hud), 60, 100, 5));
	GadgetStaticTextSetText(s_hudValue, text);
}
void CameraHudOptionsMenuShutdown(WindowLayout *layout, void *)
{
	delete s_preferences; s_preferences = nullptr; s_parent = s_zoom = s_distance = s_hud = s_zoomValue = s_distanceValue = s_hudValue = nullptr;
	if (layout) layout->hide(TRUE);
}
WindowMsgHandledType CameraHudOptionsMenuInput(GameWindow *, UnsignedInt msg, WindowMsgData key, WindowMsgData state)
{
	if (msg == GWM_CHAR && (Int)key == KEY_ESC) {
		if ((Int)state & KEY_STATE_DOWN) { s_suppressEscapeUp = TRUE; return MSG_HANDLED; }
		if ((Int)state & KEY_STATE_UP) { s_suppressEscapeUp = FALSE; CloseCameraHudOptionsMenu(); return MSG_HANDLED; }
	}
	return MSG_IGNORED;
}
WindowMsgHandledType CameraHudOptionsMenuSystem(GameWindow *, UnsignedInt msg, WindowMsgData data, WindowMsgData)
{
	if (msg != GBM_SELECTED) return MSG_IGNORED;
	GameWindow *control = (GameWindow *)data; if (!control) return MSG_IGNORED;
	const NameKeyType id = control->winGetWindowId();
	if (id == NAMEKEY("CameraHudOptionsMenu.wnd:ButtonApply")) applyStagedPreferences();
	else if (id == NAMEKEY("CameraHudOptionsMenu.wnd:ButtonResetDefaults")) setStagedDefaults();
	else if (id == NAMEKEY("CameraHudOptionsMenu.wnd:ButtonBack")) CloseCameraHudOptionsMenu();
	else return MSG_IGNORED;
	return MSG_HANDLED;
}
