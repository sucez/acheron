"""Offline X11 presentation checks. Optionally collect screenshots in argv[2]."""
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from Xlib import X, display
from x11_helpers import close_window, key, window


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
            with subprocess.Popen([executable], env=env) as child:
                hub = window(x, "Acheron Hub", child.pid)
                capture(hub, density)
                sizes.append(hub.get_geometry().height)
                if density == "comfortable":
                    def second_card_pixel():
                        return hub.get_image(30, 255, 1, 1, X.ZPixmap, 0xffffffff).data
                    before = second_card_pixel()
                    hub.set_input_focus(X.RevertToParent, X.CurrentTime)
                    key(x, "f", "Control_L")
                    for letter in "moonlight":
                        key(x, letter)
                    capture(hub, "search")
                    assert second_card_pixel() != before, "second card still visible after filtering"
                close_window(x, hub)
                assert child.wait(timeout=5) == 0
        x.close()
        assert sizes[0] < sizes[1] < sizes[2], sizes
        return sizes


if __name__ == "__main__":
    print(exercise(sys.argv[1], Path(sys.argv[2]) if len(sys.argv) > 2 else None))
