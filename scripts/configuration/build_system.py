import platform
import threading
import subprocess
import atexit
from pathlib import Path

try:
    from .config_schema import project_root
except ImportError:
    from config_schema import project_root

class BuildEnvironment:
    def __init__(self):
        self.os_name = platform.system().lower()
        self.make_cmd = 'mingw32-make' if self.os_name == 'windows' else 'make'
        self._lock = threading.Lock()
        self._build_states = {"all": False, "tune": False}
        self._active_processes = {}
        self._state_callbacks = set()
        self._is_building = False
        atexit.register(self._cleanup_processes)
    
    MSYS2_PATHS = {
            "SCOOP_GLOBAL": "C:/ProgramData/scoop/apps/msys2/current/ucrt64/bin",
            "SCOOP_USER": str(Path.home() / "scoop/apps/msys2/current/ucrt64/bin"),
            "MSYS2_DEFAULT": "C:/msys64/ucrt64/bin"
        }
    
    def verify(self):
        if self.os_name != 'windows':
            return True, None
        
        if not any(Path(path).exists() for path in self.MSYS2_PATHS.values()):
            return False, "MSYS2 UCRT64 not found. Download from https://www.msys2.org/"
        
        try:
            for cmd in ['gcc --version', 'mingw32-make --version']:
                subprocess.run(cmd, shell=True, check=True, capture_output=True, timeout=2)
            return True, None
        except subprocess.SubprocessError:
            return False, "GCC/Make not found. Run in MSYS2: pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make"
        except Exception as e:
            return False, f"Verification failed: {str(e)}"

    def _handle_process_output(self, process, output_fn):
        try:
            while True:
                line = process.stdout.readline() if process.stdout else ''
                if not line and process.poll() is not None:
                    break
                if line:
                    output_fn(line)
            
            if process.returncode != 0:
                output_fn(f"Process failed with exit code {process.returncode}\n")
            return process.returncode == 0
        except Exception as e:
            output_fn(f"Error monitoring process: {str(e)}\n")
            return False
    
    def _run_process(self, cmd, output_fn, cwd, on_complete = None, process_key = None):
        try:
            process = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, shell=True, cwd=cwd
            )
            
            if process_key:
                self._active_processes[process_key] = process

            def monitor():
                success = self._handle_process_output(process, output_fn)
                if process_key and process_key in self._active_processes:
                    del self._active_processes[process_key]
                if not self.is_busy():
                    self._update_button_states(True)
                if on_complete:
                    on_complete(success)

            threading.Thread(target=monitor, daemon=True).start()

        except Exception as e:
            output_fn(f"Error starting process: {str(e)}\n")
            if on_complete:
                on_complete(False)
    
    def run_make(self, output_fn, target, cwd = project_root, on_complete = None):
        with self._lock:
            self._is_building = True
            self._build_states[target] = True
            self._build_states["tune" if target == "all" else "all"] = False
            self._update_button_states(False)
        
        def wrapped_complete(success):
            with self._lock:
                self._is_building = False
                if not self.is_busy():
                    self._update_button_states(True)
            if on_complete:
                on_complete(success)
        
        self._run_process(f"{self.make_cmd} {target}", output_fn, cwd, wrapped_complete)
    
    def run_binary(self, output_fn, name, cwd = project_root):
        if name in self._active_processes and self._active_processes[name].poll() is None:
            return
        
        binary_path = str(cwd / 'bin' / f'{name}{".exe" if self.os_name == "windows" else ""}')
        self._run_process(binary_path, output_fn, cwd, process_key=name)
        self._update_button_states(False)
    
    def build_and_run(self, output_fn, binary, target, cwd = project_root):
        if self.is_busy():
            return
        
        if not self._build_states.get(target, False):
            self.run_make(output_fn, target, cwd, on_complete=lambda success: success and self.run_binary(output_fn, binary, cwd))
        else:
            self.run_binary(output_fn, binary, cwd)
    
    def _update_button_states(self, enabled):
        for callback in self._state_callbacks:
            callback(enabled)
    
    def is_busy(self):
        return self._is_building or bool(self._active_processes)
    
    def register_button_callback(self, callback):
        self._state_callbacks.add(callback)
        callback(not self.is_busy())
    
    def reset_build_status(self):
        with self._lock:
            self._build_states.update({k: False for k in self._build_states})
    
    def init(self, output_fn, cwd = project_root):
        if not Path(cwd, "testing/datasets/avpdb_mt.csv").exists():
            self.run_make(output_fn, "dataset", cwd)
        self.run_make(output_fn, "all", cwd)
    
    def _cleanup_processes(self):
        for proc in self._active_processes.values():
            if proc and proc.poll() is None:
                try:
                    proc.terminate()
                    proc.wait(timeout=1)
                except:
                    proc.kill()

build_env = BuildEnvironment()

DISPLAY_BINARY_PROFILE = [
    ("Singlethreaded", "ms", "all"),
    ("Multithreaded", "mt", "all"),
    ("Tuning", "batch", "tune"),
]