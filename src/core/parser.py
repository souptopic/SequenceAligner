import polars as pl


def load_sequences(file_path):
    """Load sequences and their labels from CSV file."""
    df = pl.read_csv(file_path)
    sequences = df["sequence"].to_list()
    labels = df["label"].to_list()
    return sequences, labels
