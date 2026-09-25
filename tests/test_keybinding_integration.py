#!/usr/bin/env python3
"""Source-level integration checks for the legacy-engine keybinding wiring."""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
META = (ROOT / "Core/GameEngine/Source/GameClient/MessageStream/MetaEvent.cpp").read_text()
LOOK = (ROOT / "Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp").read_text()
HEADER = (ROOT / "Core/GameEngine/Include/GameClient/MetaEvent.h").read_text()
LAYOUT = (ROOT / "GeneralsZH/Data/Window/Menus/KeyboardOptionsMenu.wnd").read_text()
W3D_LISTBOX = (ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DListBox.cpp").read_text()


class KeyBindingIntegrationTests(unittest.TestCase):
    def test_camera_defaults_and_wasd_keys_are_mappable(self):
        for action, default in (("UP", "MK_UP"), ("DOWN", "MK_DOWN"), ("LEFT", "MK_LEFT"), ("RIGHT", "MK_RIGHT")):
            self.assertIn(f"MSG_META_CAMERA_PAN_{action}", META)
            self.assertIn(default, META)
        for key in ("MK_W", "MK_A", "MK_S", "MK_D"):
            self.assertIn(key, HEADER)

    def test_overrides_follow_defaults_and_use_validated_binding_path(self):
        for game in ("Generals", "GeneralsMD"):
            source = (ROOT / game / "Code/GameEngine/Source/Common/GameEngine.cpp").read_text()
            self.assertLess(source.index("generateMetaMap()"), source.index("initializeUserBindings()"))
        loader = META[META.index("void MetaMap::initializeUserBindings()"):META.index("const MetaMapRec *MetaMap::findConflict")]
        self.assertIn("SplitOverride", loader)
        self.assertIn("applyBinding", loader)
        self.assertIn("Apply explicit unbinds first", loader)
        self.assertIn("Ignoring obsolete key binding", loader)

    def test_round_trip_reset_and_unbind_paths_exist(self):
        self.assertIn('preferences.load("KeyBindings.ini")', META)
        self.assertIn('preferences.setAsciiString(action, value)', META)
        self.assertIn("map->m_key = map->m_defaultKey", META)
        self.assertIn("partner->m_key = partner->m_defaultKey", META)
        self.assertIn("key == MK_NONE", META)

    def test_logical_pairs_are_applied_and_replaced_atomically(self):
        apply = META[META.index("Bool MetaMap::applyBinding"):META.index("Bool MetaMap::setBinding")]
        self.assertIn("getLogicalRepresentative", apply)
        self.assertIn("getLogicalPartner", apply)
        self.assertIn("partner->m_key = key", apply)
        self.assertIn("conflictingPartner->m_key = MK_NONE", apply)
        loader = META[META.index("void MetaMap::initializeUserBindings()"):META.index("const MetaMapRec *MetaMap::findConflict")]
        self.assertIn("appliedActions", loader)
        self.assertIn("Ignoring duplicate logical key binding", loader)

    def test_held_camera_uses_tested_state_and_modifier_independent_release(self):
        self.assertIn("HeldCameraPanState scrollDir", LOOK)
        self.assertIn("scrollDir.set", LOOK)
        self.assertIn("scrollDir.clear", LOOK)
        release = META[META.index("if (msg->getType() == GameMessage::MSG_RAW_KEY_UP"):META.index("// for our purposes here")]
        self.assertNotIn("newModState", release)

    def test_camera_release_continues_through_meta_up_matching_then_is_consumed(self):
        release = META[META.index("Bool releasedCameraPan"):META.index("// for our purposes here")]
        self.assertIn("releasePhysicalKey", release)
        self.assertNotIn("return;", release)
        loop_start = META.index("for (const MetaMapRec *map", META.index("Bool releasedCameraPan"))
        normal_loop_end = META[loop_start:META.index("if (msg->getType() == GameMessage::MSG_RAW_KEY_DOWN")]
        self.assertIn("map->m_transition == UP", normal_loop_end)
        self.assertIn("if (releasedCameraPan)\n\t\tdisp = DESTROY_MESSAGE", normal_loop_end)

    def test_unbound_actions_clear_stale_modifiers_for_both_pair_halves(self):
        apply = META[META.index("Bool MetaMap::applyBinding"):META.index("Bool MetaMap::setBinding")]
        self.assertIn("conflictingAction->m_modState = NONE", apply)
        self.assertIn("conflictingPartner->m_modState = NONE", apply)
        self.assertIn("otherAction->m_modState = NONE", apply)
        self.assertIn("NormalizeModifiersForKey", apply)

    def test_game_variants_keep_identical_menu_logic(self):
        base = (ROOT / "Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/KeyboardOptionsMenu.cpp").read_text()
        zh = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/KeyboardOptionsMenu.cpp").read_text()
        base = base.replace("Command & Conquer Generals(tm)", "Command & Conquer Generals Zero Hour(tm)")
        self.assertEqual(base, zh)

    def test_generalsx_keyboard_layout_contains_callback_contract(self):
        for callback in (
            "KeyboardOptionsMenuInit",
            "KeyboardOptionsMenuUpdate",
            "KeyboardOptionsMenuShutdown",
            "KeyboardOptionsMenuSystem",
            "KeyboardOptionsMenuInput",
        ):
            self.assertIn(callback, LAYOUT)
        for control in (
            "ParentKeyboardOptionsMenu",
            "ButtonBack",
            "ListBoxCommandList",
            "StaticTextStatus",
            "LabelAction",
            "LabelBinding",
            "ButtonChangeBinding",
            "ButtonClear",
            "ButtonResetSelected",
            "ButtonResetAll",
        ):
            self.assertEqual(LAYOUT.count(f'NAME = "KeyboardOptionsMenu.wnd:{control}";'), 1)
        for obsolete in ("ComboBoxCategoryList", "TextEntryAssignHotkey", "ButtonAssign"):
            self.assertNotIn(f'KeyboardOptionsMenu.wnd:{obsolete}', LAYOUT)
        self.assertIn("COLUMNS: 2, COLUMNWIDTH: 75, COLUMNWIDTH: 25", LAYOUT)
        self.assertIn("\nENDALLCHILDREN\nEND\n", LAYOUT)
        lines = [line.strip() for line in LAYOUT.splitlines()]
        self.assertEqual(lines.count("WINDOW"), lines.count("END"))

    def test_generalsx_keyboard_layout_uses_color_draw_paths(self):
        controls = re.split(r"(?m)^\s*WINDOW\s*$", LAYOUT)[1:]
        for control in controls:
            status_match = re.search(r"(?m)^\s*STATUS = ([^;]+);", control)
            if not status_match:
                continue
            images = re.findall(r"IMAGE:\s*([^,;]+)", control)
            if images and all(image.strip() == "NoImage" for image in images):
                self.assertNotIn("IMAGE", status_match.group(1).split("+"))

    def test_keyboard_menu_compile_and_update_registration(self):
        for game in ("Generals", "GeneralsMD"):
            menu = (ROOT / game / "Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/KeyboardOptionsMenu.cpp").read_text()
            lexicon = (ROOT / game / "Code/GameEngine/Source/Common/System/FunctionLexicon.cpp").read_text()
            self.assertIn('#include "GameClient/GadgetPushButton.h"', menu)
            self.assertIn('"KeyboardOptionsMenuUpdate",', lexicon)
            self.assertIn("(void*)KeyboardOptionsMenuUpdate", lexicon)
            self.assertIn("findRequiredControl", menu)
            self.assertIn("Missing required control: %s", menu)

    def test_list_rows_store_actions_and_current_bindings(self):
        menu = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/KeyboardOptionsMenu.cpp").read_text()
        self.assertIn("isLogicalRepresentative", menu)
        self.assertIn("GadgetListBoxSetItemData(s_commands, action, row)", menu)
        self.assertIn("GadgetListBoxGetItemData(s_commands, s_selectedRow)", menu)
        self.assertIn("bindingText(action->m_key, action->m_modState)", menu)
        self.assertIn('FETCH_OR_SUBSTITUTE("GUI:Unbound", L"Unbound")', menu)
        for action in ("MSG_META_CAMERA_PAN_UP", "MSG_META_CAMERA_PAN_DOWN", "MSG_META_CAMERA_PAN_LEFT", "MSG_META_CAMERA_PAN_RIGHT"):
            self.assertIn(action, META)

    def test_action_names_have_grouped_readable_fallbacks(self):
        menu = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/KeyboardOptionsMenu.cpp").read_text()
        self.assertIn('text.startsWith(L"MISSING:")', menu)
        self.assertIn("explicitActionName", menu)
        self.assertIn("prettifyActionName", menu)
        self.assertIn("actionDisplayName(action)", menu)
        self.assertNotIn("GadgetListBoxAddEntryText(s_commands, action->m_displayName", menu)
        for fallback in ("Create Team %d", "Select Team %d", "Set Bookmark %d", "View Bookmark %d"):
            self.assertIn(fallback, menu)

        expected_groups = ("CAMERA CONTROLS", "SELECTION CONTROLS", "UNIT COMMANDS", "CONTROL GROUPS",
                           "BOOKMARKS", "INTERFACE", "MISCELLANEOUS")
        group_names = menu[menu.index("static UnicodeString groupDisplayName"):menu.index("static void updateSelectedBinding")]
        positions = [group_names.index(group) for group in expected_groups]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("GadgetListBoxSetItemData(s_commands, nullptr, headerRow)", menu)
        self.assertIn("GadgetListBoxSetTopVisibleEntry(s_commands, 0)", menu)

    def test_list_readability_and_selection_highlight(self):
        listbox = LAYOUT[LAYOUT.index('NAME = "KeyboardOptionsMenu.wnd:ListBoxCommandList";'):
                         LAYOUT.index("  END", LAYOUT.index('NAME = "KeyboardOptionsMenu.wnd:ListBoxCommandList";'))]
        self.assertIn('FONT = NAME: "Arial", SIZE: 20', listbox)
        self.assertIn("ENABLED+MOUSETRACK", listbox)
        self.assertNotIn("ENABLED+IMAGE", listbox)
        self.assertIn("HILITEDRAWDATA = IMAGE: NoImage, COLOR: 12 20 36 220", listbox)
        self.assertIn("COLOR: 35 100 165 255, BORDERCOLOR: 255 210 80 255", listbox)
        self.assertIn("COLOR: 50 125 195 255, BORDERCOLOR: 255 225 100 255", listbox)
        self.assertIn("SCROLLBAR: 1", listbox)

    def test_colored_listbox_uses_color_selection_renderer(self):
        colored = W3D_LISTBOX[W3D_LISTBOX.index("void W3DGadgetListBoxDraw"):
                              W3D_LISTBOX.index("void W3DGadgetListBoxImageDraw")]
        image = W3D_LISTBOX[W3D_LISTBOX.index("void W3DGadgetListBoxImageDraw"):]
        self.assertIn("drawListBoxText( window, instData, x, y + 4 , width, height-4, FALSE )", colored)
        self.assertNotIn("height-4, TRUE", colored)
        self.assertIn("drawListBoxText( window, instData, x, y+4, width, height-4, TRUE )", image)

    def test_keyboard_menu_is_an_options_subpage_not_a_shell_screen(self):
        options = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp").read_text()
        menu = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/KeyboardOptionsMenu.cpp").read_text()
        keyboard_branch = options[options.index("else if ( controlID == buttonKeyboardOptionsMenu )") :]
        keyboard_branch = keyboard_branch[:keyboard_branch.index("else if", 8)]
        self.assertIn("ShowKeyboardOptionsMenu()", keyboard_branch)
        self.assertNotIn("TheShell->push", keyboard_branch)
        self.assertNotIn("TheShell->pop", menu)
        self.assertIn("s_optionsLayout->hide(TRUE)", menu)
        self.assertIn('winCreateLayout("Menus/KeyboardOptionsMenu.wnd")', menu)
        self.assertIn("optionsLayout->hide(FALSE)", menu)
        self.assertIn("optionsLayout->bringForward()", menu)
        self.assertIn("CloseKeyboardOptionsMenu()", menu)

    def test_keyboard_menu_modal_and_escape_ownership(self):
        menu = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/KeyboardOptionsMenu.cpp").read_text()
        init = menu[menu.index("void KeyboardOptionsMenuInit"):menu.index("void KeyboardOptionsMenuShutdown")]
        self.assertLess(init.index("findRequiredControl"), init.index("winSetModal(s_parent)"))
        self.assertLess(init.index("winSetModal(s_parent)"), init.index("winSetFocus(s_parent)"))
        close = menu[menu.index("void CloseKeyboardOptionsMenu"):menu.index("void KeyboardOptionsMenuInit")]
        self.assertIn('nameToKey("OptionsMenu.wnd:OptionsMenuParent")', close)
        self.assertLess(close.index("winSetModal(optionsParent)"), close.index("winSetFocus(optionsParent)"))

        input_callback = menu[menu.index("WindowMsgHandledType KeyboardOptionsMenuInput"):
                              menu.index("WindowMsgHandledType KeyboardOptionsMenuSystem")]
        escape_down = input_callback[input_callback.index("if (state & KEY_STATE_DOWN)"):
                                     input_callback.index("if (state & KEY_STATE_UP)")]
        escape_up = input_callback[input_callback.index("if (state & KEY_STATE_UP)"):]
        self.assertIn("s_captureMode", escape_down)
        self.assertIn("cancelCapture()", escape_down)
        self.assertNotIn("CloseKeyboardOptionsMenu()", escape_down)
        self.assertIn("s_suppressEscapeUp", escape_up)
        self.assertIn("CloseKeyboardOptionsMenu()", escape_up)
        system_callback = menu[menu.index("WindowMsgHandledType KeyboardOptionsMenuSystem"):]
        self.assertIn("if (id == s_backID) CloseKeyboardOptionsMenu()", system_callback)

    def test_zero_hour_options_button_and_packaging_are_wired(self):
        options = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp").read_text()
        self.assertIn("createKeyboardOptionsButton(parent)", options)
        self.assertIn('OptionsMenu.wnd:ButtonKeyboardOptions', options)
        self.assertNotIn('TheShell->push( "Menus/KeyboardOptionsMenu.wnd" )', options)
        helper = options[options.index("static GameWindow *createKeyboardOptionsButton") : options.index("void OptionsMenuInit")]
        self.assertNotIn("if (button)\n\t\treturn button;", helper)
        self.assertIn("if (!button)", helper)
        self.assertIn("gogoGadgetPushButton", helper)
        self.assertIn("button->winHide(FALSE)", helper)
        self.assertIn("button->winEnable(TRUE)", helper)
        for control in ("ButtonDefaults", "ButtonAccept", "ButtonBack"):
            self.assertIn(f'OptionsMenu.wnd:{control}', helper)
        self.assertIn("winGetPosition", helper)
        self.assertIn("winGetSize", helper)
        self.assertIn("GameWindow *buttons[4] = { button, defaults, accept, back }", helper)
        self.assertIn("buttons[i]->winSetPosition", helper)
        self.assertIn("buttons[i]->winSetSize", helper)
        for deploy in ("scripts/build/linux/deploy-linux-zh.sh", "scripts/build/macos/deploy-macos-zh.sh"):
            self.assertIn("ExtrasMenu KeyboardOptionsMenu", (ROOT / deploy).read_text())
        self.assertIn("KeyboardOptionsMenu.wnd", (ROOT / "flatpak/com.fbraz3.GeneralsXZH.yml").read_text())


if __name__ == "__main__":
    unittest.main()
