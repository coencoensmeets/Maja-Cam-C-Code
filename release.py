#!/usr/bin/env python3
import os
import sys
import subprocess
import re
from pathlib import Path

def check_python_version():
    if sys.version_info < (3, 7):
        print("ERROR: Python 3.7 or higher is required.")
        sys.exit(1)

def run(cmd, check=True, capture_output=False, text=True, **kwargs):
    try:
        return subprocess.run(cmd, check=check, capture_output=capture_output, text=text, **kwargs)
    except subprocess.CalledProcessError as e:
        print(f"\nERROR: Command failed: {' '.join(cmd)}")
        if e.stdout:
            print(e.stdout)
        if e.stderr:
            print(e.stderr)
        sys.exit(e.returncode)

def ensure_in_project_root():
    if not Path('main/ota_manager.h').exists():
        print("ERROR: Script must be run from the Poem_cam project root directory.")
        sys.exit(1)

def ensure_git():
    try:
        run(["git", "--version"], capture_output=True)
    except Exception:
        print("ERROR: git is not installed or not in PATH.")
        sys.exit(1)

def ensure_gh():
    try:
        run(["gh", "--version"], capture_output=True)
        return True
    except Exception:
        print("WARNING: GitHub CLI (gh) not found. Releases will require manual upload.")
        return False

def get_all_tags():
    run(["git", "fetch", "--tags", "--force"])
    tags = run(["git", "tag", "-l"], capture_output=True).stdout.splitlines()
    return tags

def get_latest_base_version(tags):
    semver = re.compile(r"^v(\d+)\.(\d+)\.(\d+)(?:-.*)?$")
    versions = [t for t in tags if semver.match(t)]
    if not versions:
        return None
    # extract bases
    bases = set()
    for v in versions:
        m = re.match(r"v(\d+)\.(\d+)\.(\d+)", v)
        if m:
            bases.add(f"v{m.group(1)}.{m.group(2)}.{m.group(3)}")
    if not bases:
        return None
    bases = list(bases)
    bases.sort(key=lambda s: list(map(int, s[1:].split('.'))))
    return bases[-1]

def get_latest_test_tag(tags, base_version):
    testver = re.compile(rf"^{base_version}-test\.(\d+)$")
    test_tags = [(t, int(testver.match(t).group(1))) for t in tags if testver.match(t)]
    if not test_tags:
        return None, 0
    test_tags.sort(key=lambda x: x[1])
    return test_tags[-1][0], test_tags[-1][1]

def get_code_version():
    with open('main/ota_manager.h') as f:
        for line in f:
            m = re.search(r'FIRMWARE_VERSION\s+\"([^"]+)\"', line)
            if m:
                return m.group(1)
    return None

def set_code_version(new_version):
    path = Path('main/ota_manager.h')
    text = path.read_text()
    new_text = re.sub(r'(FIRMWARE_VERSION\s+\")[^\"]+(\")', rf'\1{new_version}\2', text)
    path.write_text(new_text)
    # ...existing code...

def main_menu():
    print("\n========================================")
    print("  ESP32 Firmware Manager (Python)")
    print("========================================\n")
    tags = get_all_tags()
    latest = get_latest_base_version(tags)
    print(f"Current version (from Git): {latest if latest else 'None'}")
    print("\n[1] Create official release (vX.Y.Z)")
    print("[2] Create testing build")
    print("[3] Delete tags")
    print("[4] Sync tags with GitHub")
    print("[0] Exit\n")
    choice = input("Select option (0-4): ").strip()
    if choice == "1":
        create_official_release(tags)
    elif choice == "2":
        create_testing_build(tags)
    elif choice == "3":
        delete_tags(tags)
    elif choice == "4":
        sync_tags()
    elif choice == "0":
        print("Goodbye!")
        sys.exit(0)
    else:
        print("Invalid choice.")
        main_menu()

def create_official_release(tags):
    latest = get_latest_base_version(tags)
    print(f"\nLatest official version: {latest if latest else 'None'}")
    new_version = input("Enter new version (X.Y.Z): ").strip()
    if not re.match(r"^\d+\.\d+\.\d+$", new_version):
        print("ERROR: Invalid version format.")
        return main_menu()
    tag_name = f"v{new_version}"
    if tag_name in tags:
        print("ERROR: Tag already exists.")
        return main_menu()
    set_code_version(tag_name)
    run(["git", "add", "main/ota_manager.h"])
    run(["git", "commit", "-m", f"Bump version to {tag_name}"])
    print(f"\nWill build official release: {tag_name}")
    input("\n[MANUAL STEP] Please build the firmware now (e.g., run 'idf.py build' in your terminal). Press Enter to continue once the build is complete...")
    firmware_bin = "build/Poem_cam.bin"
    notes = get_release_notes()
    run(["git", "tag", "-a", tag_name, "-m", notes])
    run(["git", "push", "origin", "HEAD"])
    run(["git", "push", "origin", tag_name])
    if os.path.exists(firmware_bin):
        if ensure_gh():
            run(["gh", "release", "create", tag_name, "--generate-notes"])
            run(["gh", "release", "upload", tag_name, f"{firmware_bin}#Poem_cam.bin", "--clobber"])
            print(f"Firmware uploaded to GitHub release {tag_name}.")
        else:
            print("GitHub CLI not available. Please upload firmware manually.")
    else:
        print(f"Warning: Firmware binary not found at {firmware_bin}. Please upload manually.")
    print("\nOfficial release complete.")
    main_menu()

def create_testing_build(tags):
    base_version_full = get_latest_base_version(tags)
    base_version = base_version_full.lstrip('v') if base_version_full else "0.0.0"
    _, max_test = get_latest_test_tag(tags, f"v{base_version}")
    next_test = max_test + 1
    tag_name = f"v{base_version}-test.{next_test}"
    original_version = get_code_version()
    set_code_version(tag_name)
    print(f"\nWill build [TEST] release: {tag_name}")
    input("\n[MANUAL STEP] Please build the firmware now (e.g., run 'idf.py build' in your terminal). Press Enter to continue once the build is complete...")
    firmware_bin = "build/Poem_cam.bin"
    set_code_version(original_version)  # revert the version change
    notes = get_release_notes()
    run(["git", "tag", "-a", tag_name, "-m", notes])
    run(["git", "push", "origin", tag_name])
    if os.path.exists(firmware_bin):
        if ensure_gh():
            run(["gh", "release", "create", tag_name, "--generate-notes", "--title", f"[TEST] {tag_name}"])
            run(["gh", "release", "upload", tag_name, f"{firmware_bin}#Poem_cam.bin", "--clobber"])
            print(f"Firmware uploaded to [TEST] release {tag_name}.")
        else:
            print("GitHub CLI not available. Please upload firmware manually.")
    else:
        print(f"Warning: Firmware binary not found at {firmware_bin}. Please upload manually.")
    print(f"\n[TEST] build complete. Tag {tag_name} pushed to GitHub.")
    print(f"Version used for build: {tag_name}")
    main_menu()

def get_release_notes():
    print("\nEnter release notes (end with a single '.' on a line):")
    lines = []
    while True:
        line = input()
        if line.strip() == ".":
            break
        lines.append(line)
    return "\n".join(lines) if lines else "No notes."

def delete_tags(tags):
    print("\nTags:")
    for i, t in enumerate(tags):
        print(f"[{i+1}] {t}")
    idxs = input("Enter tag numbers to delete (comma separated): ").strip()
    if not idxs:
        return main_menu()
    idxs = [int(i)-1 for i in idxs.split(',') if i.strip().isdigit() and 0 < int(i) <= len(tags)]
    for i in idxs:
        tag = tags[i]
        run(["git", "tag", "-d", tag])
        run(["git", "push", "origin", f":refs/tags/{tag}"])
        if ensure_gh():
            try:
                run(["gh", "release", "delete", tag, "--yes"])
                print(f"Deleted release {tag}")
            except:
                print(f"Release {tag} not found or already deleted")
        else:
            print("GitHub CLI not available. Release may need manual deletion.")
        print(f"Deleted tag {tag}")
    main_menu()

def sync_tags():
    print("\nSyncing tags with GitHub...")
    run(["git", "tag", "-l"], capture_output=True)
    run(["git", "fetch", "--tags", "--force"])
    tags = get_all_tags()
    print("Tags synced.")
    if tags:
        print("\nCurrent tags:")
        for tag in tags:
            print(f"  {tag}")
    else:
        print("\nNo tags found.")
    main_menu()

if __name__ == "__main__":
    check_python_version()
    ensure_in_project_root()
    ensure_git()
    main_menu()
