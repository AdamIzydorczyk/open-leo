#!/usr/bin/env python3
"""golden_live_view.py - watch and steer the ORIGINAL when QEMU's own
window cannot.

Why this exists: on this desktop QEMU's GTK display shows DOS text but goes
BLACK the moment the game switches to its VESA mode (measured 2026-11-24 -
the frame the monitor itself captures has content: 640x480, mean pixel 42.3,
while the window shows nothing). The emulator is fine; its front-end is not.
So take the frame from where the emulator reads it and show it in a window
of our own.

Why it talks through files: the QMP socket serves one client at a time and
golden_level_trace.py owns it. So the viewer only reads the frame the
recorder writes (<workdir>/live_frame.ppm) and appends keystrokes to a
queue file (<workdir>/keys.in) that the recorder forwards with `sendkey`:

  terminal 1:  python3 tools/golden_level_trace.py --display none \
                 --live-frames --keys-from /tmp/opencode/golden/keys.in ...
  terminal 2:  python3 tools/golden_live_view.py --keys

The recorder must not run scripted input while a person drives the game;
--keys-from makes it forward only what this viewer appends. q in the viewer
closes the viewer, not Esc: the game consumes Esc.
"""

import argparse
import os
import sys
import time

try:
    import gi
    gi.require_version('Gtk', '3.0')
    from gi.repository import Gtk, Gdk, GLib, GdkPixbuf
except ImportError:
    raise SystemExit('needs python3-gi (GTK 3). Without a GUI you can still '
                     'watch the file: eog <workdir>/live_frame.ppm')

KEYS = {'Up': 'up', 'Down': 'down', 'Left': 'left', 'Right': 'right',
        'Return': 'ret', 'KP_Enter': 'ret', 'BackSpace': 'esc',
        'space': 'spc', 'x': 'x', 'z': 'z', 'a': 'a', 's': 's',
        'Control_L': 'ctrl', 'Control_R': 'ctrl',
        'F1': 'f1', 'F2': 'f2', 'F3': 'f3', 'F4': 'f4',
        'q': 'q', 'e': 'e', 'h': 'h', 'j': 'j', 'k': 'k', 'l': 'l'}
INPUT_DEBOUNCE_S = 1.0   # at most one game key event per second
KEY_HOLD_MS = {'left': 800, 'right': 800, 'up': 300, 'down': 300,
               'ret': 150, 'esc': 150, 'spc': 300, 'ctrl': 600,
               'f1': 300, 'f2': 300, 'f3': 300, 'f4': 300}


class View:
    def __init__(self, frame, keyfile, interval, keys_enabled=False):
        self.frame, self.keyfile = frame, keyfile
        self.keys = open(keyfile, 'a') if keys_enabled else None
        self.held = set()
        self.last_key_at = {}
        self.win = Gtk.Window(title='LEO Leon - via the monitor, not QEMU')
        self.area = Gtk.Image()
        self.box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        note = ('arrows held 800 ms; Ctrl/F1-F4 attack; each key limited '
                'to 1/s; q quits'
                if keys_enabled else 'view only; close with the window button')
        self.note = Gtk.Label(label=note)
        self.box.pack_start(self.area, True, True, 0)
        self.box.pack_start(self.note, False, False, 0)
        self.win.add(self.box)
        self.win.connect('destroy', self.on_close)
        self.win.set_default_size(640, 480)
        self.win.show_all()
        self.area.set_size_request(640, 480)
        if keys_enabled:
            self.area.set_can_focus(True)
            self.area.grab_focus()
            self.win.connect('key-press-event', self.on_key)
            self.win.connect('key-release-event', self.on_key_release)
        self.mtime = 0
        GLib.timeout_add(int(interval * 1000), self.tick)

    def on_key(self, _w, ev):
        name = Gdk.keyval_name(ev.keyval)
        if name in ('q', 'Q'):
            # NOT Escape: the game takes Esc, and pressing Esc to skip an
            # intro closed the viewer instead (2026-11-24).
            self.on_close()
            return True
        key = KEYS.get(name)
        if key and self.keys is not None:
            now = time.monotonic()
            if now - self.last_key_at.get(name, 0.0) < INPUT_DEBOUNCE_S:
                return True
            # GTK emits repeated key-press events while a key is held. Send
            # only a slow, bounded stream to QEMU; require release before the
            # same key can be forwarded again as well.
            if name in self.held:
                return True
            self.held.add(name)
            self.last_key_at[name] = now
            self.keys.write('%s:%d\n' % (key, KEY_HOLD_MS.get(key, 300)))
            self.keys.flush()
            os.fsync(self.keys.fileno())
        return True

    def on_key_release(self, _w, ev):
        self.held.discard(Gdk.keyval_name(ev.keyval))
        return False

    def on_close(self, *_a):
        if self.keys is not None:
            self.keys.close()
        Gtk.main_quit()

    def tick(self):
        try:
            st = os.stat(self.frame)
            if st.st_mtime != self.mtime:
                self.mtime = st.st_mtime
                self.area.set_from_pixbuf(
                    GdkPixbuf.Pixbuf.new_from_file(self.frame))
        except Exception:
            pass
        return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--workdir', default='/tmp/opencode/golden')
    ap.add_argument('--frame', help='default: <workdir>/live_frame.ppm')
    ap.add_argument('--keys-from', help='default: <workdir>/keys.in')
    ap.add_argument('--interval', type=float, default=0.25)
    ap.add_argument('--keys', action='store_true',
                    help='send what you type into the game (arrows, Enter, '
                         'Backspace=Esc, space, and a s d w x z q e h j k l)')
    a = ap.parse_args()
    frame = a.frame or os.path.join(a.workdir, 'live_frame.ppm')
    keyfile = a.keys_from or os.path.join(a.workdir, 'keys.in')
    if not os.path.exists(frame):
        print('waiting for %s - start the recorder with --live-frames'
              % frame, flush=True)
    View(frame, keyfile, a.interval, keys_enabled=a.keys)
    Gtk.main()


if __name__ == '__main__':
    main()
