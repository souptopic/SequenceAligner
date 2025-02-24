import parasail
from Bio.SeqUtils.ProtParam import ProteinAnalysis
from collections import Counter
from typing import Tuple, Dict
import numpy as np
from functools import lru_cache


class SequenceAnalyzer:
    def __init__(self, config):
        self.config = config
        self.parasail_matrix = parasail.Matrix(self.config.matrix)
        self.hydrophobicity_scale = {
            "A": 0.62,
            "R": -2.53,
            "N": -0.78,
            "D": -0.90,
            "C": 0.29,
            "Q": -0.85,
            "E": -0.74,
            "G": 0.48,
            "H": -0.40,
            "I": 1.38,
            "L": 1.06,
            "K": -1.50,
            "M": 0.64,
            "F": 1.19,
            "P": 0.12,
            "S": -0.18,
            "T": -0.05,
            "W": 0.81,
            "Y": 0.26,
            "V": 1.08,
        }

    def _calculate_identity(self, alignment) -> float:
        """Calculate sequence identity from alignment."""
        seq1, seq2 = str(alignment[0]), str(alignment[1])
        matches = sum(1 for a, b in zip(seq1, seq2) if a == b)
        return matches / max(len(seq1.replace("-", "")), len(seq2.replace("-", "")))

    def _calculate_amphipathicity(self, sequence: str) -> float:
        """Calculate sequence amphipathicity using hydrophobic moment."""
        if not sequence:
            return 0.0

        angle = 100  # Angle in degrees for alpha-helix
        angle_rad = angle * np.pi / 180

        sum_sin = sum(
            self.hydrophobicity_scale[aa] * np.sin(i * angle_rad)
            for i, aa in enumerate(sequence)
            if aa in self.hydrophobicity_scale
        )
        sum_cos = sum(
            self.hydrophobicity_scale[aa] * np.cos(i * angle_rad)
            for i, aa in enumerate(sequence)
            if aa in self.hydrophobicity_scale
        )

        moment = np.sqrt(sum_sin**2 + sum_cos**2) / len(sequence)
        return moment

    @lru_cache(maxsize=1024)
    def analyze_sequence(self, sequence: str) -> dict:
        """Enhanced sequence analysis for AMPs."""
        analysis = ProteinAnalysis(sequence)
        aa_count = Counter(sequence)

        return {
            "length": len(sequence),
            "molecular_weight": analysis.molecular_weight(),
            "aromaticity": analysis.aromaticity(),
            "instability_index": analysis.instability_index(),
            "hydrophobicity": analysis.gravy(),
            "charge": analysis.charge_at_pH(7.0),
            "hydrophobic_ratio": sum(aa_count[aa] for aa in "AILMFWYV") / len(sequence),
            "net_charge_ratio": (
                sum(aa_count[aa] for aa in "RK") - sum(aa_count[aa] for aa in "DE")
            )
            / len(sequence),
            "amphipathicity": self._calculate_amphipathicity(sequence),
            "helix_propensity": sum(aa_count[aa] for aa in "AEILMRKHW") / len(sequence),
        }

    @lru_cache(maxsize=1024)
    def calculate_similarity(self, seq1: str, seq2: str) -> Tuple[float, dict]:
        """Calculate sequence similarity using parasail."""
        result = parasail.sw_stats(
            seq1,
            seq2,
            self.config.gap_open_penalty,
            self.config.gap_extension_penalty,
            self.parasail_matrix,
        )

        identity = result.matches / max(len(seq1), len(seq2))

        stats = {
            "identity": identity,
            "matches": result.matches,
            "score": float(result.score),
            "similar": result.similar,
            "length": max(len(seq1), len(seq2)),
        }

        return result.score, stats
