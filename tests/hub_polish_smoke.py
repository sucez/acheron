"""Offline X11 presentation checks. Optionally collect screenshots in argv[2]."""
import json
import os
import subprocess
import sys
import tempfile
import time
from contextlib import contextmanager
from pathlib import Path
from Xlib import X, display
from Xlib.ext import xtest
from x11_helpers import close_window, key, until, window


@contextmanager
def running(args, env):
    child = subprocess.Popen(args, env=env)
    try:
        yield child
    finally:
        if child.poll() is None:
            child.kill()
        child.wait()


def exercise(executable, output):
    if output:
        output.mkdir(exist_ok=True, parents=True)
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        org = root / "data/sucez"
        hub_dir = org / "Acheron-Hub"
        hub_dir.mkdir(parents=True)
        env = dict(os.environ, HOME=temp, TMPDIR=temp, XDG_DATA_HOME=str(root / "data"),
                   XDG_CONFIG_HOME=str(root / "config"), XDG_CACHE_HOME=str(root / "cache"),
                   DBUS_SESSION_BUS_ADDRESS="unix:path=" + temp + "/unused", QT_QPA_PLATFORM="xcb")
        x = display.Display()
        names = {"atelier": "@atelier.demo", "luna": "@moonlight", "voyage": "@voyage.long.username"}
        for id_, caption in names.items():
            folder = org / ("Acheron-Custom-" + id_)
            folder.mkdir()
            (folder / "hub-card.json").write_text(json.dumps({"schema": 1, "caption": caption}))

        def capture(win, name):
            time.sleep(.2)
            if output:
                subprocess.run(["import", "-window", str(win.id), str(output / (name + ".png"))], check=True)

        sizes = []
        for density in ("compact", "comfortable", "spacious"):
            (hub_dir / "registry.ini").write_text(f"[General]\nschema=1\ndensity={density}\n"
                                                   "[favorite]\nluna=true\n[name]\nluna=Moon Studio\n")
            with running([executable], env=env) as child:
                hub = window(x, "Acheron Hub", child.pid)
                capture(hub, density)
                sizes.append(hub.get_geometry().height)
                if density == "comfortable":
                    def pointer(y):
                        point = x.screen().root.translate_coords(hub, 30, y)
                        xtest.fake_input(x, X.MotionNotify, x=point.x, y=point.y)
                        x.sync()
                        time.sleep(.08)
                    pointer(323)
                    xtest.fake_input(x, X.ButtonPress, 1)
                    pointer(302)
                    pointer(272)
                    pointer(252)
                    xtest.fake_input(x, X.ButtonRelease, 1)
                    x.sync()
                    until(lambda: "order=luna, voyage, atelier" in (hub_dir / "registry.ini").read_text(),
                          "dragged voyage above atelier")
                    def second_card_pixel():
                        return hub.get_image(30, 280, 1, 1, X.ZPixmap, 0xffffffff).data
                    hub.set_input_focus(X.RevertToParent, X.CurrentTime)
                    key(x, "f", "Control_L")
                    time.sleep(.25)  # Qt animates a button activated via its shortcut.
                    before = second_card_pixel()
                    for letter in "moonlight":
                        key(x, letter)
                    capture(hub, "search")
                    assert second_card_pixel() != before, "second card still visible after filtering"
                close_window(x, hub)
                assert child.wait(timeout=5) == 0
        for scale in (1.25, 1.5, 2):
            scaled = dict(env, QT_SCALE_FACTOR=str(scale))
            with running([executable], env=scaled) as child:
                hub = window(x, "Acheron Hub", child.pid)
                width = hub.get_geometry().width
                assert abs(width - 480 * scale) <= 2, (scale, width)
                if scale == 2:
                    capture(hub, "scale-200")
                close_window(x, hub)
                assert child.wait(timeout=5) == 0
        with running([executable, "--profile", "luna"], env=env) as child:
            chat = window(x, "Acheron Custom - luna", child.pid)
            capture(chat, "session-profile")
            close_window(x, chat)
            assert child.wait(timeout=5) == 0
        x.close()
        assert sizes[0] < sizes[1] < sizes[2], sizes
        return sizes


if __name__ == "__main__":
    print(exercise(sys.argv[1], Path(sys.argv[2]) if len(sys.argv) > 2 else None))
