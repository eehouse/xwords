#!/bin/sh

set -e -u

GCM_SENDER_ID=${GCM_SENDER_ID:-""}

if [ -z "$GCM_SENDER_ID" ]; then
    echo "GCM_SENDER_ID empty; GCM use will be disabled" >&2
fi

cat <<EOF
// Auto-generated: DO NOT CHECK THIS IN until questions about
// obscuring the id are cleared up
package org.eehouse.android.xw4;

public class GCMConsts {
    public static final String SENDER_ID = "${GCM_SENDER_ID}";
}
EOF
