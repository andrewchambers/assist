#!/bin/sh
# Convert man pages to HTML with proper linking between pages

set -e

usage() {
    echo "Usage: $0 <input-man-page> <output-html-file>"
    echo "Example: $0 doc/minicoder.1 www/minicoder.1.html"
    exit 1
}

if [ $# -ne 2 ]; then
    usage
fi

INPUT_MAN="$1"
OUTPUT_HTML="$2"
TMP_FILE="${OUTPUT_HTML}.tmp"

# Ensure output directory exists
mkdir -p "$(dirname "$OUTPUT_HTML")"

# Convert to HTML using mandoc
mandoc -T html -O style=style.css "$INPUT_MAN" > "$TMP_FILE"

# Apply link transformations based on which man page we're processing
if echo "$INPUT_MAN" | grep -q "\.1"; then
    # For section 1 man pages
    sed -e 's|<b>minicoder-model-config</b>(5)|<a href="minicoder-model-config.5.html"><b>minicoder-model-config</b>(5)</a>|g' \
        -e 's|<b>assist</b>(1)|<a href="assist.1.html"><b>assist</b>(1)</a>|g' \
        "$TMP_FILE" > "$OUTPUT_HTML"
else
    # For section 5 man pages
    sed -e 's|<b>assist</b>(1)|<a href="assist.1.html"><b>assist</b>(1)</a>|g' \
        -e 's|<b>minicoder-model-config</b>(5)|<a href="minicoder-model-config.5.html"><b>minicoder-model-config</b>(5)</a>|g' \
        -e 's|<br/>||g' "$TMP_FILE" | \
    perl -0pe 's|(<pre>[^<]*</pre>)|my $pre=$1; $pre=~s/\n\n/\n/g; $pre|ge' > "$OUTPUT_HTML"
fi

rm -f "$TMP_FILE"
echo "Generated: $OUTPUT_HTML"