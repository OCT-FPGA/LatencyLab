#!/usr/bin/env python3
# Flash a P4 bitstream and load driver tables.
#
# Usage:
#   sudo python3 flash_p4.py <name>     -> flashes ~/au280_<name>/<name>.mcs,
#                                          reloads driver, configures VLANs,
#                                          loads P4 tables from drivers/install
#   sudo python3 flash_p4.py golden     -> flashes golden_nic.mcs (found in cwd
#                                          or upwards, NOT in au280_*), brings
#                                          interfaces up, and STOPS. No driver
#                                          install / table loading.

import os
import subprocess
import sys
import time


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, **kwargs)


def run_quiet(cmd, check=True, **kwargs):
    return subprocess.run(
        cmd, check=check, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **kwargs
    )


def run_shell(cmd, **kwargs):
    return subprocess.run(cmd, check=True, shell=True, **kwargs)


def find_cmd(cmd):
    from shutil import which

    path = which(cmd)
    if path:
        return path
    for prefix in ("/opt/xilinx/xrt/bin", "/usr/local/bin", "/usr/sbin", "/sbin"):
        candidate = os.path.join(prefix, cmd)
        if os.path.exists(candidate):
            return candidate
    return ""


def ensure_cmd(cmd):
    path = find_cmd(cmd)
    if not path:
        print(f"error: required command not found: {cmd}", file=sys.stderr)
        sys.exit(1)
    return path


def ensure_exists(path, desc):
    if not os.path.exists(path):
        print(f"error: missing {desc}: {path}", file=sys.stderr)
        sys.exit(1)


def find_upwards(start_dir, filename, max_depth=6):
    cur = os.path.abspath(start_dir)
    for _ in range(max_depth + 1):
        candidate = os.path.join(cur, filename)
        if os.path.exists(candidate):
            return candidate
        parent = os.path.dirname(cur)
        if parent == cur:
            break
        cur = parent
    return ""


def run_as_user_quiet(cmd, cwd=None):
    user = os.environ.get("SUDO_USER")
    if user:
        return subprocess.run(
            ["sudo", "-u", user] + cmd,
            check=True,
            cwd=cwd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    return subprocess.run(
        cmd, check=True, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )


def bring_iface_up(sudo, iface):
    run_quiet([sudo, "ip", "link", "set", iface, "up"])


def main():
    if len(sys.argv) != 2:
        print("usage: sudo python3 flash_p4.py <name|golden>", file=sys.stderr)
        sys.exit(1)

    name = sys.argv[1]
    golden = (name == "golden")
    home = os.path.expanduser("~")
    cwd = os.getcwd()

    if golden:
        # GOLDEN MODE: directly use golden_nic.mcs -- no au280_* directory lookup.
        mcs_path = os.path.join(cwd, "golden_nic.mcs")
        ensure_exists(mcs_path, "golden_nic.mcs")
        driver_dir = None
        print(f"GOLDEN mode: flashing {mcs_path}; driver table install will be SKIPPED.")
    else:
        base_dir = os.path.join(home, f"au280_{name}")
        mcs_path = os.path.join(base_dir, f"{name}.mcs")
        driver_dir = os.path.join(base_dir, "drivers", "install")

        ensure_exists(base_dir, f"au280_{name} directory")
        ensure_exists(mcs_path, f"{name}.mcs")
        ensure_exists(driver_dir, "drivers/install directory")

    config_fpga = ensure_cmd("config-fpga")
    xbflash2 = ensure_cmd("xbflash2")
    make = ensure_cmd("make")
    rmmod = ensure_cmd("rmmod")
    insmod = ensure_cmd("insmod")
    dmesg = ensure_cmd("dmesg")
    sudo = ensure_cmd("sudo")

    key_path = os.path.join(cwd, "private_key.pem")
    if not os.path.exists(key_path):
        key_path = find_upwards(cwd, "private_key.pem")
    ensure_exists(key_path, "private_key.pem")

    print("Resetting FPGA...", flush=True)
    # 1) reset FPGA as current user
    run_as_user_quiet([config_fpga, "reset", key_path], cwd=cwd)
    print("Reset successful.", flush=True)

    print(f"Flashing bitstream: {mcs_path} ...", flush=True)
    # 2) flash bitstream
    run(
        [
            sudo,
            xbflash2,
            "program",
            "--spi",
            "--image",
            mcs_path,
            "-d",
            "3b:00.0",
            "--bar",
            "2",
            "3",
        ],
        input="y\n",
        text=True,
    )
    print("Flashing successful.", flush=True)

    print("Booting FPGA...", flush=True)
    # 3) boot FPGA as current user
    run_as_user_quiet([config_fpga, "boot", key_path], cwd=cwd)
    time.sleep(5)
    print("Boot successful.", flush=True)

    print("Rebuilding and reloading ONIC driver...", flush=True)
    # 4) rebuild and reload ONIC driver (needed for enp59s0f* to exist)
    onic_driver_dir = os.path.join(cwd, "opennic", "opennic-scripts", "open-nic-driver")
    if not os.path.exists(onic_driver_dir):
        onic_driver_dir = find_upwards(
            cwd, os.path.join("opennic", "opennic-scripts", "open-nic-driver")
        )
    ensure_exists(onic_driver_dir, "opennic open-nic-driver directory")
    run_shell(f"{sudo} {make} clean && {sudo} {make}", cwd=onic_driver_dir)
    run_quiet([sudo, rmmod, "onic"], check=False)  # ok if not loaded
    run_quiet([sudo, insmod, os.path.join(onic_driver_dir, "onic.ko"), "RS_FEC_ENABLED=0"])
    run_quiet([sudo, dmesg])
    time.sleep(10)
    print("ONIC driver reloaded.", flush=True)

    print("Bringing interfaces up (enp59s0f0, enp59s0f1)...", flush=True)
    # 5) bring interfaces up
    bring_iface_up(sudo, "enp59s0f0")
    bring_iface_up(sudo, "enp59s0f1")
    time.sleep(2)
    print("Interfaces up.", flush=True)

    if golden:
        print("flash_p4: done (golden: flashed + interfaces up; tables skipped)", flush=True)
        return

    print(f"Loading tables from {driver_dir} ...", flush=True)
    # 6) build driver and load tables
    driver_binary = os.path.join(driver_dir, "driver")
    run_shell(f"{make} clean && {make}", cwd=driver_dir)
    ensure_exists(driver_binary, "driver binary (build may have failed)")
    run([sudo, driver_binary], cwd=driver_dir)
    time.sleep(2)
    print("Tables loaded successfully.", flush=True)

    print("flash_p4: done", flush=True)


if __name__ == "__main__":
    main()
