"""Offline Linux integration check. Run in a private xvfb-run display (see docs/TESTING.md)."""

import os
import sqlite3
import subprocess
import sys
import tempfile
import time
from contextlib import ExitStack, closing
from pathlib import Path

from Xlib import X, XK, display, error, protocol
from Xlib.ext import xtest


def until(check, description):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        value = check()
        if value:
            return value
        time.sleep(0.05)
    raise RuntimeError(f"Timed out waiting for {description}")


def main(executable):
    with tempfile.TemporaryDirectory(prefix="acheron-profiles-") as temp, ExitStack() as cleanup:
        root = Path(temp)
        env = dict(os.environ, HOME=temp, TMPDIR=temp, XDG_DATA_HOME=f"{temp}/data",
                   XDG_CONFIG_HOME=f"{temp}/config", XDG_CACHE_HOME=f"{temp}/cache",
                   QT_QPA_PLATFORM="xcb", LC_ALL="C")
        x = display.Display()
        cleanup.callback(x.close)

        def run(*args):
            child = subprocess.Popen([executable, *args], env=env)

            def stop():
                if child.poll() is None:
                    child.kill()
                child.wait()

            cleanup.callback(stop)
            return child

        def window(title, pid=None):
            def find():
                for win in x.screen().root.query_tree().children:
                    try:
                        label = win.get_full_property(x.intern_atom("_NET_WM_NAME"), X.AnyPropertyType)
                        if not label or win.get_attributes().map_state != X.IsViewable:
                            continue
                        name = label.value.decode("utf-8")
                        if name != title and not name.startswith(title + " — "):
                            continue
                        owner = win.get_full_property(x.intern_atom("_NET_WM_PID"), X.AnyPropertyType)
                        if owner and (pid is None or owner.value[0] == pid):
                            return win
                    except error.BadWindow:
                        continue
            return until(find, title)

        def close_window(win):
            win.send_event(protocol.event.ClientMessage(
                window=win, client_type=x.intern_atom("WM_PROTOCOLS"),
                data=(32, [x.intern_atom("WM_DELETE_WINDOW"), X.CurrentTime, 0, 0, 0])))
            x.flush()

        def key(name, modifier=None):
            def send(symbol, event):
                xtest.fake_input(x, event, x.keysym_to_keycode(XK.string_to_keysym(symbol)))
            if modifier:
                send(modifier, X.KeyPress)
            send(name, X.KeyPress)
            send(name, X.KeyRelease)
            if modifier:
                send(modifier, X.KeyRelease)
            x.sync()

        def profile(name):
            return root / "data" / "sucez" / f"Acheron-Custom-{name}"

        def rejected(title, *args):
            child = run(*args)
            close_window(window(title, child.pid))
            assert child.wait(timeout=5) == 1

        first = run()
        principal = window("Acheron Custom - principal", first.pid)
        second = run("--profile", "DeUx")
        other = window("Acheron Custom - deux", second.pid)
        for name, child in [("principal", first), ("deux", second)]:
            lock = profile(name) / "profile.lock"
            assert int(lock.read_text().splitlines()[0]) == child.pid
            assert (profile(name) / "acheron.log").is_file()
        rejected("Profile unavailable", "--profile", "DEUX")
        for args in [("--profile", "../escape"), ("--profile", "a\n"), ("--profile", "x" * 33),
                     ("--profile",), ("--profile=a", "--profile", "b")]:
            rejected("Invalid profile", *args)
        assert {p.name for p in profile("deux").parent.iterdir()} == {
            "Acheron-Custom-deux", "Acheron-Custom-principal"}

        principal.set_input_focus(X.RevertToParent, X.CurrentTime)
        key("v", "Alt_L")
        key("Home")
        key("Down")
        key("Return")
        window("Open another profile", first.pid)
        for letter in "menu":
            key(letter)
        key("Return")
        launched = window("Acheron Custom - menu")
        close_window(launched)
        until(lambda: not (profile("menu") / "profile.lock").exists(), "detached process shutdown")

        for win, width in [(principal, 920), (other, 1120)]:
            win.configure(width=width, height=680)
            x.sync()
            until(lambda: win.get_geometry().width == width, f"window width {width}")
            close_window(win)
        assert first.wait(timeout=5) == second.wait(timeout=5) == 0
        for name, rows in [("principal", [(1, "alpha"), (2, "beta")]), ("deux", [(1, "gamma")])]:
            folder = profile(name)
            assert not (folder / "profile.lock").exists()
            with closing(sqlite3.connect(folder / "acheron.sqlite")) as db:
                assert db.execute("select count(*) from accounts").fetchone() == (0,)
                db.executemany("insert into accounts(id,username,auto_connect) values(?,?,0)", rows)
                db.commit()
            child = run("--profile=" + name)
            win = window("Acheron Custom - " + name, child.pid)
            expected = 920 if name == "principal" else 1120
            until(lambda: win.get_geometry().width == expected, f"restored width {expected}")
            close_window(win)
            assert child.wait(timeout=5) == 0
            with closing(sqlite3.connect(folder / "acheron.sqlite")) as db:
                assert db.execute("select id,username from accounts order by id").fetchall() == rows
                assert db.execute("pragma quick_check").fetchone() == ("ok",)
        print("PASS: profiles, locks, invalid names, detached launch, restart geometry and SQLite accounts")


if __name__ == "__main__":
    if not __debug__:
        sys.exit("Run this test with Python assertions enabled.")
    main(str(Path(sys.argv[1]).resolve(strict=True)))
