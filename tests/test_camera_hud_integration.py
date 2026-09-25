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


def artwork_offset(current_marker, canonical_marker, scale):
    return current_marker - round(canonical_marker * scale)


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

    def test_layout_uses_safe_color_rendering_and_focusable_root(self):
        self.assertNotIn("+IMAGE", LAYOUT)
        self.assertNotIn("ValueControlBarScale\"", LAYOUT)
        self.assertNotIn("UnusedSlider", LAYOUT)
        root = LAYOUT[LAYOUT.index('NAME = "CameraHudOptionsMenu.wnd:CameraHudOptionsMenuParent"'):]
        self.assertIn("STATUS = ENABLED;", root[:200])
        self.assertNotIn("NOFOCUS", root[:200])
        for slider in ("SliderMaximumZoomOut", "SliderTerrainDrawDistance", "SliderControlBarScale"):
            block = LAYOUT[LAYOUT.index(f'NAME = "CameraHudOptionsMenu.wnd:{slider}"') - 120:]
            block = block[:block.index("  END")]
            self.assertNotIn("STATUS = ENABLED+IMAGE", block)

    def test_scheme_change_restores_initializes_recaptures_then_scales(self):
        control_bar = (ROOT / "Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp").read_text()
        for method, manager_call in (
            ("setControlBarSchemeByPlayer(Player *p)", "setControlBarSchemeByPlayer(p)"),
            ("setControlBarSchemeByPlayerTemplate( const PlayerTemplate *pt)", "setControlBarSchemeByPlayerTemplate(pt)"),
            ("setControlBarSchemeByName(const AsciiString& name)", "setControlBarScheme(name)"),
        ):
            body = control_bar[control_bar.index(method):]
            body = body[:body.index("\n}")]
            order = [body.index(token) for token in (
                "restoreCanonicalControlBarGeometry()", manager_call,
                "captureCanonicalControlBarGeometry()", "applyConfiguredControlBarScale()")]
            self.assertEqual(order, sorted(order))
        capture = control_bar[control_bar.index("void ControlBar::captureCanonicalControlBarGeometry"):]
        capture = capture[:capture.index("\n}")]
        self.assertIn("CP_MASTER", capture)
        self.assertIn("CP_PURCHASE_SCIENCE", capture)
        self.assertIn("m_specialPowerShortcutParent", capture)

    def test_artwork_and_window_share_one_transform(self):
        device = (ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DControlBar.cpp").read_text()
        scheme = (ROOT / "Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp").read_text()
        self.assertIn("CalculateControlBarArtworkOffset", device)
        self.assertNotIn("ScaleControlBarCoordinate(offset", scheme)
        pivot, canonical_marker, image_x, stage_delta = 400, 100, 130, 47
        for scale in (1.0, .75, .60):
            marker = scale_coordinate(canonical_marker + stage_delta, pivot, scale)
            drawn = round(image_x * scale) + artwork_offset(marker, canonical_marker, scale)
            expected = scale_coordinate(image_x + stage_delta, pivot, scale)
            self.assertEqual(drawn, expected)
        scale = .75
        default = scale_coordinate(image_x, pivot, scale)
        low = scale_coordinate(image_x + stage_delta, pivot, scale)
        restored = scale_coordinate(image_x, pivot, scale)
        self.assertEqual(default, restored)
        self.assertNotEqual(default, low)
        control_bar = (ROOT / "Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp").read_text()
        default_stage = control_bar[control_bar.index("void ControlBar::setDefaultControlBarConfig"):control_bar.index("void ControlBar::setSquishedControlBarConfig")]
        low_stage = control_bar[control_bar.index("void ControlBar::setLowControlBarConfig"):control_bar.index("void ControlBar::setHiddenControlBar")]
        self.assertIn("applyConfiguredControlBarScale()", default_stage)
        self.assertNotIn("captureCanonicalControlBarGeometry", low_stage)

    def test_live_camera_reapplies_current_height_to_effective_limits(self):
        for game in ("Generals", "GeneralsMD"):
            page = (ROOT / game / "Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/CameraHudOptionsMenu.cpp").read_text()
            camera = page[page.index("const Real currentHeight"):page.index("if (TheControlBar)")]
            self.assertLess(camera.index("getHeightAboveGround"), camera.index("setCameraHeightAboveGroundLimitsToDefault"))
            self.assertIn("setHeightAboveGround(currentHeight)", camera)
            self.assertNotIn("getHeightAboveGround() > zoom", page)

    def test_options_row_reuses_the_full_five_button_span(self):
        for game in ("Generals", "GeneralsMD"):
            options = (ROOT / game / "Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp").read_text()
            helper = options[options.index("static GameWindow *createKeyboardOptionsButton"):options.index("void OptionsMenuInit")]
            self.assertIn("rowAlreadyCreated", helper)
            self.assertIn("sourceCount = rowAlreadyCreated ? 5 : 3", helper)
            self.assertIn("sourceOffset = rowAlreadyCreated ? 0 : 2", helper)

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
