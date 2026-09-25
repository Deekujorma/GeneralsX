/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// FILE: KeyboardOptionsMenu.cpp
// GeneralsX @feature OpenAI 23/09/2026 Complete the retail keyboard binding screen.
// GeneralsX @refactor OpenAI 25/09/2026 Present bindings in a direct two-column list owned as an Options subpage.

#include "PreRTS.h"

#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/MessageBox.h"
#include "GameClient/MetaEvent.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"

static WindowLayout *s_keyboardLayout = nullptr;
static WindowLayout *s_optionsLayout = nullptr;
static GameWindow *s_parent = nullptr;
static GameWindow *s_commands = nullptr;
static GameWindow *s_status = nullptr;
static GameWindow *s_change = nullptr;
static GameWindow *s_clear = nullptr;
static GameWindow *s_resetSelected = nullptr;
static NameKeyType s_backID = NAMEKEY_INVALID;
static NameKeyType s_commandsID = NAMEKEY_INVALID;
static NameKeyType s_changeID = NAMEKEY_INVALID;
static NameKeyType s_clearID = NAMEKEY_INVALID;
static NameKeyType s_resetSelectedID = NAMEKEY_INVALID;
static NameKeyType s_resetAllID = NAMEKEY_INVALID;
static MetaMapRec *s_selected = nullptr;
static Int s_selectedRow = -1;
static MappableKeyType s_capturedKey = MK_NONE;
static MappableKeyModState s_capturedModifiers = NONE;
static Bool s_captureMode = false;
static Bool s_suppressEscapeUp = false;
static Bool s_initialized = false;

// GeneralsX @bugfix OpenAI 25/09/2026 Report incomplete keyboard-options layouts instead of dereferencing missing controls.
static GameWindow *findRequiredControl(const char *name)
{
	GameWindow *window = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name));
	if (!window)
	{
		fprintf(stderr, "[KeyboardOptionsMenu] Missing required control: %s\n", name);
		fflush(stderr);
	}
	return window;
}

static const char *bindingName(const LookupListRec *names, Int value)
{
	for (; names->name; ++names)
		if (names->value == value)
			return names->name;
	return "KEY_NONE";
}

static UnicodeString bindingText(MappableKeyType key, MappableKeyModState modifiers)
{
	if (key == MK_NONE)
		return TheGameText->FETCH_OR_SUBSTITUTE("GUI:Unbound", L"Unbound");

	AsciiString text;
	if (modifiers & ALT) text.concat("Alt+");
	if (modifiers & CTRL) text.concat("Ctrl+");
	if (modifiers & SHIFT) text.concat("Shift+");
	const char *keyName = bindingName(KeyNames, key);
	if (strncmp(keyName, "KEY_", 4) == 0) keyName += 4;
	text.concat(keyName);
	UnicodeString result;
	result.translate(text);
	return result;
}

static MappableKeyType mappableKey(Int key)
{
	for (const LookupListRec *name = KeyNames; name->name; ++name)
		if (name->value == key)
			return (MappableKeyType)key;
	return MK_NONE;
}

static MappableKeyModState modifiersFromState(Int state)
{
	Int modifiers = NONE;
	if (state & KEY_STATE_ALT) modifiers |= ALT;
	if (state & KEY_STATE_CONTROL) modifiers |= CTRL;
	if (state & KEY_STATE_SHIFT) modifiers |= SHIFT;
	return (MappableKeyModState)modifiers;
}

static void showSelectedStatus()
{
	if (!s_selected)
	{
		GadgetStaticTextSetText(s_status, TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardSelectAction",
			L"Select an action, then choose Change Binding."));
	}
	else
	{
		GadgetStaticTextSetText(s_status, s_selected->m_description);
	}
	const Bool enabled = s_selected != nullptr;
	s_change->winEnable(enabled);
	s_clear->winEnable(enabled);
	s_resetSelected->winEnable(enabled);
}

static void cancelCapture()
{
	s_captureMode = false;
	s_capturedKey = MK_NONE;
	s_capturedModifiers = NONE;
	showSelectedStatus();
	TheWindowManager->winSetFocus(s_parent);
}

static void updateSelectedBinding()
{
	if (s_selected && s_selectedRow >= 0)
		GadgetListBoxAddEntryText(s_commands, bindingText(s_selected->m_key, s_selected->m_modState),
			GameMakeColor(255, 255, 255, 255), s_selectedRow, 1);
}

static void updateAllBindings()
{
	const Color white = GameMakeColor(255, 255, 255, 255);
	for (Int row = 0; row < GadgetListBoxGetNumEntries(s_commands); ++row)
	{
		MetaMapRec *action = (MetaMapRec *)GadgetListBoxGetItemData(s_commands, row);
		if (action)
			GadgetListBoxAddEntryText(s_commands, bindingText(action->m_key, action->m_modState), white, row, 1);
	}
}

static void fillCommands()
{
	GadgetListBoxReset(s_commands);
	s_selected = nullptr;
	s_selectedRow = -1;
	const Color white = GameMakeColor(255, 255, 255, 255);
	for (const MetaMapRec *map = TheMetaMap->getFirstMetaMapRec(); map; map = map->m_next)
	{
		if (map->m_displayName.isEmpty() || !TheMetaMap->isLogicalRepresentative(map))
			continue;
		MetaMapRec *action = TheMetaMap->getMutableMetaMapRec(map->m_meta);
		const Int row = GadgetListBoxAddEntryText(s_commands, action->m_displayName, white, -1, 0);
		GadgetListBoxAddEntryText(s_commands, bindingText(action->m_key, action->m_modState), white, row, 1);
		GadgetListBoxSetItemData(s_commands, action, row);
	}
	showSelectedStatus();
}

static void applyCaptured(Bool replaceConflict)
{
	if (!s_selected || !s_captureMode) return;
	TheMetaMap->setBinding(s_selected->m_meta, s_capturedKey, s_capturedModifiers, replaceConflict);
	updateAllBindings();
	cancelCapture();
}

static void replaceConflict()
{
	applyCaptured(true);
}

static void cancelConflict()
{
	cancelCapture();
}

static void requestAssignment(MappableKeyType key, MappableKeyModState modifiers)
{
	if (!s_selected || !s_captureMode) return;
	s_capturedKey = key;
	s_capturedModifiers = modifiers;
	const MetaMapRec *conflict = TheMetaMap->findConflict(s_selected->m_meta, key, modifiers);
	if (!conflict)
	{
		applyCaptured(false);
		return;
	}
	UnicodeString body;
	UnicodeString bodyFormat = TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardBindingConflictBody",
		L"%ls is currently assigned to %ls. Replace that binding?");
	body.format(bodyFormat.str(), bindingText(key, modifiers).str(), conflict->m_displayName.str());
	MessageBoxYesNo(TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardBindingConflictTitle", L"Keyboard binding conflict"),
		body, replaceConflict, cancelConflict);
}

static void beginCapture()
{
	if (!s_selected) return;
	s_captureMode = true;
	UnicodeString prompt;
	UnicodeString promptFormat = TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardBindingPrompt",
		L"Press a key for \"%ls\" - Esc to cancel");
	prompt.format(promptFormat.str(), s_selected->m_displayName.str());
	GadgetStaticTextSetText(s_status, prompt);
	TheWindowManager->winSetFocus(s_parent);
}

static void resetAllConfirmed()
{
	TheMetaMap->resetAllBindings();
	fillCommands();
}

static void requestResetAll()
{
	MessageBoxYesNo(TheGameText->FETCH_OR_SUBSTITUTE("GUI:ResetAll", L"Reset All"),
		TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardResetAllConfirm", L"Restore all keyboard bindings to their defaults?"),
		resetAllConfirmed, nullptr);
}

// GeneralsX @feature OpenAI 25/09/2026 Keep Keyboard Controls outside the Shell stack while preserving the live Options layout.
void ShowKeyboardOptionsMenu()
{
	if (s_keyboardLayout)
		return;

	s_optionsLayout = TheShell->getOptionsLayout(FALSE);
	if (!s_optionsLayout)
		return;
	s_optionsLayout->hide(TRUE);

	s_keyboardLayout = TheWindowManager->winCreateLayout("Menus/KeyboardOptionsMenu.wnd");
	if (!s_keyboardLayout)
	{
		s_optionsLayout->hide(FALSE);
		s_optionsLayout->bringForward();
		s_optionsLayout = nullptr;
		return;
	}

	s_initialized = false;
	s_keyboardLayout->runInit();
	if (!s_initialized)
	{
		CloseKeyboardOptionsMenu();
		return;
	}
	s_keyboardLayout->hide(FALSE);
	s_keyboardLayout->bringForward();
}

void CloseKeyboardOptionsMenu()
{
	WindowLayout *keyboardLayout = s_keyboardLayout;
	s_keyboardLayout = nullptr;
	if (keyboardLayout)
	{
		keyboardLayout->runShutdown();
		keyboardLayout->destroyWindows();
		deleteInstance(keyboardLayout);
	}

	WindowLayout *optionsLayout = s_optionsLayout;
	s_optionsLayout = nullptr;
	if (optionsLayout)
	{
		optionsLayout->hide(FALSE);
		optionsLayout->bringForward();
		GameWindow *optionsParent = TheWindowManager->winGetWindowFromId(nullptr,
			TheNameKeyGenerator->nameToKey("OptionsMenu.wnd:OptionsMenuParent"));
		if (optionsParent)
		{
			TheWindowManager->winSetModal(optionsParent);
			TheWindowManager->winSetFocus(optionsParent);
		}
	}
}

void KeyboardOptionsMenuInit(WindowLayout *layout, void *)
{
	s_initialized = false;
	s_captureMode = false;
	s_suppressEscapeUp = false;
	s_selected = nullptr;
	s_selectedRow = -1;
	s_backID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonBack");
	s_commandsID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ListBoxCommandList");
	s_changeID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonChangeBinding");
	s_clearID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonClear");
	s_resetSelectedID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonResetSelected");
	s_resetAllID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonResetAll");
	s_parent = findRequiredControl("KeyboardOptionsMenu.wnd:ParentKeyboardOptionsMenu");
	s_commands = findRequiredControl("KeyboardOptionsMenu.wnd:ListBoxCommandList");
	s_status = findRequiredControl("KeyboardOptionsMenu.wnd:StaticTextStatus");
	s_change = findRequiredControl("KeyboardOptionsMenu.wnd:ButtonChangeBinding");
	s_clear = findRequiredControl("KeyboardOptionsMenu.wnd:ButtonClear");
	s_resetSelected = findRequiredControl("KeyboardOptionsMenu.wnd:ButtonResetSelected");
	GameWindow *labelTitle = findRequiredControl("KeyboardOptionsMenu.wnd:LabelTitle");
	GameWindow *labelAction = findRequiredControl("KeyboardOptionsMenu.wnd:LabelAction");
	GameWindow *labelBinding = findRequiredControl("KeyboardOptionsMenu.wnd:LabelBinding");
	GameWindow *buttonResetAll = findRequiredControl("KeyboardOptionsMenu.wnd:ButtonResetAll");
	GameWindow *buttonBack = findRequiredControl("KeyboardOptionsMenu.wnd:ButtonBack");
	if (!layout || !s_parent || !s_commands || !s_status || !s_change || !s_clear || !s_resetSelected
		|| !labelTitle || !labelAction || !labelBinding || !buttonResetAll || !buttonBack)
	{
		if (layout) layout->hide(TRUE);
		return;
	}

	GadgetStaticTextSetText(labelTitle, TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardControls", L"Keyboard Controls"));
	GadgetStaticTextSetText(labelAction, TheGameText->FETCH_OR_SUBSTITUTE("GUI:Action", L"Action"));
	GadgetStaticTextSetText(labelBinding, TheGameText->FETCH_OR_SUBSTITUTE("GUI:Binding", L"Binding"));
	GadgetButtonSetText(s_change, TheGameText->FETCH_OR_SUBSTITUTE("GUI:ChangeBinding", L"Change Binding"));
	GadgetButtonSetText(s_clear, TheGameText->FETCH_OR_SUBSTITUTE("GUI:Clear", L"Clear"));
	GadgetButtonSetText(s_resetSelected, TheGameText->FETCH_OR_SUBSTITUTE("GUI:ResetSelected", L"Reset Selected"));
	GadgetButtonSetText(buttonResetAll, TheGameText->FETCH_OR_SUBSTITUTE("GUI:ResetAll", L"Reset All"));
	GadgetButtonSetText(buttonBack, TheGameText->FETCH_OR_SUBSTITUTE("GUI:Back", L"Back"));
	fillCommands();
	layout->hide(FALSE);
	// GeneralsX @bugfix OpenAI 25/09/2026 Place the visible child page above the preserved hidden Options modal.
	TheWindowManager->winSetModal(s_parent);
	TheWindowManager->winSetFocus(s_parent);
	s_initialized = true;
}

void KeyboardOptionsMenuShutdown(WindowLayout *layout, void *)
{
	s_captureMode = false;
	s_suppressEscapeUp = false;
	s_selected = nullptr;
	s_selectedRow = -1;
	s_initialized = false;
	if (layout) layout->hide(TRUE);
}

void KeyboardOptionsMenuUpdate(WindowLayout *, void *) {}

WindowMsgHandledType KeyboardOptionsMenuInput(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	if (msg != GWM_CHAR) return MSG_IGNORED;
	const Int state = (Int)mData2;
	const Int key = (Int)mData1;
	if (key == KEY_ESC)
	{
		if (state & KEY_STATE_DOWN)
		{
			if (!(state & KEY_STATE_AUTOREPEAT) && s_captureMode)
			{
				cancelCapture();
				s_suppressEscapeUp = true;
			}
			return MSG_HANDLED;
		}
		if (state & KEY_STATE_UP)
		{
			if (s_suppressEscapeUp)
				s_suppressEscapeUp = false;
			else
				CloseKeyboardOptionsMenu();
		}
		return MSG_HANDLED;
	}
	if (!(state & KEY_STATE_DOWN) || (state & KEY_STATE_AUTOREPEAT)) return MSG_HANDLED;
	if (!s_captureMode) return MSG_IGNORED;
	const MappableKeyType mapped = mappableKey(key);
	if (mapped != MK_NONE)
		requestAssignment(mapped, modifiersFromState(state));
	return MSG_HANDLED;
}

WindowMsgHandledType KeyboardOptionsMenuSystem(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	if (msg == GWM_INPUT_FOCUS)
	{
		if (mData1 == TRUE) *(Bool *)mData2 = TRUE;
		return MSG_HANDLED;
	}
	if (msg == GLM_SELECTED && ((GameWindow *)mData1)->winGetWindowId() == s_commandsID)
	{
		GadgetListBoxGetSelected(s_commands, &s_selectedRow);
		s_selected = s_selectedRow >= 0 ? (MetaMapRec *)GadgetListBoxGetItemData(s_commands, s_selectedRow) : nullptr;
		s_captureMode = false;
		showSelectedStatus();
		return MSG_HANDLED;
	}
	if (msg == GBM_SELECTED)
	{
		const Int id = ((GameWindow *)mData1)->winGetWindowId();
		if (id == s_backID) CloseKeyboardOptionsMenu();
		else if (id == s_changeID) beginCapture();
		else if (id == s_clearID && s_selected)
		{
			TheMetaMap->setBinding(s_selected->m_meta, MK_NONE, NONE, false);
			updateSelectedBinding();
			showSelectedStatus();
		}
		else if (id == s_resetSelectedID && s_selected)
		{
			TheMetaMap->resetBinding(s_selected->m_meta);
			updateSelectedBinding();
			showSelectedStatus();
		}
		else if (id == s_resetAllID) requestResetAll();
		return MSG_HANDLED;
	}
	return MSG_IGNORED;
}
