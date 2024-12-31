TOOLTIPS = {
    "MAX_CSV_LINE": "Maximum length of any line in CSV files (must be ≥32)",
    "MAX_SEQ_LEN": "Maximum length of any sequence (must be ≥1)",
    "GAP_PENALTY": "Penalty for gaps when aligning sequences",
    "BATCH_SIZE": "Number of sequences to process in each batch for multi-threaded mode",
    "READ_CSV_HEADER":
"""Input CSV Format Rules:
- One sequence per line
- Fixed number of columns 
- Additional columns preserved in output
- Names are arbitrary, only positions matter

Example: animal,sequence,data1,data2""",
    "READ_CSV_SEQ_POS": "Position of sequence column in input (0-based)",
    "READ_CSV_COLS": "Total number of columns in input CSV",
    "WRITE_CSV_HEADER":
"""Output CSV Format Rules:
- Each row is paired with the next row
- Output columns come in pairs (one from first row, one from second row) 
- Column pairs must be adjacent in header
- Required special columns: score, alignment
- All column names are arbitrary, only positions matter
- Number of columns = 2 * number of read columns + 2

Examples:
seq_a,seq_b,score,align,name_x,name_y,type1,type2
score,first,second,alignment,cat1,cat2,id_x,id_y
type_orig,type_comp,name1,name2,seq_first,seq_second,aligned_sequence,points""",
    "WRITE_CSV_SEQ1_POS": "Position of first sequence in output",
    "WRITE_CSV_SCORE_POS": "Position of alignment score in output",
    "WRITE_CSV_ALIGN_POS": "Position of alignment result in output",
    "WRITE_CSV_ALIGN_FMT": "Printf format for alignment (must contain two %s)",
    "INPUT_FILE": "Path to input file for single-threaded mode",
    "INPUT_MT_FILE": "Path to input file for multi-threaded mode", 
    "OUTPUT_FILE": "Path to output file for single-threaded mode",
    "OUTPUT_MT_FILE": "Path to output file for multi-threaded mode",
    "MODE_WRITE": "Uncheck to disable writing to output CSV file",
    "SIMILARITY_ANALYSIS": "Enable printing similarity analysis to terminal (unoptimized, will get changed to write to file)"
}

DEFAULT_VALUES = {
    "MAX_CSV_LINE": "256",
    "MAX_SEQ_LEN": "64",
    "GAP_PENALTY": "-4",
    "BATCH_SIZE": "32768",
    "READ_CSV_HEADER": "sequence,label",
    "READ_CSV_SEQ_POS": "0",
    "READ_CSV_COLS": "2",
    "WRITE_CSV_HEADER": "sequence1,sequence2,label1,label2,score,alignment",
    "WRITE_CSV_SEQ1_POS": "0",
    "WRITE_CSV_SCORE_POS": "4", 
    "WRITE_CSV_ALIGN_POS": "5",
    "WRITE_CSV_ALIGN_FMT": "\"('%s', '%s')\"",
    "INPUT_FILE": "testing/datasets/avpdb.csv",
    "INPUT_MT_FILE": "testing/datasets/avpdb_mt.csv",
    "OUTPUT_FILE": "results/results.csv",
    "OUTPUT_MT_FILE": "results/results_mt.csv"
}

DEFAULT_CHECKBOXES = {
    "MODE_WRITE": True,
    "SIMILARITY_ANALYSIS": False
}

DISPLAY_NAMES = {
    "MAX_CSV_LINE": "Maximum CSV Line Length",
    "MAX_SEQ_LEN": "Maximum Sequence Length", 
    "GAP_PENALTY": "Gap Penalty",
    "BATCH_SIZE": "Batch Size",
    "READ_CSV_HEADER": "Input CSV Header",
    "READ_CSV_SEQ_POS": "Sequence Column Position",
    "READ_CSV_COLS": "Number of Columns",
    "WRITE_CSV_HEADER": "Output CSV Header",
    "WRITE_CSV_SEQ1_POS": "First Sequence Position", 
    "WRITE_CSV_SCORE_POS": "Score Column Position",
    "WRITE_CSV_ALIGN_POS": "Alignment Column Position",
    "WRITE_CSV_ALIGN_FMT": "Alignment Format",
    "INPUT_FILE": "Input File (Single-thread)",
    "INPUT_MT_FILE": "Input File (Multi-thread)",
    "OUTPUT_FILE": "Output File (Single-thread)", 
    "OUTPUT_MT_FILE": "Output File (Multi-thread)",
    "MODE_WRITE": "Enable Writing to CSV File",
    "SIMILARITY_ANALYSIS": "Enable Similarity Analysis"
}

from pathlib import Path
project_root = Path(__file__).parent.parent.parent.resolve()
user_file = project_root / 'include' / 'user.h'

from typing import Tuple, Optional

def validate_config(fields, checkboxes) -> Tuple[bool, Optional[str]]:
    try:
        read_header = fields["READ_CSV_HEADER"].get().strip()
        if not read_header:
            return False, "Input Header cannot be empty"
            
        read_cols = read_header.count(',') + 1
        write_mode = checkboxes["MODE_WRITE"].get()
        
        numeric_rules = {
            "MAX_CSV_LINE": (32, "≥32"),
            "MAX_SEQ_LEN": (1, "≥1"),
            "BATCH_SIZE": (1, "≥1"),
            "GAP_PENALTY": (0, "<0", lambda x: x < 0),
            "READ_CSV_SEQ_POS": (read_cols, f"between 0 and {read_cols-1}", lambda x: 0 <= x < read_cols),
            "READ_CSV_COLS": (read_cols, f"equal to {read_cols}", lambda x: x == read_cols)
        }
        
        for key, rule in numeric_rules.items():
            try:
                val = int(fields[key].get())
                check = rule[2] if len(rule) > 2 else lambda x: x >= rule[0]
                if not check(val):
                    return False, f"{DISPLAY_NAMES[key]} must be {rule[1]}"
            except ValueError:
                return False, f"Invalid numeric value for {DISPLAY_NAMES[key]}"
        
        if write_mode:
            write_header = fields["WRITE_CSV_HEADER"].get().strip()
            if not write_header:
                return False, "Output Header cannot be empty"
            
            write_cols = write_header.count(',') + 1
            if write_cols != (2 * read_cols + 2):
                return False, f"Output must have {2 * read_cols + 2} columns (found {write_cols})"
            
            pos_fields = ["WRITE_CSV_SEQ1_POS", "WRITE_CSV_SCORE_POS", "WRITE_CSV_ALIGN_POS"]
            positions = [int(fields[k].get()) for k in pos_fields]
            
            if any(not 0 <= p < write_cols for p in positions):
                return False, "Column positions must be within output column range"
            
            if len(set(positions + [positions[0] + 1])) != 4:
                return False, "Output columns must have unique positions"
            
            if fields["WRITE_CSV_ALIGN_FMT"].get().count("%s") != 2:
                return False, "Alignment format must contain exactly two %s placeholders"
        
        for key in ["INPUT_FILE", "INPUT_MT_FILE", "OUTPUT_FILE", "OUTPUT_MT_FILE"]:
            path = Path(fields[key].get())
            try:
                if path.is_absolute():
                    path = path.relative_to(project_root)
                if not str(path).endswith('.csv'):
                    return False, f"{DISPLAY_NAMES[key]} must have .csv extension"
                if len(str(project_root / path)) > 260:
                    return False, f"{DISPLAY_NAMES[key]} path exceeds maximum length"
                if key.startswith('OUTPUT'):
                    (project_root / path.parent).mkdir(parents=True, exist_ok=True)
            except Exception as e:
                return False, f"Invalid path for {DISPLAY_NAMES[key]}: {str(e)}"
        
        return True, None
    
    except Exception as e:
        return False, f"Unexpected validation error: {str(e)}"

def save_config(fields, checkboxes) -> Tuple[bool, Optional[str]]:
    try:
        with open(user_file, "r") as f:
            lines = f.readlines()
        
        new_lines = []
        for line in lines:
            if not line.strip().startswith('#define'):
                new_lines.append(line)
                continue
            
            name = line.split()[1]
            if name in fields:
                value = fields[name].get()
                
                if name.endswith('_HEADER'):
                    value = value.rstrip('\n') + '\\n'
                elif name.endswith('_FILE'):
                    try:
                        value = str(Path(value).relative_to(project_root))
                    except ValueError:
                        value = str(Path(value))
                    value = value.replace('\\', '/')
                elif name.endswith('_FMT'):
                    value = value.replace('"', '\\"')
                
                if any(name.endswith(x) for x in ['_FILE', '_HEADER', '_FMT']):
                    value = f'"{value}"'
                
                new_lines.append(f"#define {name} {value}\n")
            
            elif name in checkboxes:
                value = "1" if checkboxes[name].get() else "0"
                new_lines.append(f"#define {name} {value}\n")
            
            else:
                new_lines.append(line)
        
        with open(user_file, "w") as f:
            f.writelines(new_lines)
        
        return True, None
    
    except Exception as e:
        return False, f"Failed to save configuration: {e}"