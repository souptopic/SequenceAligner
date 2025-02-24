from sklearn.cluster import KMeans, DBSCAN, SpectralClustering
from sklearn.mixture import GaussianMixture
from sklearn.preprocessing import StandardScaler
import numpy as np
from typing import Dict, Tuple
import gc


def _prepare_similarity_matrix(similarity_matrix: np.ndarray) -> np.ndarray:
    """Prepare similarity matrix for spectral clustering by handling NaN values."""
    # Replace NaN values with the mean of non-NaN values
    matrix = similarity_matrix.copy()
    mask = np.isnan(matrix)
    if mask.any():
        matrix[mask] = np.nanmean(matrix)

    # Make matrix symmetric
    matrix = (matrix + matrix.T) / 2

    # Make it positive semi-definite by adding a small constant to diagonal
    min_val = np.min(matrix)
    if min_val < 0:
        matrix -= min_val
    np.fill_diagonal(matrix, np.diag(matrix) + 1e-8)

    return matrix


def cluster_sequences(
    feature_matrix: np.ndarray, similarity_matrix: np.ndarray
) -> Dict:
    """Multiple clustering approaches for AMP analysis."""
    results = {}
    kmeans_labels = None
    dbscan_labels = None
    spectral_labels = None

    print("Running KMeans clustering...")
    kmeans = KMeans(n_clusters=2, random_state=42, n_init=10, max_iter=300, tol=1e-4)
    kmeans_labels = kmeans.fit_predict(feature_matrix)

    results["kmeans"] = {
        "cluster_sizes": [int(sum(kmeans_labels == i)) for i in range(2)],
        "inertia": float(kmeans.inertia_),
        "cluster_centers": kmeans.cluster_centers_.tolist(),
    }
    gc.collect()

    print("Running DBSCAN clustering...")
    try:
        dbscan = DBSCAN(eps=0.5, min_samples=5)
        dbscan_labels = dbscan.fit_predict(feature_matrix)
        unique_labels = np.unique(dbscan_labels)
        results["dbscan"] = {
            "n_clusters": int(len(unique_labels)),
            "n_noise": int(sum(dbscan_labels == -1)),
            "cluster_sizes": [
                int(sum(dbscan_labels == i)) for i in unique_labels if i != -1
            ],
        }
    except Exception as e:
        print(f"DBSCAN clustering failed: {e}")
        results["dbscan"] = {"error": str(e)}
    gc.collect()

    print("Running Spectral clustering...")
    try:
        processed_similarity = _prepare_similarity_matrix(similarity_matrix)
        spectral = SpectralClustering(
            n_clusters=2, affinity="precomputed", random_state=42, n_init=100
        )
        spectral_labels = spectral.fit_predict(processed_similarity)
        results["spectral"] = {
            "cluster_sizes": [int(sum(spectral_labels == i)) for i in range(2)]
        }
        del processed_similarity
    except Exception as e:
        print(f"Spectral clustering failed: {e}")
        results["spectral"] = {"error": str(e)}
    gc.collect()

    return results, kmeans_labels, dbscan_labels, spectral_labels
