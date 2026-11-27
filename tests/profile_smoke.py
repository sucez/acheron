"""Offline Linux integration check. Run in a private xvfb-run display (see docs/TESTING.md)."""

import os
import sqlite3
import subprocess
import sys
import tempfile
import time
from contextlib import ExitStack, closing
from pathlib import Path

from Xlib import X, display
from functools import partial
from x11_helpers import until, window as find_window, close_window as close_xwindow, key as send_key


def main(executable):
    with tempfile.TemporaryDirectory(prefix="acheron-profiles-") as temp, ExitStack() as cleanup:
        root = Path(temp)
        env = dict(os.environ, HOME=temp, TMPDIR=temp, XDG_DATA_HOME=f"{temp}/data",
                   XDG_CONFIG_HOME=f"{temp}/config", XDG_CACHE_HOME=f"{temp}/cache",
                   QT_QPA_PLATFORM="xcb", LC_ALL="C")
        x = display.Display()
        cleanup.callback(x.close)
        window = partial(find_window, x)
        close_window = partial(close_xwindow, x)
        key = partial(send_key, x)

        def run(*args):
            child = subprocess.Popen([executable, *args], env=env)

            def stop():
                if child.poll() is None:
                    child.kill()
                child.wait()

            cleanup.callback(stop)
            return child

        def profile(name):
            return root / "data" / "sucez" / f"Acheron-Custom-{name}"

        def rejected(title, *args):
            child = run(*args)
            close_window(window(title, child.pid))
            assert child.wait(timeout=5) == 1

        first = run("--profile", "principal")
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
        hub = window("Acheron Hub")
        hub.set_input_focus(X.RevertToParent, X.CurrentTime)
        key("n", "Control_L")
        prompt = window("Add an account")
        prompt.set_input_focus(X.RevertToParent, X.CurrentTime)
        key("a", "Control_L")
        for letter in "menu":
            key(letter)
        key("Return")
        launched = window("Acheron Custom - menu")
        close_window(window("Manage Accounts"))
        close_window(launched)
        until(lambda: not (profile("menu") / "profile.lock").exists(), "detached process shutdown")
        close_window(hub)

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
