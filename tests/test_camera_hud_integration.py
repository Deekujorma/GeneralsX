#!/usr/bin/env python3
"""Focused source integration checks for Camera & HUD presentation options."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFS = (ROOT / "Core/GameEngine/Source/Common/OptionPreferences.cpp").read_text()
HUD = (ROOT / "Core/GameEngine/Include/GameClient/ControlBarHudScale.h").read_text()
LAYOUT = (ROOT / "GeneralsZH/Data/Window/Menus/CameraHudOptionsMenu.wnd").read_text()


def scale_coordinate(value, pivot, scale):
    scale = min(1.0, max(0.6, scale))
    return pivot + round((value - pivot) * scale)


def viewport(base, scale):
    return 1.0 - (1.0 - base) * min(1.0, max(0.6, scale))


class CameraHudIntegrationTests(unittest.TestCase):
    def test_preference_defaults_and_clamps(self):
        self.assertIn('find("MaxCameraHeight")', PREFS)
        self.assertIn("return 750.0f", PREFS)
        self.assertIn("val = 100.0f", PREFS)
        self.assertIn("val = 1000.0f", PREFS)
        self.assertIn("return 1.25f", PREFS)
        self.assertIn("return clamp(0.60f, val, 1.00f)", PREFS)
        self.assertIn("return 0.75f", PREFS)

    def test_global_data_applies_all_camera_preferences(self):
        for game in ("Generals", "GeneralsMD"):
            source = (ROOT / game / "Code/GameEngine/Source/Common/GlobalData.cpp").read_text()
            for getter in ("getMaxCameraHeight", "getMinCameraHeight", "getCameraPitch", "getTerrainDrawDistanceScale", "getControlBarScale"):
                self.assertIn(getter + "()", source)

    def test_bottom_center_geometry(self):
        self.assertEqual(scale_coordinate(400, 400, 1.0), 400)
        self.assertEqual(scale_coordinate(0, 400, .75), 100)
        self.assertEqual(scale_coordinate(600, 600, .75), 600)
        self.assertEqual(scale_coordinate(0, 400, .60), 160)
        canonical = 40
        self.assertEqual(scale_coordinate(canonical, 400, .75), scale_coordinate(canonical, 400, .75))
        self.assertNotEqual(scale_coordinate(scale_coordinate(canonical, 400, .75), 400, .80), scale_coordinate(canonical, 400, .80))
        self.assertIn("m_canonicalGeometry", (ROOT / "Core/GameEngine/Include/GameClient/ControlBarResizer.h").read_text())
        self.assertIn("ScaleControlBarCoordinate", HUD)

    def test_viewport_formula(self):
        self.assertAlmostEqual(viewport(.8, 1), .8)
        self.assertAlmostEqual(viewport(.8, .75), .85)
        self.assertAlmostEqual(viewport(.8, .60), .88)
        self.assertAlmostEqual(viewport(1, .60), 1)

    def test_options_contract_and_packaging(self):
        for token in ("SliderMaximumZoomOut", "MINVALUE: 500, MAXVALUE: 1000", "SliderTerrainDrawDistance", "MINVALUE: 100, MAXVALUE: 200", "SliderControlBarScale", "MINVALUE: 60, MAXVALUE: 100"):
            self.assertIn(token, LAYOUT)
        for game in ("Generals", "GeneralsMD"):
            options = (ROOT / game / "Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp").read_text()
            self.assertEqual(options.count('m_decoratedNameString = "OptionsMenu.wnd:ButtonCameraHudOptions"'), 1)
            self.assertIn("GameWindow *buttons[5]", options)
        page = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/CameraHudOptionsMenu.cpp").read_text()
        self.assertNotIn("TheShell->push", page)
        self.assertNotIn("TheShell->pop", page)
        self.assertIn("s_optionsLayout->hide(TRUE)", page)
        for deploy in ("scripts/build/linux/deploy-linux-zh.sh", "scripts/build/macos/deploy-macos-zh.sh", "flatpak/com.fbraz3.GeneralsXZH.yml"):
            self.assertIn("CameraHudOptionsMenu", (ROOT / deploy).read_text())


if __name__ == "__main__":
    unittest.main()
