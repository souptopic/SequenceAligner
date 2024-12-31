def main():
    try:
        import importlib.util
        if importlib.util.find_spec('customtkinter'):
            from scripts.modern import ModernConfigEditor
            editor = ModernConfigEditor()
        else:
            from scripts.editor_window import ConfigEditor
            editor = ConfigEditor()
        editor.run()
    except Exception as e:
        import tkinter as tk
        from tkinter import messagebox
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("Error", f"Failed to start application: {str(e)}")
        root.destroy()

if __name__ == "__main__":
    main()