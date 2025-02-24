from sklearn.model_selection import cross_val_score
from sklearn.ensemble import RandomForestClassifier
from sklearn.svm import SVC
import numpy as np
from typing import Dict


def analyze_predictive_power(feature_matrix: np.ndarray, labels: np.ndarray) -> Dict:
    """Analyze predictive power of features."""
    models = {
        "random_forest": RandomForestClassifier(n_estimators=100, random_state=42),
        "svm": SVC(kernel="rbf", random_state=42),
    }

    results = {}
    for name, model in models.items():
        scores = cross_val_score(model, feature_matrix, labels, cv=5)
        results[name] = {
            "mean_accuracy": float(scores.mean()),
            "std_accuracy": float(scores.std()),
        }

    return results
