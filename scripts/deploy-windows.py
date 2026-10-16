#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess

def get_imported_dlls(filepath):
    dlls = set()
    try:
        out = subprocess.check_output(
            ["objdump", "-p", filepath],
            stderr=subprocess.DEVNULL
        ).decode("latin-1", errors="ignore")
        for line in out.splitlines():
            if "DLL Name:" in line:
                dll = line.split("DLL Name:")[1].strip()
                dlls.add(dll)
    except Exception as e:
        print(f"Warning: could not inspect {filepath}: {e}")
    return dlls

def deploy(app_dir, msys_bin):
    app_dir = os.path.abspath(app_dir)
    exe_path = os.path.join(app_dir, "antimony.exe")
    if not os.path.isfile(exe_path):
        print(f"Error: {exe_path} not found")
        sys.exit(1)

    print(f"==> Deploying Qt with windeployqt to {app_dir}...")
    windeploy = shutil.which("windeployqt-qt5") or shutil.which("windeployqt") or os.path.join(msys_bin, "windeployqt-qt5.exe")
    if os.path.isfile(windeploy):
        subprocess.run([
            windeploy,
            "--no-angle",
            "--no-opengl-sw",
            "--compiler-runtime",
            "--no-translations",
            exe_path
        ], check=False)
    else:
        print("windeployqt not found, relying on recursive dependency copy")

    print(f"==> Resolving recursive DLL dependencies from {msys_bin}...")
    # Seed queue with all PE files in app_dir
    scanned = set()
    copied = set()

    while True:
        current_pes = []
        for root, dirs, files in os.walk(app_dir):
            for f in files:
                ext = f.lower()
                if ext.endswith(".exe") or ext.endswith(".dll"):
                    full_p = os.path.join(root, f)
                    if full_p not in scanned:
                        current_pes.append(full_p)
                        scanned.add(full_p)

        if not current_pes:
            break

        new_copies = 0
        for pe in current_pes:
            imported = get_imported_dlls(pe)
            for dll_name in imported:
                src_candidate = os.path.join(msys_bin, dll_name)
                dst = os.path.join(app_dir, dll_name)
                if os.path.isfile(src_candidate) and not os.path.exists(dst):
                    print(f"  Copying dependency: {dll_name} (needed by {os.path.basename(pe)})")
                    shutil.copy2(src_candidate, dst)
                    copied.add(dll_name)
                    new_copies += 1

        if new_copies == 0:
            break

    print(f"==> Copied {len(copied)} dependency DLLs:")
    for d in sorted(copied):
        print(f"    - {d}")

    # Bundle Python standard library
    print("==> Bundling Python standard library...")
    py_ver = f"{sys.version_info.major}.{sys.version_info.minor}"
    py_lib_src = os.path.join(os.path.dirname(msys_bin), "lib", f"python{py_ver}")
    dest_py = os.path.join(app_dir, "lib", f"python{py_ver}")

    if os.path.isdir(py_lib_src):
        os.makedirs(os.path.join(app_dir, "lib"), exist_ok=True)
        if os.path.exists(dest_py):
            shutil.rmtree(dest_py)
        print(f"  Copying {py_lib_src} -> {dest_py}")
        shutil.copytree(py_lib_src, dest_py, ignore=shutil.ignore_patterns("test", "__pycache__", "idlelib", "tkinter"))

    print(f"==> Deployment complete for {app_dir}!")

if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "build/antimony"
    msys = sys.argv[2] if len(sys.argv) > 2 else "/ucrt64/bin"
    if not os.path.isdir(msys):
        msys = "C:/msys64/ucrt64/bin"
    deploy(target, msys)
