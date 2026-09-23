/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
** Licensed under the GNU General Public License version 3.
*/

// FILE: KeyboardOptionsMenu.cpp
// GeneralsX @feature OpenAI 23/09/2026 Complete the retail keyboard binding screen.

#include "PreRTS.h"

#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetStaticText.h"
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
		s_capture->winEnable(false);
		return;
	}
	GadgetStaticTextSetText(s_description, s_selected->m_description);
	GadgetStaticTextSetText(s_current, bindingText(s_selected->m_key, s_selected->m_modState));
	s_capture->winEnable(true);
}

static void fillCommands(MappableKeyCategories category)
{
	GadgetListBoxReset(s_commands);
	s_selected = nullptr;
	const Color white = GameMakeColor(255, 255, 255, 255);
	for (const MetaMapRec *map = TheMetaMap->getFirstMetaMapRec(); map; map = map->m_next)
		if (map->m_category == category && !map->m_displayName.isEmpty())
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
		if (map->m_category == (MappableKeyCategories)CategoryListName[categoryIndex].value && !map->m_displayName.isEmpty())
		{
			if (index++ == selected)
			{
				s_selected = TheMetaMap->getMutableMetaMapRec(map->m_meta);
				break;
			}
		}
	}
	s_hasCapture = false;
	refreshSelection();
}

static void applyCaptured(Bool replaceConflict)
{
	if (!s_selected || !s_hasCapture) return;
	TheMetaMap->setBinding(s_selected->m_meta, s_capturedKey, s_capturedModifiers, replaceConflict);
	s_hasCapture = false;
	refreshSelection();
}

static void replaceConflict()
{
	applyCaptured(true);
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
	body.format(L"%ls is currently assigned to %ls. Replace that binding?",
		bindingText(s_capturedKey, s_capturedModifiers).str(), conflict->m_displayName.str());
	MessageBoxYesNo(UnicodeString(L"Keyboard binding conflict"), body, replaceConflict, nullptr);
}

WindowMsgHandledType KeyboardTextEntryInput(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	if (msg != GWM_CHAR) return MSG_IGNORED;
	const Int key = (Int)mData1;
	const Int state = (Int)mData2;
	if (!(state & KEY_STATE_DOWN) || (state & KEY_STATE_AUTOREPEAT)) return MSG_HANDLED;
	if (key == KEY_ESC)
	{
		s_hasCapture = false;
		refreshSelection();
		TheWindowManager->winSetFocus(s_parent);
		return MSG_HANDLED;
	}
	if (key == KEY_BACKSPACE && (state & KEY_STATE_CONTROL) && s_selected)
	{
		TheMetaMap->resetBinding(s_selected->m_meta);
		refreshSelection();
		return MSG_HANDLED;
	}
	if (key == KEY_DEL && (state & KEY_STATE_CONTROL) && s_selected)
	{
		TheMetaMap->setBinding(s_selected->m_meta, MK_NONE, NONE, false);
		refreshSelection();
		return MSG_HANDLED;
	}
	MappableKeyType mapped = mappableKey(key);
	if (mapped == MK_NONE) return MSG_HANDLED;
	s_capturedKey = mapped;
	s_capturedModifiers = modifiersFromState(state);
	s_hasCapture = true;
	GadgetStaticTextSetText(s_current, bindingText(mapped, s_capturedModifiers));
	return MSG_HANDLED;
}

void KeyboardOptionsMenuInit(WindowLayout *layout, void *)
{
	s_parent = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ParentKeyboardOptionsMenu"));
	s_backID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonBack");
	s_categoryID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ComboBoxCategoryList");
	s_commandsID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ListBoxCommandList");
	s_resetAllID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonResetAll");
	s_assignID = TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:ButtonAssign");
	s_category = TheWindowManager->winGetWindowFromId(nullptr, s_categoryID);
	s_commands = TheWindowManager->winGetWindowFromId(nullptr, s_commandsID);
	s_description = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:StaticTextDescription"));
	s_current = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:StaticTextCurrentHotkey"));
	s_capture = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("KeyboardOptionsMenu.wnd:TextEntryAssignHotkey"));
	s_capture->winSetInputFunc(KeyboardTextEntryInput);
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
	layout->hide(true);
	TheShell->shutdownComplete(layout);
}

void KeyboardOptionsMenuUpdate(WindowLayout *, void *) {}

WindowMsgHandledType KeyboardOptionsMenuInput(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	if (msg == GWM_CHAR && mData1 == KEY_ESC && BitIsSet(mData2, KEY_STATE_UP))
	{
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
			refreshSelection();
		}
		return MSG_HANDLED;
	}
	return MSG_IGNORED;
}
