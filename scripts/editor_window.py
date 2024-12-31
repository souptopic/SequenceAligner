import platform, sys
from pathlib import Path
from tkinter import ttk, filedialog, messagebox
import tkinter as tk
import logging
from datetime import datetime

try:
    from .build_system import DISPLAY_BINARY_PROFILE, build_env
    from .config_schema import DISPLAY_NAMES, TOOLTIPS, DEFAULT_VALUES, DEFAULT_CHECKBOXES, validate_config, save_config
except ImportError:
    from build_system import DISPLAY_BINARY_PROFILE, build_env
    from config_schema import DISPLAY_NAMES, TOOLTIPS, DEFAULT_VALUES, DEFAULT_CHECKBOXES, validate_config, save_config

try: import customtkinter as ctk
except ImportError: ctk = None

class ConfigEditor:
    def __init__(self, only_tk=True):
        self.only_tk = only_tk
        self.widgets = {k: (getattr(ttk, v[0]) if only_tk else getattr(ctk, v[1])) 
            for k, v in {'frame': ('Frame','CTkFrame'), 'label': ('Label','CTkLabel'),
                        'entry': ('Entry','CTkEntry'), 'button': ('Button','CTkButton'),
                        'checkbox': ('Checkbutton','CTkCheckBox')}.items()}
        self.widgets['text_output'] = tk.Text if only_tk else lambda *a, **k: ctk.CTkTextbox(*a, wrap="word", **k)
        self.fields = {}
        self.checkboxes = {}
        self.root = (tk.Tk if only_tk else ctk.CTk)()
        self.root.title("Sequence Aligner Configuration")
        self._init_logging()
        self._init_ui()
        build_env.init(self.update_output)
    
    def _create_scrollable(self, parent):
        if not self.only_tk:
            frame = ctk.CTkScrollableFrame(parent, fg_color="transparent")
            frame.pack(fill="both", expand=True, padx=10, pady=10)
            return frame
        
        canvas = tk.Canvas(container := ttk.Frame(parent))
        scrollbar = ttk.Scrollbar(container, orient="vertical", command=canvas.yview)
        frame = ttk.Frame(canvas)
        frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.create_window((0, 0), window=frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.bind_all("<MouseWheel>" if platform.system().lower() == 'windows' else "<Button-4><Button-5>",
            lambda e: canvas.yview_scroll(-1*(e.delta//120), "units"))
        scrollbar.pack(side="right", fill="y")
        canvas.pack(side="left", fill="both", expand=True)
        container.pack(fill="both", expand=True)
        return frame
    
    def _init_ui(self):
        self.content_frame = self._create_scrollable(self.root)
        frame = self.widgets['frame'](self.content_frame)
        frame.pack(fill="x", pady=5)
        
        bf = self.widgets['frame'](frame)
        bf.pack(fill="x", pady=5)
        for display, binary, profile in DISPLAY_BINARY_PROFILE:
            btn = self.widgets['button'](bf, 
                text=display, 
                command=lambda b=binary, p=profile: build_env.build_and_run(self.update_output, b, p))
            btn.pack(side="left", padx=5)
            build_env.register_button_callback(lambda en, b=btn: b.configure(state='normal' if en else 'disabled'))
        
        self.output_text = self.widgets['text_output'](frame, height=10 if self.only_tk else 200)
        self.output_text.pack(fill="both", expand=True, pady=5)
        
        for title, fields in {"Size Limits": [k for k in ("MAX_CSV_LINE", "MAX_SEQ_LEN", "GAP_PENALTY", "BATCH_SIZE")],
                            "CSV Format": [k for k in DEFAULT_VALUES if 'CSV' in k and not k.endswith('_FILE') and k != "MAX_CSV_LINE"],
                            "File Paths": [k for k in DEFAULT_VALUES if k.endswith('_FILE')]}.items():
            self._create_section(frame, title, fields)
        
        options = self.widgets['frame'](self.content_frame)
        options.pack(fill="x", pady=5)
        self.widgets['label'](options, text="Options").pack()
        
        for key, default in DEFAULT_CHECKBOXES.items():
            var = tk.BooleanVar(value=default)
            cb = self.widgets['checkbox'](options, text=DISPLAY_NAMES[key], variable=var)
            cb.pack(fill="x", pady=2)
            self.checkboxes[key], tooltip = var, TOOLTIPS[key]
            cb.bind('<Enter>', lambda e, w=cb, t=tooltip: self._show_tooltip(w, t))
            cb.bind('<Leave>', lambda e, w=cb: self._hide_tooltip(w))
        
        actions = self.widgets['frame'](self.content_frame)
        actions.pack(fill="x", pady=5)
        self.save_button = self.widgets['button'](actions, text="Save", command=self.save)
        self.save_button.pack(side="left", padx=5)
        build_env.register_button_callback(lambda en: self.save_button.configure(state='normal' if en else 'disabled'))
        
        for text, cmd in [("Reset", self.reset_defaults), ("Exit", self.root.quit)]:
            self.widgets['button'](actions, text=text, command=cmd).pack(side="left", padx=5)
        
        self.root.minsize(800, 600)
        self.root.geometry(f"800x600+{(self.root.winfo_screenwidth()-800)//2}+{(self.root.winfo_screenheight()-600)//2}")
        
        ok, error = save_config(self.fields, self.checkboxes)        
        if not ok:
            messagebox.showerror("Error", error)
            return
    
    def _create_section(self, parent, title, fields):
        section = (ttk.LabelFrame(parent, padding=10, text=title) if self.only_tk else 
            (lambda f: (f.pack(fill="x", padx=10, pady=5), 
                     self.widgets['label'](f, text=title).pack(anchor='w', padx=5, pady=(5,0)),
                     self.widgets['frame'](f))[-1])(self.widgets['frame'](parent, fg_color="transparent")))
        section.pack(fill="x", pady=5)
        
        for key in fields:
            row = self.widgets['frame'](section)
            row.pack(fill="x", pady=2)
            label = self.widgets['label'](row, text=f"{DISPLAY_NAMES[key]}:", width=25 if self.only_tk else 200)
            label.pack(side="left")
            entry = self.widgets['entry'](row)
            entry.insert(0, DEFAULT_VALUES[key])
            entry.pack(side="left", fill="x", expand=True, padx=5)
            
            if key.endswith('_FILE'):
                self.widgets['button'](row, 
                    text="Browse", 
                    width=None if self.only_tk else 100, 
                    command=lambda e=entry, 
                    i=key.startswith('INPUT'): self._browse_file(e, i)
                ).pack(side="left", padx=5)
            
            self.fields[key] = entry
            if key in TOOLTIPS:
                t = TOOLTIPS[key]
                for w in (label, entry):
                    w.bind('<Enter>', lambda e, w=w, t=t: self._show_tooltip(w, t))
                    w.bind('<Leave>', lambda e, w=w: self._hide_tooltip(w))
    
    def _show_tooltip(self, widget, text):
        widget.tooltip = tk.Toplevel()
        if self.only_tk:
            ttk.Label(widget.tooltip, 
                text=text, 
                background="#ffffe0", 
                relief="solid", 
                borderwidth=1
            ).pack(padx=2, pady=2)
            o_x = 0
            o_y = 7 if text.startswith("Path") else 3
        else:
            widget.tooltip.configure(bg='black')
            if build_env.os_name == "windows":
                widget.tooltip.attributes('-transparentcolor', 'black')
            else:
                widget.tooltip.configure(bg='#2b2b2b')
            
            ctk.CTkLabel(widget.tooltip, 
                text=text, 
                fg_color=("gray75", "gray25"), 
                corner_radius=4, 
                justify="left", 
                anchor="w"
            ).pack(padx=4, pady=4)
            o_x = -5
            o_y = 0
        
        widget.tooltip.wm_overrideredirect(True)
        o_x += widget.winfo_rootx()
        o_y += widget.winfo_rooty() + widget.winfo_height()
        widget.tooltip.wm_geometry(f"+{o_x}+{o_y}")
    
    def _hide_tooltip(self, widget):
        if hasattr(widget, 'tooltip'): 
            widget.tooltip.destroy()
            del widget.tooltip
    
    def _browse_file(self, entry, is_input):
        current = Path(entry.get())
        
        if filename := (filedialog.askopenfilename if is_input else filedialog.asksaveasfilename)(initialdir=current.parent, initialfile=current.name, defaultextension=".csv", filetypes=[("CSV files", "*.csv")]):
            entry.delete(0, "end")
            entry.insert(0, str(Path(filename).absolute().as_posix()))
    
    def _init_logging(self):
        log_dir = Path(__file__).parent.parent / "logs"
        log_dir.mkdir(exist_ok=True)
        log_file = log_dir / f"sequence_aligner_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
        logging.basicConfig(
            filename=str(log_file),
            level=logging.INFO,
            format='%(asctime)s - %(message)s',
            datefmt='%Y-%m-%d %H:%M:%S'
        )
    
    def update_output(self, text):
        if hasattr(self, 'output_text'):
            def update():
                should_log = True
                self.output_text.configure(state='normal')
                if text.startswith('\f'):
                    self.output_text.delete("1.0", "end")
                elif text.startswith('\r'):
                    self.output_text.delete("end-2c linestart", "end-1c")
                    should_log = False
                    
                clean_text = text.lstrip('\f\r')
                self.output_text.insert("end", clean_text)
                self.output_text.see("end")
                self.output_text.configure(state='disabled')
                if should_log:
                    logging.info(clean_text.rstrip())
                
            self.root.after(0, update)
    
    def save(self):
        ok, error = validate_config(self.fields, self.checkboxes)
        if not ok:
            messagebox.showerror("Error", error)
            return
        
        ok, error = save_config(self.fields, self.checkboxes)        
        if not ok:
            messagebox.showerror("Error", error)
            return
        
        messagebox.showinfo("Success", "Configuration saved successfully!")
        build_env.reset_build_status()
        build_env.run_make(self.update_output, "all", on_complete = lambda s: self.update_output("\fBuild complete! You can now press the Run button or modify the settings below.\n") if s else None)
    
    def reset_defaults(self):
        for key, value in DEFAULT_VALUES.items():
            if key in self.fields:
                self.fields[key].delete(0, "end")
                self.fields[key].insert(0, value)
        
        for key, value in DEFAULT_CHECKBOXES.items():
            if key in self.checkboxes:
                self.checkboxes[key].set(value)
    
    def run(self):
        self.root.mainloop()

if __name__ == "__main__":
    editor = ConfigEditor()
    editor.run()