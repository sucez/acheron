"""Helpers for the offline tests on a private Xvfb display."""
import time
from Xlib import X, XK, error, protocol
from Xlib.ext import xtest


def until(check, description):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        value = check()
        if value:
            return value
        time.sleep(0.05)
    raise RuntimeError(f"Timed out waiting for {description}")


def window(x, title, pid=None):
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

def close_window(x, win):
    win.send_event(protocol.event.ClientMessage(
        window=win, client_type=x.intern_atom("WM_PROTOCOLS"),
        data=(32, [x.intern_atom("WM_DELETE_WINDOW"), X.CurrentTime, 0, 0, 0])))
    x.flush()

def key(x, name, modifier=None):
    def send(symbol, event):
        xtest.fake_input(x, event, x.keysym_to_keycode(XK.string_to_keysym(symbol)))
    if modifier:
        send(modifier, X.KeyPress)
    send(name, X.KeyPress)
    send(name, X.KeyRelease)
    if modifier:
        send(modifier, X.KeyRelease)
    x.sync()
