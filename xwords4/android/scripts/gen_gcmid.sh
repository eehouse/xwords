#!/bin/sh

set -e -u

GCM_SENDER_ID=${GCM_SENDER_ID:-""}
CRITTERCISM_APP_ID=${CRITTERCISM_APP_ID:-""}

if [ -z "$GCM_SENDER_ID" ]; then
    echo "GCM_SENDER_ID empty; GCM use will be disabled" >&2
fi
if [ -z "$CRITTERCISM_APP_ID" ]; then
    echo "CRITTERCISM_APP_ID empty; Crittercism will not be enabled" >&2
fi

PKG=$1

cat <<EOF
// Auto-generated: DO NOT CHECK THIS IN until questions about
// obscuring various ids are cleared up. For now they're not meant 
// to be committed to public repos.

package org.eehouse.android.$PKG;

public class GCMConsts {
    public static final String SENDER_ID = "${GCM_SENDER_ID}";
    public static final String CRITTERCISM_APP_ID  = "${CRITTERCISM_APP_ID}";
}
EOF
