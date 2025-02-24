from typing import List, Dict, Optional
from dataclasses import dataclass

# In development


@dataclass
class Sequence:
    """Represents a single protein sequence with metadata."""

    sequence: str
    label: Optional[int] = None
    id: Optional[str] = None
    properties: Optional[Dict] = None

    def __post_init__(self):
        """Validate sequence after initialization."""
        if not self._validate():
            raise ValueError("Invalid sequence: contains invalid amino acids")
        self.properties = self.properties or {}

    def _validate(self) -> bool:
        """Validate if sequence contains only valid amino acids."""
        valid_residues = set("ACDEFGHIKLMNPQRSTVWY")
        return all(aa in valid_residues for aa in self.sequence)

    @property
    def length(self) -> int:
        """Get sequence length."""
        return len(self.sequence)


class SequenceCollection:
    """Collection of protein sequences with analysis capabilities."""

    def __init__(self):
        self.sequences: List[Sequence] = []
        self._labels: Optional[List[int]] = None

    def add_sequence(self, sequence: Sequence) -> None:
        """Add a sequence to the collection."""
        self.sequences.append(sequence)
        self._labels = None  # Reset cached labels

    def get_sequences(self) -> List[str]:
        """Get raw sequences as strings."""
        return [seq.sequence for seq in self.sequences]

    def get_labels(self) -> List[int]:
        """Get sequence labels."""
        if self._labels is None:
            self._labels = [seq.label for seq in self.sequences]
        return self._labels

    @property
    def size(self) -> int:
        """Get number of sequences in collection."""
        return len(self.sequences)
