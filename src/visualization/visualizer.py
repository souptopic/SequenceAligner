import os
import seaborn as sns
import matplotlib.pyplot as plt
from sklearn.decomposition import PCA
import numpy as np
import gc

# Will move to main.py later
from ..analysis.aa_analysis import analyze_aa_composition


class SequenceVisualizer:
    def __init__(self, output_dir="output"):
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
        plt.switch_backend("agg")

    def create_visualization(
        self, sequences, labels, similarity_matrix, kmeans_labels, stats
    ):
        """Create separate visualizations for each analysis component."""
        pca = PCA(n_components=2)
        coords = pca.fit_transform(similarity_matrix)

        # Process visualizations in batches with memory cleanup
        print("Creating similarity heatmap...")
        self._create_similarity_heatmap(similarity_matrix)
        print("Creating similarity distribution plot...")
        self._create_similarity_distribution(similarity_matrix, labels)
        del similarity_matrix
        gc.collect()

        print("Creating PCA plot...")
        self._create_pca_plot(coords, labels)
        print("Creating clusters plot...")
        self._create_clusters_plot(coords, kmeans_labels)
        del coords
        gc.collect()

        print("Creating sequence length plot...")
        self._create_sequence_lengths_plot(sequences, labels)
        print("Creating AA composition plot...")
        self._create_aa_composition_plot(sequences)
        del sequences
        gc.collect()

        print("Creating stats plots...")
        self._create_stats_plots(stats)

    def _save_plot(self, name):
        """Helper method to save plots."""
        output_path = os.path.join(self.output_dir, f"{name}.png")
        plt.savefig(output_path, dpi=300, bbox_inches="tight")
        plt.close()

    def _create_similarity_heatmap(self, similarity_matrix):
        plt.figure(figsize=(10, 8))
        sns.heatmap(
            similarity_matrix, cmap="YlOrRd", xticklabels=False, yticklabels=False
        )
        plt.title("Sequence Similarity Matrix")
        self._save_plot("similarity_heatmap")

    def _create_pca_plot(self, similarity_matrix, labels):
        plt.figure(figsize=(10, 8))
        pca = PCA(n_components=2)
        coords = pca.fit_transform(similarity_matrix)
        scatter = plt.scatter(
            coords[:, 0], coords[:, 1], c=labels, cmap="coolwarm", s=100, alpha=0.6
        )
        plt.title("PCA of Sequence Similarities")
        plt.legend(*scatter.legend_elements(), title="Class Labels")
        self._save_plot("pca_plot")

    def _create_similarity_distribution(self, similarity_matrix, labels):
        plt.figure(figsize=(10, 8))
        scores_positive = []
        scores_negative = []

        n = len(labels)
        for i in range(n):
            for j in range(i + 1, n):
                score = similarity_matrix[i, j]
                if labels[i] == labels[j] == 1:
                    scores_positive.append(score)
                elif labels[i] == labels[j] == 0:
                    scores_negative.append(score)

        sns.kdeplot(data=scores_positive, label="Within positive class", color="red")
        sns.kdeplot(data=scores_negative, label="Within negative class", color="blue")
        plt.title("Distribution of Similarity Scores")
        plt.xlabel("Similarity Score")
        plt.legend()
        self._save_plot("similarity_distribution")

    def _create_clusters_plot(self, similarity_matrix, clusters):
        plt.figure(figsize=(10, 8))
        pca = PCA(n_components=2)
        coords = pca.fit_transform(similarity_matrix)
        cluster_labels = np.asarray(clusters).flatten()
        scatter = plt.scatter(
            coords[:, 0],
            coords[:, 1],
            c=cluster_labels,
            cmap="viridis",
            s=100,
            alpha=0.6,
        )
        plt.title("Sequence Clusters")
        plt.legend(*scatter.legend_elements(), title="Clusters")
        self._save_plot("clusters")

    def _create_sequence_lengths_plot(self, sequences, labels):
        plt.figure(figsize=(10, 8))
        lengths = [len(seq) for seq in sequences]
        pos_lengths = [l for l, label in zip(lengths, labels) if label == 1]
        neg_lengths = [l for l, label in zip(lengths, labels) if label == 0]

        sns.kdeplot(data=pos_lengths, label="Positive", color="red")
        sns.kdeplot(data=neg_lengths, label="Negative", color="blue")
        plt.title("Sequence Length Distribution")
        plt.xlabel("Length")
        plt.legend()
        self._save_plot("sequence_lengths")

    def _create_stats_plots(self, stats):
        # Plot similarity metrics
        self._create_similarity_metrics_plot(stats)
        # Plot property correlations
        if "property_correlations" in stats:
            self._create_correlation_plot(stats["property_correlations"])

    def _create_similarity_metrics_plot(self, stats):
        plt.figure(figsize=(12, 6))

        # Get values directly from similarity_analysis
        similarity_analysis = stats.get("similarity_analysis", {})

        metrics = {
            f"Within Positive\n({int(similarity_analysis.get('pos_samples', 0))} samples)": similarity_analysis.get(
                "within_positive", 0.0
            ),
            f"Within Negative\n({int(similarity_analysis.get('neg_samples', 0))} samples)": similarity_analysis.get(
                "within_negative", 0.0
            ),
            "Between Groups": similarity_analysis.get("between_groups", 0.0),
        }

        std_values = {
            f"Within Positive\n({int(similarity_analysis.get('pos_samples', 0))} samples)": similarity_analysis.get(
                "within_positive_std", 0.0
            ),
            f"Within Negative\n({int(similarity_analysis.get('neg_samples', 0))} samples)": similarity_analysis.get(
                "within_negative_std", 0.0
            ),
            "Between Groups": similarity_analysis.get("between_groups_std", 0.0),
        }

        y_pos = np.arange(len(metrics))
        values = list(metrics.values())
        stds = list(std_values.values())

        # Create bars with error bars
        bars = plt.bar(y_pos, values, align="center", yerr=stds, capsize=5)
        plt.xticks(y_pos, list(metrics.keys()), rotation=45, ha="right")
        plt.title("Similarity Metrics")
        plt.ylabel("Similarity Score")

        # Add value labels on bars with better formatting
        for bar in bars:
            height = bar.get_height()
            plt.text(
                bar.get_x() + bar.get_width() / 2.0,
                height,
                f"{height:.2f}",
                ha="center",
                va="bottom",
            )

        # Adjust layout to prevent label cutoff
        plt.tight_layout()

        # Add grid for better readability
        plt.grid(True, axis="y", linestyle="--", alpha=0.7)

        # Set y-axis limits to show full range of data with padding
        max_height = max(v + s for v, s in zip(values, stds))
        plt.ylim(
            0, max_height * 1.2
        )  # Add 20% padding above highest point including error bars

        self._save_plot("similarity_metrics")

    def _create_correlation_plot(self, correlations):
        plt.figure(figsize=(10, 8))
        properties = list(correlations.keys())
        correlation_values = [data["correlation"] for data in correlations.values()]
        p_values = [data["p_value"] for data in correlations.values()]

        y_pos = np.arange(len(properties))
        bars = plt.bar(y_pos, correlation_values, align="center")
        plt.xticks(y_pos, properties, rotation=45, ha="right")
        plt.title("Property Correlations with Labels")
        plt.ylabel("Correlation Coefficient")

        # Add significance indicators
        for i, (bar, p_val) in enumerate(zip(bars, p_values)):
            height = bar.get_height()
            significance = (
                "***"
                if p_val < 0.001
                else "**" if p_val < 0.01 else "*" if p_val < 0.05 else "ns"
            )
            plt.text(
                bar.get_x() + bar.get_width() / 2.0,
                height,
                f"{height:.2f}\n{significance}",
                ha="center",
                va="bottom",
            )

        plt.grid(True, axis="y", linestyle="--", alpha=0.7)
        self._save_plot("property_correlations")

    def _create_aa_composition_plot(self, sequences):
        """Create amino acid composition visualization."""
        aa_analysis = analyze_aa_composition(sequences)

        # AA frequency plot
        plt.figure(figsize=(15, 10))

        # Plot 1: AA frequencies
        plt.subplot(2, 1, 1)
        aa_freq = aa_analysis["aa_frequencies"]
        sorted_aa = sorted(aa_freq.items(), key=lambda x: x[1], reverse=True)

        x = [aa for aa, _ in sorted_aa]
        y = [freq for _, freq in sorted_aa]

        plt.bar(x, y)
        plt.title("Amino Acid Frequencies")
        plt.ylabel("Frequency")

        # Plot 2: Property percentages
        plt.subplot(2, 1, 2)
        props = aa_analysis["property_percentages"]
        plt.bar(props.keys(), props.values())
        plt.title("Amino Acid Property Distribution")
        plt.ylabel("Percentage")

        plt.tight_layout()
        self._save_plot("aa_composition")
