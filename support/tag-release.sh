#!/bin/bash
# tag-release.sh - Create a new release tag

set -e

# Check if version argument provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 v1.2.3"
    exit 1
fi

VERSION="$1"

# Validate version format
if ! [[ $VERSION =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Version must be in format vX.Y.Z"
    exit 1
fi

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo "Error: Uncommitted changes found. Please commit first."
    exit 1
fi

# Create tag with release message including itch.io download link
echo "Creating tag $VERSION..."
RELEASE_MESSAGE="Release $VERSION

Download minicoder binaries at: https://andrewchambers.itch.io/minicoder"

git tag -a "$VERSION" -m "$RELEASE_MESSAGE"

echo "Tag $VERSION created successfully"
echo "To push: git push origin $VERSION"