import numpy as np
from scipy import stats

from typing import List, Dict, Tuple
from sklearn.metrics import (
    silhouette_score,
    calinski_harabasz_score,
    davies_bouldin_score,
    adjusted_rand_score,
    adjusted_mutual_info_score,
)


def analyze_sequence_properties(feature_matrix: np.ndarray, labels: np.ndarray) -> Dict:
    """Analyze correlations between sequence properties and labels."""
    properties = [
        "molecular_weight",
        "aromaticity",
        "instability_index",
        "hydrophobicity",
        "charge",
        "hydrophobic_ratio",
        "net_charge_ratio",
        "amphipathicity",
        "helix_propensity",
    ]
    correlations = {}

    for i, prop in enumerate(properties):
        corr, p_val = stats.pointbiserialr(labels, feature_matrix[:, i])
        correlations[prop] = {"correlation": float(corr), "p_value": float(p_val)}

    return correlations


def analyze_similarity_groups(
    similarity_matrix: np.ndarray,
    labels: List[int],
    clustering_dict: Dict = None,
    feature_matrix: np.ndarray = None,
) -> Dict:
    """Analyze similarity scores within and between groups."""
    n = len(labels)
    labels_array = np.array(labels)

    # Create masks for positive and negative samples
    pos_mask = labels_array == 1
    neg_mask = labels_array == 0

    results = {
        "pos_samples": np.sum(pos_mask),
        "neg_samples": np.sum(neg_mask),
        "total_samples": n,
        "class_ratio": np.mean(pos_mask),
    }

    # Extract upper triangle indices to avoid counting pairs twice
    triu_indices = np.triu_indices(n, k=1)

    # Calculate within positive similarities
    if np.sum(pos_mask) > 1:
        pos_pairs = similarity_matrix[pos_mask][:, pos_mask]
        pos_triu = pos_pairs[np.triu_indices(np.sum(pos_mask), k=1)]
        if len(pos_triu) > 0:
            results["within_positive"] = float(np.mean(pos_triu))
            results["within_positive_std"] = float(np.std(pos_triu))

    # Calculate within negative similarities
    if np.sum(neg_mask) > 1:
        neg_pairs = similarity_matrix[neg_mask][:, neg_mask]
        neg_triu = neg_pairs[np.triu_indices(np.sum(neg_mask), k=1)]
        if len(neg_triu) > 0:
            results["within_negative"] = float(np.mean(neg_triu))
            results["within_negative_std"] = float(np.std(neg_triu))

    # Calculate between-group similarities
    if np.sum(pos_mask) > 0 and np.sum(neg_mask) > 0:
        between_similarities = similarity_matrix[pos_mask][:, neg_mask].flatten()
        if len(between_similarities) > 0:
            results["between_groups"] = float(np.mean(between_similarities))
            results["between_groups_std"] = float(np.std(between_similarities))

            # Statistical tests between positive and negative groups
            if len(pos_triu) > 0 and len(neg_triu) > 0:
                t_stat, p_val = stats.ttest_ind(pos_triu, neg_triu)
                results["ttest_pvalue"] = float(p_val)

                u_stat, p_val = stats.mannwhitneyu(pos_triu, neg_triu)
                results["mannwhitney_pvalue"] = float(p_val)

    # Overall similarity statistics
    all_similarities = similarity_matrix[triu_indices]
    results.update(
        {
            "similarity_mean": float(np.mean(all_similarities)),
            "similarity_std": float(np.std(all_similarities)),
            "similarity_min": float(np.min(all_similarities)),
            "similarity_max": float(np.max(all_similarities)),
            "similarity_quartiles": [
                float(q) for q in np.percentile(all_similarities, [25, 50, 75])
            ],
            "similarity_range": float(np.ptp(all_similarities)),
        }
    )

    # Clustering metrics
    if clustering_dict is not None and "labels" in clustering_dict:
        try:
            cluster_labels = np.array(
                clustering_dict["labels"]
            )  # Convert to numpy array
            results.update(
                {
                    "silhouette_score": float(
                        silhouette_score(similarity_matrix, cluster_labels)
                    ),
                    "calinski_harabasz_score": float(
                        calinski_harabasz_score(similarity_matrix, cluster_labels)
                    ),
                    "davies_bouldin_score": float(
                        davies_bouldin_score(similarity_matrix, cluster_labels)
                    ),
                }
            )
        except Exception as e:
            print(f"Warning: Could not calculate some clustering metrics: {e}")

    if feature_matrix is not None:
        try:
            property_correlations = analyze_sequence_properties(
                feature_matrix, np.array(labels)
            )
            results["property_correlations"] = property_correlations
        except Exception as e:
            print(f"Warning: Could not calculate property correlations: {e}")

    return results


def calculate_clustering_significance(clusters: np.ndarray, labels: np.ndarray) -> Dict:
    """Calculate statistical significance of clustering results."""
    # Convert inputs to numpy arrays
    clusters = np.asarray(clusters)
    labels = np.asarray(labels)

    # Create contingency table
    unique_clusters = np.unique(clusters)
    contingency = np.zeros((2, len(unique_clusters)))

    for i, cluster in enumerate(unique_clusters):
        mask = clusters == cluster
        # Convert boolean mask results to integer
        contingency[0, i] = np.sum((labels[mask] == 0).astype(int))
        contingency[1, i] = np.sum((labels[mask] == 1).astype(int))

    # Chi-square test
    chi2, chi2_pval, _, _ = stats.chi2_contingency(contingency)

    # Fisher's exact test (if applicable)
    if len(unique_clusters) == 2:
        fisher_stat, fisher_pval = stats.fisher_exact(contingency)
    else:
        fisher_stat, fisher_pval = None, None

    # Clustering metrics
    ari = adjusted_rand_score(labels, clusters)
    ami = adjusted_mutual_info_score(labels, clusters)

    return {
        "chi2_statistic": float(chi2),
        "chi2_pvalue": float(chi2_pval),
        "fisher_statistic": float(fisher_stat) if fisher_stat else None,
        "fisher_pvalue": float(fisher_pval) if fisher_pval else None,
        "adjusted_rand_index": float(ari),
        "adjusted_mutual_info": float(ami),
    }
