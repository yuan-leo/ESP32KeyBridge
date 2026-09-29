from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]


def test_required_project_files_exist():
    required = [
        "README.ja.md",
        "README.md",
        "library.properties",
        "keywords.txt",
        "CHANGELOG.md",
        "src/ESP32KeyBridge.h",
        "src/ESP32KeyBridge.cpp",
        "src/ESP32KeyBridgeBluetooth.h",
        "src/ESP32KeyBridgeMacroSettings.h",
        "src/ESP32KeyBridgeMacroSettings.cpp",
        "src/ESP32KeyBridgeMacroSettingsJson.h",
        "src/ESP32KeyBridgeMacroSettingsJson.cpp",
        "src/ESP32KeyBridgeSettings.h",
        "src/ESP32KeyBridgeSettings.cpp",
        "src/ESP32KeyBridgeSettingsJson.h",
        "src/ESP32KeyBridgeSettingsJson.cpp",
        "src/ESP32KeyBridgeSettingsNavigation.h",
        "src/ESP32KeyBridgePreferences.h",
        "src/ESP32KeyBridgeWeb.h",
        "src/ESP32KeyBridgeEspBle.h",
        "examples/README.ja.md",
        "examples/README.md",
        "docs/REQUIREMENTS.ja.md",
        "docs/SCOPE.md",
        "docs/SETTINGS.md",
        "docs/CORE_DESIGN.ja.md",
        "docs/ADAPTERS.ja.md",
        "docs/DATA_MODEL.ja.md",
        "docs/USE_CASES.ja.md",
        "docs/DECISIONS.ja.md",
        "docs/API_SKETCHES.ja.md",
        "docs/CONFIGURATION.ja.md",
        "docs/DEVELOPMENT_PLAN.ja.md",
        "docs/RELEASE_CHECKLIST.ja.md",
        "tests/README.ja.md",
        "tests/TEST_PLAN.ja.md",
        "tests/unit/core_smoke/core_smoke.ino",
        "tests/unit/core_smoke/sketch.yaml",
        "tests/unit/core_smoke/test_core_smoke.py",
        "tests/single/README.ja.md",
        "tests/peer/README.ja.md",
        "tests/peer/README.md",
        "tests/manual/README.ja.md",
        "tests/manual/README.md",
    ]

    missing = [path for path in required if not (ROOT / path).exists()]

    assert missing == []


def test_library_properties_names_public_header():
    library_properties = (ROOT / "library.properties").read_text(encoding="utf-8")

    assert "name=ESP32KeyBridge" in library_properties
    assert "includes=ESP32KeyBridge.h" in library_properties


def readme_section(readme, title):
    return readme.split(f"## {title}", 1)[1].split("\n## ", 1)[0]


def test_examples_readmes_list_current_examples():
    example_dirs = sorted(
        path.parent.name for path in (ROOT / "examples" / "practical").glob("*/*.ino")
    )
    for readme_name in ["README.ja.md", "README.md"]:
        readme = (ROOT / "examples" / readme_name).read_text(encoding="utf-8")
        listed = sorted(re.findall(r"- `([^`]+)`:", readme_section(readme, "Practical Examples")))

        assert listed == example_dirs, readme_name


def test_examples_are_grouped_with_docs_and_compile_profiles():
    missing = []
    for ino in sorted((ROOT / "examples").rglob("*.ino")):
        example_dir = ino.parent
        assert example_dir.parent.name == "practical", str(example_dir)
        for required in ["README.ja.md", "sketch.yaml"]:
            if not (example_dir / required).exists():
                missing.append(str((example_dir / required).relative_to(ROOT)))

    assert missing == []


def test_phase7_settings_portal_is_offline_authenticated_and_bounded():
    web = (ROOT / "src/ESP32KeyBridgeWeb.h").read_text(encoding="utf-8")

    assert 'server_.on("/", HTTP_GET' in web
    assert 'server_.on("/api/v1/config", HTTP_GET' in web
    assert 'server_.on("/api/v1/config", HTTP_PUT' in web
    assert 'server_.on("/api/v1/layouts", HTTP_GET' in web
    assert 'server_.on("/api/v1/navigation", HTTP_GET' in web
    assert 'server_.on("/api/v1/macros", HTTP_GET' in web
    assert 'server_.on("/api/v1/macros", HTTP_PUT' in web
    assert 'server_.on("/api/v1/status", HTTP_GET' in web
    assert 'server_.on("/healthz", HTTP_GET' in web
    assert "HTTP_POST" not in web
    assert web.count("HTTP_PUT") == 2
    assert "https://" not in web
    assert 'src="http' not in web
    assert 'href="http' not in web
    assert "#include <DNSServer.h>" in web
    assert 'dnsServer_.start(53, "*", WiFi.softAPIP())' in web
    assert 'server_.uri() != "/api"' in web
    assert '!server_.uri().startsWith("/api/")' in web
    assert 'String("http://") + WiFi.softAPIP().toString()' in web
    assert "InvalidPortalCredentials" in web
    assert "mode != DIGEST_AUTH" in web
    assert "username.equalsConstantTime(portalUsername_)" in web
    assert 'server_.requestAuthentication(DIGEST_AUTH, "ESP32 KeyBridge"' in web
    assert "BASIC_AUTH" not in web
