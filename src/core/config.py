from dataclasses import dataclass
from typing import Tuple, Optional

# In development


@dataclass
class AlignmentConfig:
    """Configuration for sequence alignment."""

    matrix: str = "blosum45"

    matrix_configs = {
        "blosum45": {"gap_open": 14, "gap_extend": 2},
        "blosum50": {"gap_open": 13, "gap_extend": 2},
        "blosum62": {"gap_open": 11, "gap_extend": 1},
        "blosum80": {"gap_open": 10, "gap_extend": 1},
        "pam30": {"gap_open": 9, "gap_extend": 1},
        "pam70": {"gap_open": 10, "gap_extend": 1},
        "pam250": {"gap_open": 13, "gap_extend": 2},
    }

    def __post_init__(self):
        if self.matrix not in self.matrix_configs:
            raise ValueError(
                f"Unsupported matrix: {self.matrix}. "
                f"Supported matrices: {list(self.matrix_configs.keys())}"
            )
        self.gap_open_penalty, self.gap_extension_penalty = self.matrix_configs[
            self.matrix
        ].values()


@dataclass
class AnalysisConfig:
    """Configuration for analysis and visualization."""

    n_clusters: int = 2
    cluster_random_state: int = 42
    visualization_size: Tuple[int, int] = (20, 8)
    heatmap_cmap: str = "YlOrRd"
    # add more that are relevant to analysis and visualization


@dataclass
class PerformanceConfig:
    """Configuration for performance optimization."""

    cache_size: int = 1024
    # add more that are relevant to performance optimization
