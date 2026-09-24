#!/usr/bin/env python3
"""Source-level integration checks for the legacy-engine keybinding wiring."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
META = (ROOT / "Core/GameEngine/Source/GameClient/MessageStream/MetaEvent.cpp").read_text()
LOOK = (ROOT / "Core/GameEngine/Source/GameClient/MessageStream/LookAtXlat.cpp").read_text()
HEADER = (ROOT / "Core/GameEngine/Include/GameClient/MetaEvent.h").read_text()
LAYOUT = (ROOT / "GeneralsZH/Data/Window/Menus/KeyboardOptionsMenu.wnd").read_text()


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
            "ComboBoxCategoryList",
            "ListBoxCommandList",
            "StaticTextDescription",
            "StaticTextCurrentHotkey",
            "TextEntryAssignHotkey",
            "ButtonAssign",
            "ButtonResetAll",
        ):
            self.assertEqual(LAYOUT.count(f'NAME = "KeyboardOptionsMenu.wnd:{control}";'), 1)
        lines = [line.strip() for line in LAYOUT.splitlines()]
        self.assertEqual(lines.count("WINDOW"), lines.count("END"))

    def test_zero_hour_options_button_and_packaging_are_wired(self):
        options = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp").read_text()
        self.assertIn("createKeyboardOptionsButton(parent)", options)
        self.assertIn('OptionsMenu.wnd:ButtonKeyboardOptions', options)
        self.assertIn('TheShell->push( "Menus/KeyboardOptionsMenu.wnd" )', options)
        for deploy in ("scripts/build/linux/deploy-linux-zh.sh", "scripts/build/macos/deploy-macos-zh.sh"):
            self.assertIn("ExtrasMenu KeyboardOptionsMenu", (ROOT / deploy).read_text())
        self.assertIn("KeyboardOptionsMenu.wnd", (ROOT / "flatpak/com.fbraz3.GeneralsXZH.yml").read_text())


if __name__ == "__main__":
    unittest.main()
