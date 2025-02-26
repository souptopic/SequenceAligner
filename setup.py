from setuptools import setup, find_packages

setup(
    name="SequenceAligner",
    version="2.0.0",
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    install_requires=[
        "numpy",
        "scipy",
        "scikit-learn",
        "biopython",
        "parasail",
        "polars",
        "tqdm",
        "matplotlib",
        "seaborn",
    ],
    python_requires=">=3.8",
    author="JakovDev",
    description="A tool for sequence alignment and analysis",
)
