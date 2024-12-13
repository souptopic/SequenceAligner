import sys
import platform
def check_tkinter():
    try:
        import tkinter as tk
    except ImportError:
        os_type = platform.system().lower()
        if os_type == "linux":
            print("Error: tkinter is not installed. Install it with one of these commands:")
            print("Ubuntu/Debian: sudo apt-get install python3-tk")
            print("Fedora: sudo dnf install python3-tkinter")
            print("Arch: sudo pacman -S tk")
            print("WSL2: not supported unless you have vcxsrv, xming or similar")
        elif os_type == "darwin":
            print("Error: tkinter is not installed. Install it with:")
            print("brew install python-tk@3.x")
        sys.exit(1)
check_tkinter()
import tkinter as tk
from tkinter import ttk, messagebox
from pathlib import Path
import os
import re

script_dir = Path(__file__).parent.resolve()
project_root = script_dir.parent
user_file = project_root / 'include' / 'user.h'

if not all(path.exists() for path in [project_root / 'include', user_file]):
    raise FileNotFoundError("Invalid project structure. Please run from scripts directory.")

def normalize_path(path_str):
    return str(Path(path_str)).replace('\\', '/')

class ConfigEditor:
    TOOLTIPS = {
        "MAX_CSV_LINE": "Maximum length of any line in CSV files (must be ≥32)",
        "MAX_SEQ_LEN": "Maximum length of any sequence (must be ≥1)",
        "READ_CSV_HEADER": """Input CSV Format Rules:
- One sequence per line
- Fixed number of columns 
- Additional columns preserved in output
- Names are arbitrary, only positions matter

Example: "animal,sequence,data1,data2" """,
        "READ_CSV_SEQ_POS": "Position of sequence column in input (0-based)",
        "READ_CSV_COLS": "Total number of columns in input CSV",
        "WRITE_CSV_HEADER": """Output CSV Format Rules:
- Each row is paired with the next row
- Output columns come in pairs (one from first row, one from second row) 
- Column pairs must be adjacent in header
- Required special columns: score, alignment
- All column names are arbitrary, only positions matter
- Number of columns = 2 * number of read columns + 2

Valid examples:
"seq_a,seq_b,score,align,name_x,name_y,type1,type2"
"score,first,second,alignment,cat1,cat2,id_x,id_y"
"type_orig,type_comp,name1,name2,seq_first,seq_second,aligned_sequence,points" """,
        "WRITE_CSV_SEQ1_POS": "Position of first sequence in output",
        "WRITE_CSV_SCORE_POS": "Position of alignment score in output",
        "WRITE_CSV_ALIGN_POS": "Position of alignment result in output",
        "WRITE_CSV_ALIGN_FMT": "Printf format for alignment (must contain two %s)",
        "INPUT_FILE": "Path to input file for single-threaded mode",
        "INPUT_MT_FILE": "Path to input file for multi-threaded mode", 
        "OUTPUT_FILE": "Path to output file for single-threaded mode",
        "OUTPUT_MT_FILE": "Path to output file for multi-threaded mode",
        "MODE_WRITE": "Uncheck to disable output"
    }
    
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Sequence Aligner Configuration")
        self.root.geometry("464x726")
        
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        canvas = tk.Canvas(main_frame)
        scrollbar = ttk.Scrollbar(main_frame, orient="vertical", command=canvas.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        canvas.configure(yscrollcommand=scrollbar.set)
        self.content_frame = ttk.Frame(canvas)
        canvas.create_window((0, 0), window=self.content_frame, anchor="nw")
        
        # Extra spaces added to align input fields
        self.create_section("Size Limits", [
            ("MAX_CSV_LINE", "Maximum CSV line length:  ", "256"),
            ("MAX_SEQ_LEN", "Maximum sequence length:", "64")
        ])
        
        self.create_section("Input CSV Format", [
            ("READ_CSV_HEADER", "Header format (comma-separated):   ", "sequence,label"),
            ("READ_CSV_SEQ_POS", "Sequence column position (0-based):", "0"),
            ("READ_CSV_COLS", "Number of columns:                             ", "2")
        ])
        
        self.create_section("Output CSV Format", [
            ("WRITE_CSV_HEADER", "Header format:              ", "sequence1,sequence2,label1,label2,score,alignment"),
            ("WRITE_CSV_SEQ1_POS", "First sequence position:", "0"),
            ("WRITE_CSV_SCORE_POS", "Score position:               ", "4"),
            ("WRITE_CSV_ALIGN_POS", "Alignment position:      ", "5"),
            ("WRITE_CSV_ALIGN_FMT", "Alignment format:        ", "\"('%s', '%s')\"")
        ])
        
        self.create_section("File Paths", [
            ("INPUT_FILE", "Single-threaded input file path:   ", "testing/datasets/avpdb.csv"),
            ("INPUT_MT_FILE", "Multi-threaded input file path:    ", "testing/datasets/avpdb_mega.csv"),
            ("OUTPUT_FILE", "Single-threaded output file path:", "results/results.csv"),
            ("OUTPUT_MT_FILE", "Multi-threaded output file path: ", "results/results_mega.csv")
        ])
        
        self.create_checkbox_section("Output Options", [
            ("MODE_WRITE", "Enable output writing", True)
        ])
        
        button_frame = ttk.Frame(self.content_frame)
        button_frame.pack(pady=20)
        
        save_btn = ttk.Button(button_frame, text="Save Configuration", command=self.save_config)
        save_btn.pack(side=tk.LEFT, padx=5)
        
        reset_btn = ttk.Button(button_frame, text="Reset to Defaults", command=self.reset_defaults)
        reset_btn.pack(side=tk.LEFT, padx=5)
        
        # Update scroll region
        self.content_frame.update_idletasks()
        canvas.configure(scrollregion=canvas.bbox("all"))
        
        self.root.mainloop()
    
    def browse_path(self, entry):
        from tkinter import filedialog
        current = Path(entry.get())
        
        if not current.is_absolute():
            current = project_root / current
            
        is_input = any(entry is self.fields[key] for key in ["INPUT_FILE", "INPUT_MT_FILE"])
        
        if is_input:
            filename = filedialog.askopenfilename(
                initialdir=current.parent,
                initialfile=current.name,
                defaultextension=".csv",
                filetypes=[("CSV files", "*.csv")]
            )
        else:
            filename = filedialog.asksaveasfilename(
                initialdir=current.parent,
                initialfile=current.name,
                defaultextension=".csv", 
                filetypes=[("CSV files", "*.csv")]
            )
        
        if filename:
            try:
                path = Path(filename).relative_to(project_root)
                entry.delete(0, tk.END)
                entry.insert(0, str(path))
            except ValueError:
                messagebox.showerror("Error", "Selected path must be within project directory")
    
    def create_tooltip(self, widget, text):
        def show_tooltip(event):
            tooltip = tk.Toplevel()
            tooltip.wm_overrideredirect(True)
            tooltip.wm_geometry(f"+{event.x_root+10}+{event.y_root+10}")
            
            label = ttk.Label(tooltip, text=text, background="#ffffe0")
            label.pack()
            
            def hide_tooltip():
                tooltip.destroy()
            
            widget.tooltip = tooltip
            widget.bind('<Leave>', lambda e: hide_tooltip())
            tooltip.bind('<Leave>', lambda e: hide_tooltip())
            
        widget.bind('<Enter>', show_tooltip)
    
    def create_section(self, title, fields):
        section = ttk.LabelFrame(self.content_frame, text=title, padding=10)
        section.pack(fill=tk.X, padx=5, pady=5)
        
        self.fields = getattr(self, 'fields', {})
        
        for var_name, label_text, default in fields:
            frame = ttk.Frame(section)
            frame.pack(fill=tk.X, pady=2)
            
            label = ttk.Label(frame, text=label_text)
            label.pack(side=tk.LEFT)
            
            entry = ttk.Entry(frame)
            if var_name in self.TOOLTIPS:
                self.create_tooltip(entry, self.TOOLTIPS[var_name])
            
            if var_name.endswith('_FILE'):
                browse_btn = ttk.Button(frame, text="Browse", command=lambda e=entry: self.browse_path(e))
                browse_btn.pack(side=tk.RIGHT)
            
            entry.insert(0, default)
            entry.pack(side=tk.RIGHT, expand=True)
            
            self.fields[var_name] = entry
    
    def create_checkbox_section(self, title, fields):
        section = ttk.LabelFrame(self.content_frame, text=title, padding=10)
        section.pack(fill=tk.X, padx=5, pady=5)
        
        self.checkboxes = getattr(self, 'checkboxes', {})
        
        for var_name, label_text, default in fields:
            frame = ttk.Frame(section)
            frame.pack(fill=tk.X, pady=2)
            
            var = tk.BooleanVar(value=default)
            checkbox = ttk.Checkbutton(frame, text=label_text, variable=var)
            if var_name in self.TOOLTIPS:
                self.create_tooltip(checkbox, self.TOOLTIPS[var_name])
            checkbox.pack(side=tk.LEFT)
            
            self.checkboxes[var_name] = var
    
    def validate_input(self):
        # Validate size limits
        try:
            max_csv_line = int(self.fields["MAX_CSV_LINE"].get())
            max_seq_len = int(self.fields["MAX_SEQ_LEN"].get())
            if max_csv_line < 32 or max_seq_len < 1:
                messagebox.showerror("Error", "MAX_CSV_LINE must be ≥32 and MAX_SEQ_LEN must be ≥1")
                return False
        except ValueError:
            messagebox.showerror("Error", "Size limits must be integers")
            return False
        
        # Validate CSV headers
        read_header = self.fields["READ_CSV_HEADER"].get().rstrip('\n') + '\n'
        write_header = self.fields["WRITE_CSV_HEADER"].get().rstrip('\n') + '\n'
        
        # Update the entry fields
        self.fields["READ_CSV_HEADER"].delete(0, tk.END)
        self.fields["READ_CSV_HEADER"].insert(0, read_header.rstrip('\n'))
        
        self.fields["WRITE_CSV_HEADER"].delete(0, tk.END)
        self.fields["WRITE_CSV_HEADER"].insert(0, write_header.rstrip('\n'))
        
        if not all(h.strip() and "," in h for h in [read_header, write_header]):
            messagebox.showerror("Error", "CSV headers must contain at least two columns")
            return False
        
        try:
            read_cols = int(self.fields["READ_CSV_COLS"].get())
            if read_cols < 2:
                messagebox.showerror("Error", "READ_CSV_COLS must be at least 2")
                return False
            
            # Validate that READ_CSV_SEQ_POS is within READ_CSV_COLS
            seq_pos = int(self.fields["READ_CSV_SEQ_POS"].get())
            if seq_pos >= read_cols:
                messagebox.showerror("Error", "READ_CSV_SEQ_POS must be less than READ_CSV_COLS")
                return False
        except ValueError:
            messagebox.showerror("Error", "READ_CSV_COLS must be an integer")
            return False
        
        # Validate column positions
        try:
            positions = [
                int(self.fields["READ_CSV_SEQ_POS"].get()),
                int(self.fields["WRITE_CSV_SEQ1_POS"].get()),
                int(self.fields["WRITE_CSV_SCORE_POS"].get()),
                int(self.fields["WRITE_CSV_ALIGN_POS"].get())
            ]
            if any(p < 0 for p in positions):
                messagebox.showerror("Error", "Column positions must be non-negative")
                return False
        except ValueError:
            messagebox.showerror("Error", "Column positions must be integers")
            return False
        
        # Validate alignment format
        align_fmt = self.fields["WRITE_CSV_ALIGN_FMT"].get()
        if align_fmt.count("%s") != 2:
            messagebox.showerror("Error", "Alignment format must contain exactly two %s specifiers")
            return False
        
        # Validate file paths
        for key in ["INPUT_FILE", "INPUT_MT_FILE", "OUTPUT_FILE", "OUTPUT_MT_FILE"]:
            path = Path(self.fields[key].get())
            try:
                # Convert absolute path to project relative
                if path.is_absolute():
                    path = path.relative_to(project_root)
                path_str = str(path)
            except ValueError:
                messagebox.showerror("Error", f"{key} must be within project directory")
                return False
            
            if not path_str.endswith('.csv'):
                messagebox.showerror("Error", f"{key} must have .csv extension")
                return False
            if any(c in path_str for c in '<>:"|?*'):
                messagebox.showerror("Error", f"{key} contains invalid characters")
                return False
            if len(path_str) > 260:  # Windows MAX_PATH
                messagebox.showerror("Error", f"{key} path is too long")
                return False
            
            # Check parent directories exist
            parent = project_root / path.parent
            if not parent.exists():
                try:
                    parent.mkdir(parents=True)
                except Exception as e:
                    messagebox.showerror("Error", f"Cannot create directory for {key}: {e}")
                    return False
        return True
    
    def reset_defaults(self):
        defaults = {
            "MAX_CSV_LINE": "256",
            "MAX_SEQ_LEN": "64",
            "READ_CSV_HEADER": "sequence,label",
            "READ_CSV_SEQ_POS": "0",
            "READ_CSV_COLS": "2",
            "WRITE_CSV_HEADER": "sequence1,sequence2,label1,label2,score,alignment",
            "WRITE_CSV_SEQ1_POS": "0",
            "WRITE_CSV_SCORE_POS": "4", 
            "WRITE_CSV_ALIGN_POS": "5",
            "WRITE_CSV_ALIGN_FMT": "\"('%s', '%s')\"",
            "INPUT_FILE": "testing/datasets/avpdb.csv",
            "INPUT_MT_FILE": "testing/datasets/avpdb_mega.csv",
            "OUTPUT_FILE": "results/results.csv",
            "OUTPUT_MT_FILE": "results/results_mega.csv"
        }
        
        for key, value in defaults.items():
            if key in self.fields:
                self.fields[key].delete(0, tk.END)
                self.fields[key].insert(0, value)
        
        checkbox_defaults = {
            "MODE_WRITE": True
        }
        
        for key, value in checkbox_defaults.items():
            if key in self.checkboxes:
                self.checkboxes[key].set(value)
    
    def save_config(self):
        if not self.validate_input():
            return
        
        try:
            with open(user_file, "r") as f:
                lines = f.readlines()
            
            new_lines = []
            for line in lines:
                if line.strip().startswith("#define"):
                    define_name = line.split()[1]
                    if define_name in self.fields:
                        value = self.fields[define_name].get()
                        
                        if define_name.endswith('_HEADER'):
                            value = value.rstrip('\n') + '\\n'
                        
                        if define_name.endswith('_FILE'):
                            try:
                                value = normalize_path(Path(value).relative_to(project_root))
                            except ValueError:
                                value = normalize_path(Path(value))
                        
                        if define_name.endswith('_FMT'):
                            value = value.replace('"', '\\"')
                        
                        if any(define_name.endswith(x) for x in 
                            ["_FILE", "_HEADER", "_FMT"]):
                            value = f'"{value}"'
                        
                        new_lines.append(f"#define {define_name} {value}\n")
                    
                    elif define_name in self.checkboxes:
                        value = "1" if self.checkboxes[define_name].get() else "0"
                        new_lines.append(f"#define {define_name} {value}\n")
                    
                    else:
                        new_lines.append(line)
                else:
                    new_lines.append(line)
            with open(user_file, "w") as f:
                f.writelines(new_lines)
            
            messagebox.showinfo("Success", "Configuration saved successfully!")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save configuration: {str(e)}")

if __name__ == "__main__":
    ConfigEditor()