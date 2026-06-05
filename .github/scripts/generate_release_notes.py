import os
import sys

TEMPLATE_PATH = os.path.join(os.path.dirname(__file__), "..", "release_template.md")
OUTPUT_PATH = "release_notes.md"

replacements = {
    "__REPO__": os.environ["REPO"],
    "__VERSION__": os.environ["VERSION"],
    "__BASE_URL__": os.environ["BASE_URL"],
    "__COMMIT_LOG__": os.environ["COMMIT_LOG"],
    "__BUILD_DATE__": os.environ["BUILD_DATE"],
    "__COMMIT_HASH__": os.environ["COMMIT_HASH"],
    "__CHANGELOG_URL__": os.environ["CHANGELOG_URL"],
}

with open(TEMPLATE_PATH, "r", encoding="utf-8") as f:
    content = f.read()

for key, value in replacements.items():
    content = content.replace(key, value)

with open(OUTPUT_PATH, "w", encoding="utf-8") as f:
    f.write(content)

print(f"Generated {OUTPUT_PATH}")
