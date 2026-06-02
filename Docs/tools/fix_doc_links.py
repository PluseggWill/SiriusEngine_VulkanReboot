#!/usr/bin/env python3
"""Apply known relative-path fixes for Docs/Archived/plans and related files."""
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PLANS = REPO / "Docs" / "Archived" / "plans"


def fix_plans_dir() -> None:
    replacements = [
        ("](Active-Plan.md)", "](../../Active-Plan.md)"),
        ("](../Active-Plan.md)", "](../../Active-Plan.md)"),
        ("](Archived-Plan.md)", "](../../Archived-Plan.md)"),
        ("](EngineArchitecture.md)", "](../../EngineArchitecture.md)"),
        ("](../EngineArchitecture.md)", "](../../EngineArchitecture.md)"),
        ("](forward-rendering-epic_Plan.md)", "](../../forward-rendering-epic_Plan.md)"),
        ("](../forward-rendering-epic_Plan.md)", "](../../forward-rendering-epic_Plan.md)"),
        ("](../forward-stage1.md)", "](../../forward-stage1.md)"),
        ("](forward-stage1.md)", "](../../forward-stage1.md)"),
        ("](SceneJSON.md)", "](../../SceneJSON.md)"),
        ("](SceneJSON.en.md)", "](../../SceneJSON.en.md)"),
        ("](../Data/", "](../../../Data/"),
        ("](Archived/plans/descriptor-strategy_Plan.md)", "](descriptor-strategy_Plan.md)"),
        ("](Archived/plans/descriptor-layout-verify_Plan.md)", "](descriptor-layout-verify_Plan.md)"),
        ("](Archived/plans/shader-reflection_Plan.md)", "](shader-reflection_Plan.md)"),
        ("](Archived/plans/pipeline-dynamic-state_Plan.md)", "](pipeline-dynamic-state_Plan.md)"),
    ]
    for path in PLANS.glob("*.md"):
        text = path.read_text(encoding="utf-8")
        orig = text
        for old, new in replacements:
            text = text.replace(old, new)
        if text != orig:
            path.write_text(text, encoding="utf-8")
            print(f"fixed: {path.relative_to(REPO)}")


def fix_s1_summary() -> None:
    path = REPO / "Docs" / "Archived" / "S1-回顾总结.md"
    text = path.read_text(encoding="utf-8")
    orig = text
    text = text.replace("](../Data/LOD.md)", "](../../Data/LOD.md)")
    text = text.replace("](EngineArchitecture.md)", "](../EngineArchitecture.md)")
    text = text.replace("](README.md)", "](../README.md)")
    if text != orig:
        path.write_text(text, encoding="utf-8")
        print(f"fixed: {path.relative_to(REPO)}")


def fix_cursor_rules() -> None:
    fixes = {
        REPO / ".cursor" / "rules" / "docs-roadmap-arch-sync.mdc": [
            ("](Docs/README.md)", "](../../Docs/README.md)"),
        ],
        REPO / ".cursor" / "rules" / "vulkan-smoke-test.mdc": [
            ("](Docs/CLI.md)", "](../../Docs/CLI.md)"),
        ],
    }
    for path, replacements in fixes.items():
        text = path.read_text(encoding="utf-8")
        orig = text
        for old, new in replacements:
            text = text.replace(old, new)
        if text != orig:
            path.write_text(text, encoding="utf-8")
            print(f"fixed: {path.relative_to(REPO)}")


def main() -> None:
    fix_plans_dir()
    fix_s1_summary()
    fix_cursor_rules()


if __name__ == "__main__":
    main()
