from concurrent.futures import ProcessPoolExecutor, as_completed
from typing import List, Dict, Tuple
import numpy as np
from tqdm.auto import tqdm
import multiprocessing
from itertools import combinations


def create_feature_matrix(sequences: List[str], analyzer) -> np.ndarray:
    """Create a feature matrix from sequence properties."""
    features = []
    for seq in sequences:
        props = analyzer.analyze_sequence(seq)
        features.append(
            [
                props["molecular_weight"],
                props["aromaticity"],
                props["instability_index"],
                props["hydrophobicity"],
                props["charge"],
                props["hydrophobic_ratio"],
                props["net_charge_ratio"],
                props["amphipathicity"],
                props["helix_propensity"],
            ]
        )
    return np.array(features)


def _process_chunk(args):
    """Process a chunk of sequence pairs with progress tracking."""
    from .scoring import SequenceAnalyzer
    from ..core.config import AlignmentConfig

    chunk_pairs, sequences = args
    config = AlignmentConfig()
    analyzer = SequenceAnalyzer(config)

    results = []
    for i, j in chunk_pairs:
        try:
            score, stats = analyzer.calculate_similarity(sequences[i], sequences[j])
            results.append((i, j, score))
        except Exception as e:
            print(f"Error processing sequences {i} and {j}: {str(e)}")
            results.append((i, j, 0.0))
    return results


def create_similarity_matrix(sequences: List[str], analyzer) -> Tuple[np.ndarray, Dict]:
    """Create similarity matrix using optimized parallel processing."""
    n = len(sequences)
    similarity_matrix = np.zeros((n, n), dtype=np.float32)
    total_comparisons = (n * (n - 1)) // 2

    stats = {
        "total_comparisons": total_comparisons,
        "total_sequences": n,
        "min_similarity": float("inf"),
        "max_similarity": float("-inf"),
        "sum_similarity": 0.0,
        "processed_pairs": 0,
    }

    n_cores = multiprocessing.cpu_count()
    chunk_size = max(1, total_comparisons // (n_cores * 4))

    pairs = list(combinations(range(n), 2))

    chunks = [pairs[i : i + chunk_size] for i in range(0, len(pairs), chunk_size)]

    with ProcessPoolExecutor(max_workers=n_cores) as executor:
        futures = [
            executor.submit(_process_chunk, (chunk, sequences)) for chunk in chunks
        ]

        with tqdm(
            total=len(chunks), desc="Processing chunks", position=0
        ) as pbar_chunks:
            with tqdm(
                total=total_comparisons, desc="Computing comparisons", position=1
            ) as pbar_comparisons:
                for future in as_completed(futures):
                    chunk_results = future.result()
                    for i, j, score in chunk_results:
                        similarity_matrix[i, j] = similarity_matrix[j, i] = score

                        stats["min_similarity"] = min(stats["min_similarity"], score)
                        stats["max_similarity"] = max(stats["max_similarity"], score)
                        stats["sum_similarity"] += score
                        stats["processed_pairs"] += 1

                    pbar_chunks.update(1)
                    pbar_comparisons.update(len(chunk_results))

    np.fill_diagonal(similarity_matrix, 1.0)

    stats["mean_similarity"] = float(stats["sum_similarity"] / stats["processed_pairs"])
    stats["min_similarity"] = float(stats["min_similarity"])
    stats["max_similarity"] = float(stats["max_similarity"])

    return similarity_matrix, stats
