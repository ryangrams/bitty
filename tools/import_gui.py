#!/usr/bin/env python3
"""Open a native local-file importer with an explicit rights confirmation."""
from __future__ import annotations

import argparse
import contextlib
import io
from pathlib import Path
import queue
import threading

ROOT = Path(__file__).resolve().parents[1]
RIGHTS_CONFIRMATION = ('I confirm I have the rights or permission needed to use and convert '
                       'the assets I supply for this build.')


def build_arguments(mode: str, apk: str, atlas: str, manifest: str,
                    audio: str, output: str, acknowledged: bool) -> list[str]:
    """Construct a CLI import only after the user checks the confirmation."""
    if not acknowledged:
        raise ValueError('Check the rights confirmation before importing.')
    if not output.strip():
        raise ValueError('Choose a private output folder.')
    arguments = ['--acknowledge-rights', '--output-dir', output]
    if mode == 'apk':
        if not apk.strip():
            raise ValueError('Choose a local APK file.')
        arguments += ['--apk', apk]
    elif mode == 'atlas':
        if not atlas.strip() or not manifest.strip():
            raise ValueError('Choose both a local atlas PNG and its text manifest.')
        arguments += ['--atlas', atlas, '--manifest', manifest]
    else:
        raise ValueError('Choose an import format.')
    if audio.strip():
        arguments += ['--audio-dir', audio]
    return arguments


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / '.local' / 'assets', help='Private output folder')
    args = parser.parse_args(argv)
    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox, ttk
        try:
            from .generate_assets import main as import_assets
        except ImportError:
            from generate_assets import main as import_assets
    except ImportError as exc:
        parser.exit(2, f'Native importer requires Python with Tkinter and the project requirements installed: {exc}\n')
    try:
        root = tk.Tk()
    except tk.TclError as exc:
        parser.exit(2, f'Cannot open the native importer on this display: {exc}\n')
    root.title('Import local game assets')
    root.minsize(680, 530)
    frame = ttk.Frame(root, padding=20)
    frame.pack(fill='both', expand=True)
    frame.columnconfigure(1, weight=1)
    mode, acknowledged = tk.StringVar(value='apk'), tk.BooleanVar(value=False)
    paths = {key: tk.StringVar() for key in ('apk', 'atlas', 'manifest', 'audio')}
    paths['output'] = tk.StringVar(value=str(args.output_dir))
    busy, result_queue = False, queue.Queue()
    ttk.Label(frame, text='Import files you supply locally', font=('', 16, 'bold')).grid(row=0, column=0, columnspan=3, sticky='w')
    ttk.Label(frame, text='The importer reads a local APK or a PNG atlas with its text manifest.', wraplength=620).grid(row=1, column=0, columnspan=3, sticky='w', pady=(8, 14))
    mode_frame = ttk.Frame(frame)
    mode_frame.grid(row=2, column=0, columnspan=3, sticky='w', pady=(0, 10))
    ttk.Radiobutton(mode_frame, text='Local APK', variable=mode, value='apk').pack(side='left')
    ttk.Radiobutton(mode_frame, text='Atlas and manifest', variable=mode, value='atlas').pack(side='left', padx=(20, 0))
    rows = [
        ('apk', 'APK', [('APK files', '*.apk'), ('All files', '*')]),
        ('atlas', 'Atlas PNG', [('PNG files', '*.png'), ('All files', '*')]),
        ('manifest', 'Manifest', [('Text files', '*.txt'), ('All files', '*')]),
        ('audio', 'Audio folder (optional)', None),
        ('output', 'Private output folder', None),
    ]
    controls = {}
    for row, (key, label, filetypes) in enumerate(rows, 3):
        ttk.Label(frame, text=label).grid(row=row, column=0, sticky='w', padx=(0, 10), pady=5)
        entry = ttk.Entry(frame, textvariable=paths[key])
        entry.grid(row=row, column=1, sticky='ew', pady=5)
        def browse(key=key, filetypes=filetypes):
            chosen = (filedialog.askdirectory(parent=root, title='Choose folder') if filetypes is None
                      else filedialog.askopenfilename(parent=root, title='Choose local file', filetypes=filetypes))
            if chosen:
                paths[key].set(chosen)
        button = ttk.Button(frame, text='Choose', command=browse)
        button.grid(row=row, column=2, padx=(8, 0), pady=5)
        controls[key] = (entry, button)
    confirmation_frame = ttk.Frame(frame)
    confirmation_frame.grid(row=8, column=0, columnspan=3, sticky='w', pady=(18, 4))
    confirmation = ttk.Checkbutton(confirmation_frame, variable=acknowledged)
    confirmation.pack(side='left', anchor='n')
    ttk.Label(confirmation_frame, text=RIGHTS_CONFIRMATION, wraplength=590).pack(side='left')
    ttk.Label(frame, text='Checking this box records your statement and grants no license.', wraplength=620).grid(row=9, column=0, columnspan=3, sticky='w', pady=(0, 12))
    status = tk.StringVar(value='Choose your local files, then check the confirmation.')
    ttk.Label(frame, textvariable=status, wraplength=620).grid(row=10, column=0, columnspan=3, sticky='w', pady=(0, 12))

    def refresh(*_):
        for key, widgets in controls.items():
            enabled = not busy and (key not in ('apk', 'atlas', 'manifest') or
                                    (key == 'apk') == (mode.get() == 'apk'))
            for widget in widgets:
                widget.configure(state='normal' if enabled else 'disabled')
        import_button.configure(state='normal' if acknowledged.get() and not busy else 'disabled')
        confirmation.configure(state='disabled' if busy else 'normal')

    def worker(arguments):
        output = io.StringIO()
        try:
            with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
                result = import_assets(arguments)
            result_queue.put((result == 0, output.getvalue().strip()))
        except SystemExit as exc:
            result_queue.put((False, output.getvalue().strip() or f'Import exited with code {exc.code}'))
        except Exception as exc:
            result_queue.put((False, str(exc)))

    def poll_result():
        nonlocal busy
        try:
            success, message = result_queue.get_nowait()
        except queue.Empty:
            root.after(100, poll_result)
            return
        busy = False
        refresh()
        status.set(message)
        if success:
            messagebox.showinfo('Import complete', message, parent=root)
        else:
            messagebox.showerror('Import failed', message, parent=root)

    def start_import():
        nonlocal busy
        try:
            arguments = build_arguments(mode.get(), *(paths[key].get() for key in ('apk', 'atlas', 'manifest', 'audio', 'output')), acknowledged.get())
        except ValueError as exc:
            messagebox.showerror('Choose local files', str(exc), parent=root)
            return
        busy = True
        status.set('Converting local assets. The output stays in your private folder.')
        refresh()
        threading.Thread(target=worker, args=(arguments,), daemon=True).start()
        root.after(100, poll_result)

    import_button = ttk.Button(frame, text='Import', command=start_import, state='disabled')
    import_button.grid(row=11, column=0, columnspan=3, sticky='e')
    mode.trace_add('write', refresh)
    acknowledged.trace_add('write', refresh)
    refresh()
    root.mainloop()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
