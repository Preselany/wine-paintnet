#!/usr/bin/python3
"""Ubuntu GTK file chooser for the opt-in, 64-bit Wine IFileDialog bridge.

stdin/stdout are bounded NUL-separated UTF-8 records, never shell commands.
The helper returns paths and a filter index; Paint.NET performs all file writes.
SPDX-License-Identifier: LGPL-2.1-or-later
"""
import os
import re
import sys

LIMIT = 1024 * 1024


def extensions(pattern):
    return [p[1:] for p in pattern.split(";") if re.fullmatch(r"\*\.[\w-]+", p)]


def save_name(name, pattern, default_ext):
    """Preserve any extension allowed by this format; otherwise add its default."""
    exts = extensions(pattern)
    if not exts and default_ext:
        exts = ["." + default_ext.lstrip(".")]
    if exts and not any(name.lower().endswith(ext.lower()) for ext in exts):
        return name + exts[0]
    return name


def main():
    raw = sys.stdin.buffer.read(LIMIT + 1)
    if not raw.endswith(b"\0") or len(raw) > LIMIT:
        raise ValueError("Invalid native dialog request")
    fields = raw[:-1].decode("utf-8").split("\0")
    version, kind, flags, selected, title, accept, folder, name, default_ext, parent, count = fields[:11]
    flags, selected, parent, count = map(int, (flags, selected, parent, count))
    if version != "PDNFD1" or kind not in ("open", "save", "folder") or len(fields) != 11 + count * 2:
        raise ValueError("Unsupported native dialog request")
    if flags < 0 or parent < 0 or count < 0 or not (0 <= selected < max(count, 1)):
        raise ValueError("Invalid native dialog options")
    specs = list(zip(fields[11::2], fields[12::2]))
    if os.path.isabs(name):
        folder, name = os.path.split(name)

    # Wine is deliberately configured for X11. This also keeps isolated-display
    # testing out of the user's Wayland session and desktop portal.
    os.environ["GDK_BACKEND"] = "x11"
    os.environ["GTK_USE_PORTAL"] = "0"
    import gi
    gi.require_version("Gtk", "3.0")
    gi.require_version("GdkX11", "3.0")
    from gi.repository import Gtk, Gdk, GdkX11, GLib

    GLib.set_prgname("paintnet-file-chooser")
    GLib.set_application_name("Paint.NET")
    actions = {"open": Gtk.FileChooserAction.OPEN, "save": Gtk.FileChooserAction.SAVE,
               "folder": Gtk.FileChooserAction.SELECT_FOLDER}
    dialog = Gtk.FileChooserDialog(title=title or {"open": "Open Image", "save": "Save Image",
                                                  "folder": "Select Folder"}[kind],
                                   action=actions[kind], use_header_bar=True)
    dialog.add_buttons("_Cancel", Gtk.ResponseType.CANCEL,
                       accept or ("_Save" if kind == "save" else "_Open"), Gtk.ResponseType.ACCEPT)
    dialog.set_default_response(Gtk.ResponseType.ACCEPT)
    dialog.set_default_size(960, 640)
    dialog.set_local_only(True)
    dialog.set_modal(True)
    dialog.set_select_multiple(kind != "save" and bool(flags & 0x200))
    dialog.set_show_hidden(bool(flags & 0x10000000))
    # We confirm only after adding the extension, so the exact output path is checked.
    dialog.set_do_overwrite_confirmation(False)
    if folder and os.path.isdir(folder):
        dialog.set_current_folder(folder)
    if kind == "save":
        dialog.set_current_name(name or "Untitled")
    elif name and folder and os.path.exists(os.path.join(folder, name)):
        dialog.set_filename(os.path.join(folder, name))

    filters = []
    for label, patterns in specs:
        f = Gtk.FileFilter()
        f.set_name(re.sub(r"\s*\([^)]*\)\s*$", "", label) if len(label) > 90 else label)
        for pattern in patterns.split(";"):
            # Windows wildcard matching is case insensitive; *.* also matches extensionless files.
            pattern = "*" if pattern == "*.*" else pattern
            f.add_pattern("".join("[" + c.lower() + c.upper() + "]" if c.isascii() and c.isalpha()
                                  else c for c in pattern))
        dialog.add_filter(f)
        filters.append(f)
    if filters:
        dialog.set_filter(filters[min(max(selected, 0), len(filters) - 1)])

    previous = selected

    def filter_changed(chooser, _property):
        nonlocal previous
        current = filters.index(chooser.get_filter()) if filters else 0
        if kind == "save":
            filename = chooser.get_current_name()
            old_exts = extensions(specs[previous][1]) if 0 <= previous < len(specs) else []
            for ext in old_exts:
                if filename.lower().endswith(ext.lower()):
                    filename = filename[:-len(ext)]
                    break
            chooser.set_current_name(save_name(filename, specs[current][1], default_ext))
        previous = current

    if kind == "save" and filters:
        dialog.connect("notify::filter", filter_changed)
        filter_changed(dialog, None)

    dialog.realize()
    foreign = None
    if parent:
        foreign = GdkX11.X11Window.foreign_new_for_display(Gdk.Display.get_default(), parent)
        if foreign:
            dialog.get_window().set_transient_for(foreign)
    dialog.present()
    while True:
        if dialog.run() != Gtk.ResponseType.ACCEPT:
            result = ["CANCEL"]
            break
        paths = dialog.get_filenames()
        index = filters.index(dialog.get_filter()) if filters else 0
        if kind == "save" and paths:
            paths[0] = save_name(paths[0], specs[index][1] if specs else "", default_ext)
            if flags & 2 and os.path.lexists(paths[0]):
                warning = Gtk.MessageDialog(transient_for=dialog, modal=True,
                                            message_type=Gtk.MessageType.QUESTION,
                                            buttons=Gtk.ButtonsType.NONE,
                                            text="Replace the existing file?")
                warning.format_secondary_text(os.path.basename(paths[0]) + " already exists. Replacing it will overwrite its contents.")
                warning.add_buttons("_Cancel", Gtk.ResponseType.CANCEL, "_Replace", Gtk.ResponseType.ACCEPT)
                warning.set_default_response(Gtk.ResponseType.CANCEL)
                response = warning.run()
                warning.destroy()
                if response != Gtk.ResponseType.ACCEPT:
                    continue
        if paths:
            result = ["OK", str(index), *paths]
            break
    dialog.destroy()
    output = ("\0".join(result) + "\0").encode("utf-8")
    if len(output) >= LIMIT:
        raise ValueError("Too many selected files")
    sys.stdout.buffer.write(output)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"Paint.NET native file chooser: {error}", file=sys.stderr)
        sys.exit(1)
