import json
from datetime import datetime
import os


def format_percentage(value):
    return f"{value * 100:.1f}%"


def format_number(value):
    return f"{value:,.2f}"


def generate_readme(json_path, output_path):
    with open(json_path, "r") as f:
        data = json.load(f)

    readme_content = f"""# Sequence Analysis Results

## Dataset Overview
- Total sequences: {data['dataset']['sequences']:,}
- Positive samples: {data['dataset']['positive_samples']:,} ({data['dataset']['positive_samples']/data['dataset']['sequences']*100:.1f}%)
- Negative samples: {data['dataset']['negative_samples']:,} ({data['dataset']['negative_samples']/data['dataset']['sequences']*100:.1f}%)
- Dataset: [{os.path.basename(data['dataset']['path'])}](../datasets/{os.path.basename(data['dataset']['path'])})

## Clustering Analysis

### K-means Clustering
- Number of clusters: {len(data['clustering_results']['kmeans']['cluster_sizes'])}
- Cluster sizes: {' and '.join(str(size) for size in data['clustering_results']['kmeans']['cluster_sizes'])} sequences
- Inertia: {format_number(data['clustering_results']['kmeans']['inertia'])}

### DBSCAN
- Number of clusters: {data['clustering_results']['dbscan']['n_clusters']}
- Noise points: {data['clustering_results']['dbscan']['n_noise']}
- {'No significant clusters found' if not data['clustering_results']['dbscan']['cluster_sizes'] else f"Cluster sizes: {', '.join(map(str, data['clustering_results']['dbscan']['cluster_sizes']))}"}

### Spectral Clustering
- Number of clusters: {len(data['clustering_results']['spectral']['cluster_sizes'])}
- Cluster sizes: {' and '.join(str(size) for size in data['clustering_results']['spectral']['cluster_sizes'])} sequences

![Clustering Results](clusters.png)

## Sequence Similarity Analysis
- Total comparisons: {data['similarity_stats']['total_comparisons']:,}
- Similarity range: {data['similarity_stats']['min_similarity']} to {data['similarity_stats']['max_similarity']}
- Mean similarity: {format_number(data['similarity_stats']['mean_similarity'])}
- Quartiles [Q1, Q2, Q3]: {data['similarity_analysis']['similarity_quartiles']}

### Group-wise Similarity
- Within positive samples: {format_number(data['similarity_analysis']['within_positive'])} ± {format_number(data['similarity_analysis']['within_positive_std'])}
- Within negative samples: {format_number(data['similarity_analysis']['within_negative'])} ± {format_number(data['similarity_analysis']['within_negative_std'])}
- Between groups: {format_number(data['similarity_analysis']['between_groups'])} ± {format_number(data['similarity_analysis']['between_groups_std'])}

![Similarity Distribution](similarity_distribution.png)
![Similarity Heatmap](similarity_heatmap.png)

## Machine Learning Performance
### Random Forest
- Mean accuracy: {format_percentage(data['ml_analysis']['random_forest']['mean_accuracy'])} ± {format_percentage(data['ml_analysis']['random_forest']['std_accuracy'])}

### Support Vector Machine
- Mean accuracy: {format_percentage(data['ml_analysis']['svm']['mean_accuracy'])} ± {format_percentage(data['ml_analysis']['svm']['std_accuracy'])}

## Sequence Properties
### Property Correlations with Similarity
| Property | Correlation | P-value |
|----------|------------|---------|
{chr(10).join(f"| {prop.replace('_', ' ').title()} | {corr['correlation']:.3f} | {corr['p_value']:.2e} |" for prop, corr in data['similarity_analysis']['property_correlations'].items() if corr['correlation'] > 0.05)}

![Sequence Properties](similarity_metrics.png)

## Additional Visualizations
### Sequence Composition
![Amino Acid Composition](aa_composition.png)

### Sequence Length Distribution
![Sequence Lengths](sequence_lengths.png)

### Principal Component Analysis
![PCA Plot](pca_plot.png)

---
*Analysis timestamp: {datetime.fromisoformat(data['analysis_timestamp']).strftime('%Y-%m-%d %H:%M:%S')}*
"""

    with open(output_path, "w") as f:
        f.write(readme_content)


if __name__ == "__main__":
    json_path = "analysis_results.json"
    output_path = "README.md"
    generate_readme(json_path, output_path)
