from pathlib import Path
import json
import gc
from datetime import datetime
import os
import numpy as np

from src.core import AlignmentConfig, load_sequences, Sequence, SequenceCollection
from src.analysis import (
    SequenceAnalyzer,
    create_similarity_matrix,
    create_feature_matrix,
    cluster_sequences,
    analyze_similarity_groups,
    calculate_clustering_significance,
    analyze_predictive_power,
)
from src.visualization import SequenceVisualizer
from src.utils import archive_results, save_results

# TODO: Check gc.collect() calls and memory usage
# TODO: Expand Sequence dataclass usage
# TODO: Change results format to be more structured


def analyze_dataset(csv_path: str, output_dir: str = "results"):
    """Analyze protein sequences dataset with optimized processing."""
    # Setup
    Path(output_dir).mkdir(exist_ok=True)
    archive_results(output_dir)

    # Load sequences into collection (still underutilized, will expand later)
    raw_sequences, raw_labels = load_sequences(csv_path)
    sequence_collection = SequenceCollection()
    for seq, label in zip(raw_sequences, raw_labels):
        sequence_collection.add_sequence(Sequence(sequence=seq, label=label))

    sequences = sequence_collection.get_sequences()
    labels = sequence_collection.get_labels()

    analyzer = SequenceAnalyzer(AlignmentConfig())

    print("Creating matrices...")
    similarity_matrix, similarity_stats = create_similarity_matrix(sequences, analyzer)
    feature_matrix = create_feature_matrix(sequences, analyzer)
    gc.collect()

    print("Running clustering analysis...")
    clustering_results, kmeans_labels, dbscan_labels, spectral_labels = (
        cluster_sequences(feature_matrix, similarity_matrix)
    )

    print("Calculating clustering significance...")
    clustering_significance = calculate_clustering_significance(kmeans_labels, labels)
    gc.collect()

    print("Running ML analysis...")
    ml_results = analyze_predictive_power(feature_matrix, labels)
    gc.collect()

    print("Analyzing similarity groups...")
    analysis_results = analyze_similarity_groups(
        similarity_matrix, labels, clustering_results["kmeans"], feature_matrix
    )

    # Combine all results (will make it more structured later)
    final_results = {
        "analysis_timestamp": datetime.now().isoformat(),
        "dataset": {
            "path": csv_path,
            "sequences": int(len(sequences)),
            "positive_samples": int(sum(labels)),
            "negative_samples": int(len(labels) - sum(labels)),
        },
        "clustering_results": clustering_results,
        "clustering_significance": clustering_significance,
        "similarity_stats": similarity_stats,
        "ml_analysis": ml_results,
        "similarity_analysis": {
            k: float(v) if isinstance(v, (np.float32, np.float64, np.int64)) else v
            for k, v in analysis_results.items()
        },
    }

    print("Saving analysis results...")
    save_results(final_results, output_dir)
    gc.collect()

    print("Creating visualizations...")
    visualizer = SequenceVisualizer(output_dir)
    visualizer.create_visualization(
        sequences,
        labels,
        similarity_matrix,
        kmeans_labels,
        final_results,
    )

    # Final cleanup
    gc.collect()
    print(f"Analysis complete. Results saved to {output_dir}")


if __name__ == "__main__":
    analyze_dataset("datasets/avpdb.csv")
