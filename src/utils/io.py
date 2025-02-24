from pathlib import Path
import json
import zipfile
from datetime import datetime
import os
import numpy as np
from typing import Any, Dict


def _convert_to_serializable(obj: Any) -> Any:
    """Convert numpy types to Python native types for JSON serialization."""
    if isinstance(
        obj,
        (
            np.int8,
            np.int16,
            np.int32,
            np.int64,
            np.uint8,
            np.uint16,
            np.uint32,
            np.uint64,
        ),
    ):
        return int(obj)
    elif isinstance(obj, (np.float16, np.float32, np.float64)):
        return float(obj)
    elif isinstance(obj, np.ndarray):
        return obj.tolist()
    elif isinstance(obj, dict):
        return {key: _convert_to_serializable(value) for key, value in obj.items()}
    elif isinstance(obj, list):
        return [_convert_to_serializable(x) for x in obj]
    return obj


def archive_results(output_dir: str) -> None:
    """Archive existing results into timestamped zip."""
    output_path = Path(output_dir)
    if not output_path.exists():
        return

    files = [f for f in output_path.glob("*") if f.is_file()]
    if not files:
        return

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    archive_name = f"results_archive_{timestamp}.zip"
    archive_path = output_path / archive_name

    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as zipf:
        for file in files:
            if file.suffix not in [".zip", ".py", ".md"]:
                zipf.write(file, file.name)
                file.unlink()


def save_results(results: Dict, output_dir: str) -> None:
    """Save analysis results to JSON."""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    # Convert results to serializable format
    serializable_results = _convert_to_serializable(results)

    # Save results
    results_file = output_path / "analysis_results.json"
    with open(results_file, "w") as f:
        json.dump(serializable_results, f, indent=2)
