#!/usr/bin/env python3
"""Run source-only tests without downloading or importing game artwork."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-sanitize", action="store_true")
    parser.add_argument("--nes", action="store_true", help="Also run the optional NES core host checks")
    args = parser.parse_args()
    subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_*.py", "-v"],
                   cwd=ROOT, check=True)
    compiler = os.environ.get("CXX") or shutil.which("c++") or shutil.which("g++")
    if not compiler:
        raise SystemExit("A C++11 compiler is required for the gameplay checks")
    target = ROOT / ".local" / "tests"
    target.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c++11", "-Wall", "-Wextra", "-Werror"]
    if not args.no_sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    for name in ("hockey_game_test", "hockey_protocol_test", "game_home_test", "mario_world_clock_test", "tetris_clock_test", "jetpack_clock_test", "geometry_clock_test", "galaga_clock_test", "arkanoid_clock_test", "bitty_ui_test", "mario_clock_test", "speaker_audio_test", "cartridge_store_test", "library_protocol_test", "jetpack_placement_test", "game_core_test", "app_model_test", "arcade_support_test", "galaga_game_test", "arkanoid_game_test", "jetpack_game_test", "jetpack_art_test", "jetpack_fairness_test", "jetpack_progression_test", "jetpack_rewards_test", "jetpack_missions_test", "jetpack_profile_test", "jetpack_spin_test", "jetpack_bonus_integration_test", "jetpack_time_step_test", "jetpack_rocket_time_test", "jetpack_rocket_course_test", "jetpack_vehicle_physics_test", "coop_protocol_test", "controller_input_test", "ble_radio_test", "nes_viewport_test", "nes_system_button_test", "nes_sound_events_test", "piezo_sound_test", "piezo_output_test", "mario_viewport_test", "rom_fingerprint_test", "geometry_input_test", "geometry_scene_test", "geometry_collision_test", "geometry_level_test", "geometry_renderer_test", "geometry_scanorder_test", "geometry_launcher_test", "tetris_input_test", "tetris_actions_test", "tetris_viewport_test", "tetris_zero_viewport_test", "menu_font_test", "display_damage_test", "async_display_test", "inactivity_test"):
        if not (ROOT / "tests" / (name + ".cpp")).is_file():
            continue
        executable = target / (name + (".exe" if os.name == "nt" else ""))
        target_flags = flags + (["-O2"] if name == "jetpack_fairness_test" else [])
        subprocess.run([compiler] + target_flags + (["-pthread"] if name == "async_display_test" else []) + [str(ROOT / "tests" / (name + ".cpp")), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        if name == "jetpack_rocket_course_test":
            for fixture in (1, 2):
                alternate = target / ("jetpack_rocket_fixture_" + str(fixture))
                subprocess.run([compiler] + flags + ["-DJETPACK_ROCKET_TEST_FIXTURE=" + str(fixture),
                                str(ROOT / "tests" / (name + ".cpp")), "-o", str(alternate)], check=True)
                subprocess.run([str(alternate)], check=True)
        if name == "async_display_test":
            alternate = target / "async_display_three_test"
            subprocess.run([compiler] + flags + ["-pthread", "-DM5FLAP_DISPLAY_QUEUE_CAPACITY=3",
                            str(ROOT / "tests" / "async_display_test.cpp"), "-o", str(alternate)], check=True)
            subprocess.run([str(alternate)], check=True)
    sys.path.insert(0, str(ROOT / "tests"))
    from test_asset_import import make_fixture
    from generate_assets import generate
    from generate_skins import generate as generate_skins
    with tempfile.TemporaryDirectory(dir=target) as temporary:
        fixture = Path(temporary)
        atlas, manifest = make_fixture(fixture)
        assets = fixture / "generated"
        generate(atlas.read_bytes(), manifest.read_bytes(), {}, assets)
        generate_skins([], assets)
        for name in ("render_features_test", "flappy_clock_test", "clock_runtime_test"):
            source = ROOT / "tests" / (name + ".cpp")
            if not source.is_file():
                continue
            executable = fixture / (name + (".exe" if os.name == "nt" else ""))
            subprocess.run([compiler] + flags + ["-I", str(assets), str(source), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
        preview = fixture / ("render_preview.exe" if os.name == "nt" else "render_preview")
        subprocess.run([compiler] + flags + ["-I", str(assets), str(ROOT / "tests" / "render_preview.cpp"),
                                            "-o", str(preview)], check=True)
        for width, height in ((80, 160), (135, 240), (144, 256)):
            for state in (0, 1, 2):
                subprocess.run([str(preview), str(width), str(height), str(state)],
                               stdout=subprocess.DEVNULL, check=True)
    if args.nes:
        nes_checks = ROOT / "tools" / "test_nes.py"
        if not nes_checks.is_file():
            raise SystemExit("NES core host checks are unavailable: tools/test_nes.py is missing")
        subprocess.run([sys.executable, str(nes_checks)], cwd=ROOT, check=True)
    print("Source-only checks passed. No proprietary assets were used.")


if __name__ == "__main__":
    main()
