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

#include "PreRTS.h"

#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/MessageBox.h"
#include "GameClient/MetaEvent.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"

static GameWindow *s_parent = nullptr;
static GameWindow *s_category = nullptr;
static GameWindow *s_commands = nullptr;
static GameWindow *s_description = nullptr;
static GameWindow *s_current = nullptr;
static GameWindow *s_capture = nullptr;
static NameKeyType s_backID = NAMEKEY_INVALID;
static NameKeyType s_categoryID = NAMEKEY_INVALID;
static NameKeyType s_commandsID = NAMEKEY_INVALID;
static NameKeyType s_resetAllID = NAMEKEY_INVALID;
static NameKeyType s_assignID = NAMEKEY_INVALID;
static MetaMapRec *s_selected = nullptr;
static MappableKeyType s_capturedKey = MK_NONE;
static MappableKeyModState s_capturedModifiers = NONE;
static Bool s_hasCapture = false;
static Bool s_suppressEscapeUp = false;

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

static void clearPendingCapture()
{
	s_capturedKey = MK_NONE;
	s_capturedModifiers = NONE;
	s_hasCapture = false;
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
	AsciiString text;
	if (key == MK_NONE)
		modifiers = NONE;
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

static void refreshSelection()
{
	if (!s_selected)
	{
		GadgetStaticTextSetText(s_description, UnicodeString::TheEmptyString);
		GadgetStaticTextSetText(s_current, UnicodeString::TheEmptyString);
		if (s_capture) GadgetTextEntrySetText(s_capture, UnicodeString::TheEmptyString);
		s_capture->winEnable(false);
		return;
	}
	UnicodeString description = s_selected->m_description;
	description.concat(L"\n\n");
	description.concat(TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardBindingHints",
		L"Click the binding field and press a key. Ctrl+Delete unbinds; Ctrl+Backspace restores the default."));
	GadgetStaticTextSetText(s_description, description);
	GadgetStaticTextSetText(s_current, bindingText(s_selected->m_key, s_selected->m_modState));
	GadgetTextEntrySetText(s_capture, bindingText(s_selected->m_key, s_selected->m_modState));
	s_capture->winEnable(true);
}

static void fillCommands(MappableKeyCategories category)
{
	GadgetListBoxReset(s_commands);
	s_selected = nullptr;
	const Color white = GameMakeColor(255, 255, 255, 255);
	for (const MetaMapRec *map = TheMetaMap->getFirstMetaMapRec(); map; map = map->m_next)
		if (map->m_category == category && !map->m_displayName.isEmpty() && TheMetaMap->isLogicalRepresentative(map))
			GadgetListBoxAddEntryText(s_commands, map->m_displayName, white, -1, -1);
	refreshSelection();
}

static void selectCommand(Int selected)
{
	Int index = 0;
	Int categoryIndex = 0;
	GadgetComboBoxGetSelectedPos(s_category, &categoryIndex);
	for (const MetaMapRec *map = TheMetaMap->getFirstMetaMapRec(); map; map = map->m_next)
	{
		if (map->m_category == (MappableKeyCategories)CategoryListName[categoryIndex].value && !map->m_displayName.isEmpty() && TheMetaMap->isLogicalRepresentative(map))
		{
			if (index++ == selected)
			{
				s_selected = TheMetaMap->getMutableMetaMapRec(map->m_meta);
				break;
			}
		}
	}
	clearPendingCapture();
	refreshSelection();
	TheWindowManager->winSetFocus(s_capture);
}

static void applyCaptured(Bool replaceConflict)
{
	if (!s_selected || !s_hasCapture) return;
	TheMetaMap->setBinding(s_selected->m_meta, s_capturedKey, s_capturedModifiers, replaceConflict);
	clearPendingCapture();
	refreshSelection();
	TheWindowManager->winSetFocus(s_capture);
}

static void replaceConflict()
{
	applyCaptured(true);
}

static void cancelConflict()
{
	clearPendingCapture();
	refreshSelection();
	TheWindowManager->winSetFocus(s_capture);
}

static void requestAssignment()
{
	if (!s_selected || !s_hasCapture) return;
	const MetaMapRec *conflict = TheMetaMap->findConflict(s_selected->m_meta, s_capturedKey, s_capturedModifiers);
	if (!conflict)
	{
		applyCaptured(false);
		return;
	}
	UnicodeString body;
	UnicodeString bodyFormat = TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardBindingConflictBody",
		L"%ls is currently assigned to %ls. Replace that binding?");
	body.format(bodyFormat.str(),
		bindingText(s_capturedKey, s_capturedModifiers).str(), conflict->m_displayName.str());
	MessageBoxYesNo(TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardBindingConflictTitle", L"Keyboard binding conflict"),
		body, replaceConflict, cancelConflict);
}

WindowMsgHandledType KeyboardTextEntryInput(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	if (msg == GWM_LEFT_DOWN)
	{
		// GeneralsX @bugfix OpenAI 23/09/2026 Make capture focus explicit instead of relying on parent propagation.
		BitSet(window->winGetInstanceData()->m_state, WIN_STATE_HILITED);
		TheWindowManager->winSetFocus(window);
		return MSG_HANDLED;
	}
	if (msg == GWM_INPUT_FOCUS)
	{
		if (mData1 == TRUE)
			*(Bool *)mData2 = TRUE;
		return MSG_HANDLED;
	}
	if (msg != GWM_CHAR) return MSG_IGNORED;
	const Int key = (Int)mData1;
	const Int state = (Int)mData2;
	if (!(state & KEY_STATE_DOWN) || (state & KEY_STATE_AUTOREPEAT)) return MSG_HANDLED;
	if (key == KEY_ESC)
	{
		s_suppressEscapeUp = true;
		clearPendingCapture();
		refreshSelection();
		TheWindowManager->winSetFocus(s_parent);
		return MSG_HANDLED;
	}
	if (key == KEY_BACKSPACE && (state & KEY_STATE_CONTROL) && s_selected)
	{
		TheMetaMap->resetBinding(s_selected->m_meta);
		clearPendingCapture();
		refreshSelection();
		return MSG_HANDLED;
	}
	if (key == KEY_DEL && (state & KEY_STATE_CONTROL) && s_selected)
	{
		TheMetaMap->setBinding(s_selected->m_meta, MK_NONE, NONE, false);
		clearPendingCapture();
		refreshSelection();
		return MSG_HANDLED;
	}
	MappableKeyType mapped = mappableKey(key);
	if (mapped == MK_NONE) return MSG_HANDLED;
	s_capturedKey = mapped;
	s_capturedModifiers = modifiersFromState(state);
	s_hasCapture = true;
	GadgetTextEntrySetText(s_capture, bindingText(mapped, s_capturedModifiers));
	return MSG_HANDLED;
}

void KeyboardOptionsMenuInit(WindowLayout *layout, void *)
{
	clearPendingCapture();
	s_suppressEscapeUp = false;
	s_selected = nullptr;
	s_backID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonBack");
	s_categoryID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ComboBoxCategoryList");
	s_commandsID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ListBoxCommandList");
	s_resetAllID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonResetAll");
	s_assignID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonAssign");
	s_parent = findRequiredControl("KeyboardOptionsMenu.wnd:ParentKeyboardOptionsMenu");
	s_category = findRequiredControl("KeyboardOptionsMenu.wnd:ComboBoxCategoryList");
	s_commands = findRequiredControl("KeyboardOptionsMenu.wnd:ListBoxCommandList");
	s_description = findRequiredControl("KeyboardOptionsMenu.wnd:StaticTextDescription");
	s_current = findRequiredControl("KeyboardOptionsMenu.wnd:StaticTextCurrentHotkey");
	s_capture = findRequiredControl("KeyboardOptionsMenu.wnd:TextEntryAssignHotkey");
	GameWindow *labelTitle = findRequiredControl("KeyboardOptionsMenu.wnd:LabelTitle");
	GameWindow *labelCategory = findRequiredControl("KeyboardOptionsMenu.wnd:LabelCategory");
	GameWindow *labelCurrent = findRequiredControl("KeyboardOptionsMenu.wnd:LabelCurrentHotkey");
	GameWindow *labelAssign = findRequiredControl("KeyboardOptionsMenu.wnd:LabelAssignHotkey");
	GameWindow *buttonAssign = findRequiredControl("KeyboardOptionsMenu.wnd:ButtonAssign");
	GameWindow *buttonResetAll = findRequiredControl("KeyboardOptionsMenu.wnd:ButtonResetAll");
	GameWindow *buttonBack = findRequiredControl("KeyboardOptionsMenu.wnd:ButtonBack");
	if (!layout || !s_parent || !s_category || !s_commands || !s_description || !s_current || !s_capture
		|| !labelTitle || !labelCategory || !labelCurrent || !labelAssign || !buttonAssign || !buttonResetAll || !buttonBack)
	{
		if (layout)
			layout->hide(true);
		return;
	}

	s_capture->winSetInputFunc(KeyboardTextEntryInput);
	// GeneralsX @feature OpenAI 24/09/2026 Localize the GeneralsX-owned layout with safe English fallbacks.
	GadgetStaticTextSetText(labelTitle,
		TheGameText->FETCH_OR_SUBSTITUTE("GUI:KeyboardControls", L"Keyboard Controls"));
	GadgetStaticTextSetText(labelCategory,
		TheGameText->FETCH_OR_SUBSTITUTE("GUI:Category", L"Category"));
	GadgetStaticTextSetText(labelCurrent,
		TheGameText->FETCH_OR_SUBSTITUTE("GUI:CurrentHotkey", L"Current Binding"));
	GadgetStaticTextSetText(labelAssign,
		TheGameText->FETCH_OR_SUBSTITUTE("GUI:NewHotkey", L"New Binding"));
	GadgetButtonSetText(buttonAssign,
		TheGameText->FETCH_OR_SUBSTITUTE("GUI:Assign", L"Assign"));
	GadgetButtonSetText(buttonResetAll,
		TheGameText->FETCH_OR_SUBSTITUTE("GUI:ResetAll", L"Reset All"));
	GadgetButtonSetText(buttonBack,
		TheGameText->FETCH_OR_SUBSTITUTE("GUI:Back", L"Back"));
	GadgetComboBoxReset(s_category);
	const Color white = GameMakeColor(255, 255, 255, 255);
	for (Int i = 0; i < CATEGORY_NUM_CATEGORIES; ++i)
	{
		AsciiString label;
		label.format("GUI:%s", CategoryListName[i].name);
		GadgetComboBoxAddEntry(s_category, TheGameText->fetch(label), white);
	}
	GadgetComboBoxSetSelectedPos(s_category, 0);
	fillCommands(CATEGORY_CONTROL);
	layout->hide(false);
	TheWindowManager->winSetFocus(s_parent);
}

void KeyboardOptionsMenuShutdown(WindowLayout *layout, void *)
{
	clearPendingCapture();
	s_suppressEscapeUp = false;
	s_selected = nullptr;
	layout->hide(true);
	TheShell->shutdownComplete(layout);
}

void KeyboardOptionsMenuUpdate(WindowLayout *, void *) {}

WindowMsgHandledType KeyboardOptionsMenuInput(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	if (msg == GWM_CHAR && mData1 == KEY_ESC && BitIsSet(mData2, KEY_STATE_UP))
	{
		if (s_suppressEscapeUp)
		{
			s_suppressEscapeUp = false;
			return MSG_HANDLED;
		}
		TheShell->pop();
		return MSG_HANDLED;
	}
	return MSG_IGNORED;
}

WindowMsgHandledType KeyboardOptionsMenuSystem(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	if (msg == GWM_INPUT_FOCUS)
	{
		if (mData1 == TRUE) *(Bool *)mData2 = TRUE;
		return MSG_HANDLED;
	}
	if (msg == GCM_SELECTED && ((GameWindow *)mData1)->winGetWindowId() == s_categoryID)
	{
		clearPendingCapture();
		Int selected = 0;
		GadgetComboBoxGetSelectedPos(s_category, &selected);
		fillCommands((MappableKeyCategories)CategoryListName[selected].value);
		return MSG_HANDLED;
	}
	if (msg == GLM_SELECTED && ((GameWindow *)mData1)->winGetWindowId() == s_commandsID)
	{
		Int selected = 0;
		GadgetListBoxGetSelected(s_commands, &selected);
		selectCommand(selected);
		return MSG_HANDLED;
	}
	if (msg == GBM_SELECTED)
	{
		const Int id = ((GameWindow *)mData1)->winGetWindowId();
		if (id == s_backID) TheShell->pop();
		else if (id == s_assignID) requestAssignment();
		else if (id == s_resetAllID)
		{
			TheMetaMap->resetAllBindings();
			clearPendingCapture();
			refreshSelection();
		}
		return MSG_HANDLED;
	}
	return MSG_IGNORED;
}
