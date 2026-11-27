"""Run with xvfb-run; optional second argument receives screenshots of the real windows."""
import hashlib
import json
import os
import shutil
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
from contextlib import closing, ExitStack
from pathlib import Path

from Xlib import X, display, error
from Xlib.ext import xtest
from x11_helpers import close_window, key, until, window


def exercise(exe, screenshots):
    with tempfile.TemporaryDirectory(prefix="hub-test-") as temp, ExitStack() as cleanup:
        root = Path(temp)
        runtime = root / "runtime"
        runtime.mkdir(mode=0o700)
        org = root / "data/sucez"
        hub_dir = org / "Acheron-Hub"
        env = dict(os.environ, HOME=temp, TMPDIR=temp, XDG_RUNTIME_DIR=str(runtime), XDG_DATA_HOME=str(root / "data"),
                   XDG_CONFIG_HOME=str(root / "config"), XDG_CACHE_HOME=str(root / "cache"),
                   DBUS_SESSION_BUS_ADDRESS="unix:path=" + temp + "/unused", QT_QPA_PLATFORM="xcb",
                   QT_SCALE_FACTOR="1", LC_ALL="C")
        x = display.Display()
        cleanup.callback(x.close)
        if screenshots:
            screenshots.mkdir(exist_ok=True, parents=True)

        def start(*args, binary=exe):
            child = subprocess.Popen([binary, *args], env=env)
            def stop():
                if child.poll() is None:
                    child.kill()
                child.wait()
            cleanup.callback(stop)
            return child

        def focus(win):
            win.configure(stack_mode=X.Above)
            win.set_input_focus(X.RevertToParent, X.CurrentTime)
            x.sync()

        def invisible(win):
            try:
                return win.get_attributes().map_state != X.IsViewable
            except error.BadWindow:
                return True

        def click(win, dx, dy):
            focus(win)
            origin = x.screen().root.translate_coords(win, dx, dy)
            xtest.fake_input(x, X.MotionNotify, x=origin.x, y=origin.y)
            xtest.fake_input(x, X.ButtonPress, 1)
            xtest.fake_input(x, X.ButtonRelease, 1)
            x.sync()

        def capture(win, filename):
            if screenshots:
                time.sleep(.15)
                subprocess.run(["import", "-window", str(win.id), str(screenshots / filename)], check=True)

        def control(directory, command):
            digest = hashlib.sha256(f"{directory}|{root}".encode()).hexdigest()[:32]
            with socket.socket(socket.AF_UNIX) as peer:
                peer.settimeout(2)
                peer.connect(str(runtime / ("acheron-v1-" + digest)))
                peer.sendall(command)
                return peer.recv(128)

        def profile(name):
            return org / ("Acheron-Custom-" + name)

        def close_detached_sessions():
            for lock in org.glob("Acheron-Custom-*/profile.lock"):
                try:
                    control(lock.parent, b"1 close\n")
                except OSError:
                    pass  # The private Xvfb display also exits at the end of the run.
        cleanup.callback(close_detached_sessions)

        def seed(name, username):
            folder = profile(name)
            folder.mkdir()
            with closing(sqlite3.connect(folder / "acheron.sqlite")) as db:
                db.execute("create table accounts(id integer primary key, username text, display_name text,"
                           " avatar text, gateway_url text, rest_url text, cdn_url text, display_order integer,"
                           " auto_connect integer default 0, proxy_url text)")
                # Zero is synthetic and connectAccount rejects it before looking up any credentials.
                db.execute("insert into accounts(id, username) values(0,?)", (username,))
                db.commit()

        def usage(label, pids):
            def ticks(pid):
                fields = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
                return int(fields[11]) + int(fields[12])
            def rss(pid):
                for line in Path(f"/proc/{pid}/status").read_text().splitlines():
                    if line.startswith("VmRSS:"):
                        return int(line.split()[1]) / 1024
            began = time.monotonic()
            before = sum(ticks(pid) for pid in pids)
            time.sleep(2)
            cpu = 100 * (sum(ticks(pid) for pid in pids) - before) / os.sysconf("SC_CLK_TCK")
            print(f"{label}: RSS MiB {[round(rss(pid), 1) for pid in pids]}, "
                  f"CPU {cpu / (time.monotonic() - began):.2f}% of one core over 2s", flush=True)

        started = time.monotonic()
        hub_process = start()
        hub = window(x, "Acheron Hub", hub_process.pid)
        print(f"Empty Hub visible in {time.monotonic() - started:.3f}s", flush=True)
        usage("Idle Hub", [hub_process.pid])
        assert not list(org.glob("Acheron-Custom-*"))
        assert start().wait(timeout=5) == 0
        assert int((hub_dir / "hub.lock").read_text().splitlines()[0]) == hub_process.pid
        capture(hub, "01-welcome.png")
        focus(hub)
        key(x, "n", "Control_L")
        prompt = window(x, "Add an account", hub_process.pid)
        capture(prompt, "04-add.png")
        close_window(x, prompt)
        assert not list(org.glob("Acheron-Custom-*"))
        close_window(x, hub)
        assert hub_process.wait(timeout=5) == 0

        seed("atelier", "atelier.demo")
        one = start()
        only = window(x, "Acheron Hub", one.pid)
        assert not (profile("atelier") / "profile.lock").exists()
        close_window(x, only)
        assert one.wait(timeout=5) == 0
        seed("luna", "luna.studio")
        # A synthetic 48px moon exercises the bounded local portrait path.
        portrait = "iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAIAAADYYG7QAAAAqklEQVR4nO3WwQmAMBQDUHdyKG+OLLiCEygIXjz0J238pQZyrT7zi+00L2tXmdIFBhlkkEHZgrFA27ET74BWAaDruXdQDbRK29CjiZvkI1M1xM2L+IwQCK29JmUQsQ/+BPpYA4OSG3prDDKoAUhtYkBS0yggnYkHiUzwn1rNagBqa4JP+6CPtkpANeXBN0apJgqiBycEQSyOwoCKrBoKD5LGIIMMMig73YFOzevg/dgQTl4AAAAASUVORK5CYII="
        (profile("luna") / "hub-card.json").write_text(json.dumps(
            {"schema": 1, "caption": "@luna.studio", "avatarKey": "0/test-avatar", "png": portrait}))
        with closing(sqlite3.connect(profile("luna") / "acheron.sqlite")) as db:
            db.execute("update accounts set avatar='test-avatar'")
            db.commit()
        seed("voyage", "Étoile_Ω_" + "horizon" * 14)
        hub_process = start()
        hub = window(x, "Acheron Hub", hub_process.pid)
        focus(hub)
        capture(hub, "02-accounts.png")
        hub.configure(x=60, y=60)
        x.sync()
        focus(hub)
        key(x, "comma", "Control_L")
        settings = window(x, "Hub settings", hub_process.pid)
        capture(settings, "05-settings.png")
        close_window(x, settings)
        until(lambda: invisible(settings), "settings dismissed")
        focus(hub)
        click(hub, 430, 183)
        def open_menu():
            for win in x.screen().root.query_tree().children:
                if win.get_attributes().override_redirect and not invisible(win):
                    return win
        popup = until(open_menu, "account actions popup")
        if screenshots:
            time.sleep(.15)
            geo = hub.get_geometry()
            menu = popup.get_geometry()
            width = max(geo.width, menu.x + menu.width - 60)
            height = max(geo.height, menu.y + menu.height - 60)
            subprocess.run(["import", "-window", "root", "-crop", f"{width}x{height}+60+60",
                            str(screenshots / "06-actions.png")], check=True)
        key(x, "Escape")
        until(lambda: invisible(popup), "account actions dismissed")
        # Standard 480px hub at 100% scale. Click the first two account rows.
        click(hub, 370, 183)
        window(x, "Acheron Custom - atelier")
        first_pid = int((profile("atelier") / "profile.lock").read_text().splitlines()[0])
        click(hub, 370, 254)
        second = window(x, "Acheron Custom - luna")
        until(lambda: control(profile("atelier"), b"1 status\n") == b"1 window\n", "session endpoint")
        assert control(profile("luna"), b"2 close\n") == b"1 error\n"
        assert control(profile("luna"), b"1 unknown\n") == b"1 error\n"
        second_pid = int((profile("luna") / "profile.lock").read_text().splitlines()[0])
        usage("Hub and two offline sessions", [hub_process.pid, first_pid, second_pid])
        focus(hub)
        capture(hub, "03-sessions.png")
        click(hub, 370, 183)
        assert int((profile("atelier") / "profile.lock").read_text().splitlines()[0]) == first_pid
        assert control(profile("atelier"), b"1 close\n") == b"1 ok\n"
        until(lambda: not (profile("atelier") / "profile.lock").exists(), "clean close")
        assert not (profile("atelier") / "restore-session").exists()
        close_window(x, hub)
        assert hub_process.wait(timeout=5) == 0
        assert control(profile("luna"), b"1 status\n") == b"1 window\n"
        hub_process = start()
        hub = window(x, "Acheron Hub", hub_process.pid)
        time.sleep(.3)
        click(hub, 370, 183)
        window(x, "Acheron Custom - atelier")
        assert control(profile("atelier"), b"1 suspend\n") == b"1 ok\n"
        until(lambda: not (profile("atelier") / "profile.lock").exists(), "suspended close")
        close_window(x, second)
        until(lambda: not (profile("luna") / "profile.lock").exists(), "manual close")
        close_window(x, hub)
        assert hub_process.wait(timeout=5) == 0

        prefs = hub_dir / "registry.ini"
        with prefs.open("a") as f:
            f.write("\n[General]\nstartup=restore\n")
        direct = start("--profile", "luna")
        direct_window = window(x, "Acheron Custom - luna", direct.pid)
        assert not (hub_dir / "hub.lock").exists()
        assert not (profile("atelier") / "profile.lock").exists()
        close_window(x, direct_window)
        assert direct.wait(timeout=5) == 0
        hub_process = start()
        hub = window(x, "Acheron Hub", hub_process.pid)
        window(x, "Acheron Custom - atelier")
        assert not (profile("luna") / "profile.lock").exists()
        pid = int((profile("atelier") / "profile.lock").read_text().splitlines()[0])
        os.kill(pid, 9)  # Only the synthetic session of this temporary root.
        until(lambda: not (profile("atelier") / "profile.lock").exists(), "stale lock recovery")
        for n in range(16):
            seed(f"demo-{n}", f"demo_{n}")
        time.sleep(.25)
        focus(hub)
        capture(hub, "07-many.png")
        close_window(x, hub)
        assert hub_process.wait(timeout=5) == 0
        # Isolate the three-account cases from the scrolling stress fixture.
        for n in range(16):
            profile(f"demo-{n}").rename(root / f"parked-demo-{n}")

        # Bulk opens eligible closed profiles, while selected startup is independent
        # of a profile's exclusion from Launch all. Neither opens archived profiles.
        common = "[exclude]\nluna=true\n[archived]\nvoyage=true\n"
        prefs.write_text("[General]\nschema=1\nstartup=hub\n" + common)
        bulk = start()
        hub = window(x, "Acheron Hub", bulk.pid)
        focus(hub)
        # Launch all is the second footer button at this width.
        click(hub, 218, hub.get_geometry().height - 33)
        window(x, "Acheron Custom - atelier")
        time.sleep(1.4)
        assert not any((profile(id_) / "profile.lock").exists() for id_ in ("luna", "voyage"))
        assert control(profile("atelier"), b"1 close\n") == b"1 ok\n"
        until(lambda: not (profile("atelier") / "profile.lock").exists(), "bulk session closed")
        close_window(x, hub)
        assert bulk.wait(timeout=5) == 0

        prefs.write_text("[General]\nschema=1\nstartup=selected\nclose=all\n"
                         "[selected]\natelier=true\nluna=true\nvoyage=true\n" + common)
        selected = start()
        hub = window(x, "Acheron Hub", selected.pid)
        window(x, "Acheron Custom - atelier")
        window(x, "Acheron Custom - luna")
        assert not (profile("voyage") / "profile.lock").exists()
        close_window(x, hub)
        def confirmation():
            for win in x.screen().root.query_tree().children:
                if win.get_attributes().map_state == X.IsViewable and win.get_wm_transient_for() == hub:
                    return win
        question = until(confirmation, "confirm global close")
        focus(question)
        key(x, "y")
        assert selected.wait(timeout=22) == 0
        for id_ in ("atelier", "luna"):
            assert not (profile(id_) / "profile.lock").exists()
            assert (profile(id_) / "restore-session").exists()

        prefs.write_text("[General]\nschema=1\nstartup=default\ndefault=luna\n" + common)
        default = start()
        hub = window(x, "Acheron Hub", default.pid)
        window(x, "Acheron Custom - luna")
        assert not (profile("atelier") / "profile.lock").exists()
        assert control(profile("luna"), b"1 close\n") == b"1 ok\n"
        until(lambda: not (profile("luna") / "profile.lock").exists(), "default session closed")
        close_window(x, hub)
        assert default.wait(timeout=5) == 0

        # Run a disposable copy, then replace its launch target with a child that
        # exits without acquiring a profile lock. Only files in this test root move.
        prefs.write_text("[General]\nschema=1\nstartup=hub\n")
        binary = root / "failure-test"
        shutil.copy2(exe, binary)
        probe = start(binary=binary)
        hub = window(x, "Acheron Hub", probe.pid)
        real = binary.with_suffix(".saved")
        binary.rename(real)
        binary.write_text("#!/bin/sh\nexit 1\n")
        binary.chmod(0o700)
        def button_pixels():
            return hub.get_image(305, 167, 92, 30, X.ZPixmap, 0xffffffff).data
        idle = button_pixels()
        click(hub, 365, 182)
        time.sleep(.25)
        launching = button_pixels()
        assert idle != launching
        capture(hub, "08-launching.png")
        time.sleep(16.5)
        failed = button_pixels()
        assert failed != idle and failed != launching
        assert not (profile("atelier") / "profile.lock").exists()
        capture(hub, "09-retry.png")
        real.replace(binary)
        click(hub, 365, 182)
        window(x, "Acheron Custom - atelier")
        assert control(profile("atelier"), b"1 close\n") == b"1 ok\n"
        until(lambda: not (profile("atelier") / "profile.lock").exists(), "retry and clean close")
        close_window(x, hub)
        assert probe.wait(timeout=5) == 0
        print("PASS: singleton, two sessions, bulk, selected, default, global close, restore, crash recovery and retry")


if __name__ == "__main__":
    if not __debug__:
        sys.exit("Enable Python assertions")
    exercise(str(Path(sys.argv[1]).resolve(strict=True)), Path(sys.argv[2]) if len(sys.argv) > 2 else None)
