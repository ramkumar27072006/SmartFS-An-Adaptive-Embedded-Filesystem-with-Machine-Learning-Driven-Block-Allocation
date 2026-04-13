"""
SmartFS TinyML — Model Training Script

Trains three model candidates:
  1. Decision Tree (max_depth=3)
  2. Logistic Regression
  3. Tiny Neural Network (Dense 8 relu -> Dense 3 softmax)

Saves:
  - dtree.pkl
  - logreg.pkl
  - tiny_model.keras

Also exports the Decision Tree as C++ if-else for embedded deployment.
"""

import os
import sys
import csv
import numpy as np
import pickle


def load_dataset(csv_path):
    """Load dataset from CSV."""
    X, y = [], []
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            X.append([float(row["avg_write_size"]), float(row["writes_per_sec"])])
            y.append(int(row["alloc_mode"]))
    return np.array(X), np.array(y)


def train_decision_tree(X_train, y_train, X_test, y_test, output_dir):
    """Train Decision Tree classifier."""
    from sklearn.tree import DecisionTreeClassifier, export_text

    clf = DecisionTreeClassifier(max_depth=3, random_state=42)
    clf.fit(X_train, y_train)

    acc = clf.score(X_test, y_test)
    print(f"Decision Tree accuracy: {acc:.4f}")

    # Save model
    model_path = os.path.join(output_dir, "dtree.pkl")
    with open(model_path, "wb") as f:
        pickle.dump(clf, f)
    print(f"  Saved: {model_path}")

    # Export tree rules
    rules = export_text(clf, feature_names=["avg_write_size", "writes_per_sec"])
    print("\nDecision Tree Rules:")
    print(rules)

    # Export as C++ if-else
    export_tree_to_cpp(clf, output_dir)

    return clf


def export_tree_to_cpp(clf, output_dir):
    """Export decision tree as C++ if-else code."""
    from sklearn.tree import _tree

    tree = clf.tree_
    feature_names = ["avg_write_size", "writes_per_sec"]

    lines = []
    lines.append("// Auto-generated from trained Decision Tree")
    lines.append("// Paste this into ml_predict.cpp if you retrain")
    lines.append("")
    lines.append("int ml_predict(float avg_write_size, float writes_per_sec)")
    lines.append("{")

    def recurse(node, indent):
        prefix = "    " * indent
        if tree.feature[node] != _tree.TREE_UNDEFINED:
            fname = feature_names[tree.feature[node]]
            threshold = tree.threshold[node]
            lines.append(f"{prefix}if ({fname} <= {threshold:.1f}f)")
            lines.append(f"{prefix}{{")
            recurse(tree.children_left[node], indent + 1)
            lines.append(f"{prefix}}}")
            lines.append(f"{prefix}else")
            lines.append(f"{prefix}{{")
            recurse(tree.children_right[node], indent + 1)
            lines.append(f"{prefix}}}")
        else:
            class_idx = int(np.argmax(tree.value[node]))
            mode_names = {0: "SEQUENTIAL", 1: "RANDOM", 2: "WEAR_AWARE"}
            lines.append(f"{prefix}return {class_idx}; // {mode_names.get(class_idx, '?')}")

    recurse(0, 1)
    lines.append("}")

    cpp_path = os.path.join(output_dir, "ml_predict_generated.cpp")
    with open(cpp_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"\n  C++ export: {cpp_path}")


def train_logistic_regression(X_train, y_train, X_test, y_test, output_dir):
    """Train Logistic Regression classifier."""
    from sklearn.linear_model import LogisticRegression
    from sklearn.preprocessing import StandardScaler

    scaler = StandardScaler()
    X_train_s = scaler.fit_transform(X_train)
    X_test_s = scaler.transform(X_test)

    clf = LogisticRegression(max_iter=1000, random_state=42)
    clf.fit(X_train_s, y_train)

    acc = clf.score(X_test_s, y_test)
    print(f"\nLogistic Regression accuracy: {acc:.4f}")

    model_path = os.path.join(output_dir, "logreg.pkl")
    with open(model_path, "wb") as f:
        pickle.dump({"model": clf, "scaler": scaler}, f)
    print(f"  Saved: {model_path}")

    return clf


def train_neural_network(X_train, y_train, X_test, y_test, output_dir):
    """Train tiny neural network."""
    try:
        os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"
        import tensorflow as tf
        from tensorflow import keras

        # Normalize
        mean = X_train.mean(axis=0)
        std = X_train.std(axis=0)
        X_train_n = (X_train - mean) / std
        X_test_n = (X_test - mean) / std

        model = keras.Sequential([
            keras.layers.Dense(8, activation="relu", input_shape=(2,)),
            keras.layers.Dense(3, activation="softmax"),
        ])

        model.compile(
            optimizer="adam",
            loss="sparse_categorical_crossentropy",
            metrics=["accuracy"],
        )

        model.fit(X_train_n, y_train, epochs=20, batch_size=64, verbose=0)

        loss, acc = model.evaluate(X_test_n, y_test, verbose=0)
        print(f"\nTiny Neural Network accuracy: {acc:.4f}")

        model_path = os.path.join(output_dir, "tiny_model.keras")
        model.save(model_path)
        print(f"  Saved: {model_path}")

        # Save normalization params
        norm_path = os.path.join(output_dir, "nn_norm_params.pkl")
        with open(norm_path, "wb") as f:
            pickle.dump({"mean": mean, "std": std}, f)

        return model

    except ImportError:
        print("\nTiny Neural Network: SKIPPED (TensorFlow not installed)")
        return None


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    csv_path = os.path.join(script_dir, "smartfs_dataset.csv")

    if not os.path.exists(csv_path):
        print(f"Dataset not found: {csv_path}")
        print("Run generate_dataset.py first.")
        sys.exit(1)

    print("Loading dataset...")
    X, y = load_dataset(csv_path)
    print(f"Loaded {len(X)} samples")

    # Train/test split (80/20)
    split = int(0.8 * len(X))
    indices = np.arange(len(X))
    np.random.seed(42)
    np.random.shuffle(indices)
    X, y = X[indices], y[indices]

    X_train, X_test = X[:split], X[split:]
    y_train, y_test = y[:split], y[split:]

    print(f"Train: {len(X_train)}, Test: {len(X_test)}\n")

    # Train all three models
    train_decision_tree(X_train, y_train, X_test, y_test, script_dir)
    train_logistic_regression(X_train, y_train, X_test, y_test, script_dir)
    train_neural_network(X_train, y_train, X_test, y_test, script_dir)

    print("\n=== Training Complete ===")


if __name__ == "__main__":
    main()
