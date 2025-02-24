from collections import Counter
import numpy as np
from typing import List, Dict
from Bio.SeqUtils.ProtParam import ProteinAnalysis


def analyze_aa_composition(sequences: List[str]) -> Dict:
    """Analyze amino acid composition across sequences."""
    # Standard amino acids
    aa_list = list("ACDEFGHIKLMNPQRSTVWY")

    # Initialize counters
    total_aa = Counter()
    aa_by_class = {"positive": Counter(), "negative": Counter()}

    # Group properties
    aa_properties = {
        "hydrophobic": set("AILMFWV"),
        "polar": set("NCQSTY"),
        "charged": set("DEKRH"),
        "aromatic": set("FWY"),
    }

    property_counts = {prop: 0 for prop in aa_properties}

    for seq in sequences:
        counts = Counter(seq)
        total_aa.update(counts)

        # Count properties
        for prop, aa_set in aa_properties.items():
            property_counts[prop] += sum(counts[aa] for aa in aa_set)

    # Calculate frequencies
    total_residues = sum(total_aa.values())
    aa_frequencies = {aa: count / total_residues for aa, count in total_aa.items()}

    # Calculate property percentages
    property_percentages = {
        prop: (count / total_residues) * 100 for prop, count in property_counts.items()
    }

    return {
        "aa_counts": dict(total_aa),
        "aa_frequencies": aa_frequencies,
        "property_percentages": property_percentages,
        "total_residues": total_residues,
        "unique_aa": len(set("".join(sequences))),
    }
