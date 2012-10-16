#!/bin/sh

if [ -z "$GCM_SENDER_ID" ]; then
    echo "GCM_SENDER_ID not in env"
    exit 1
fi

cat <<EOF
// Auto-generated: DO NOT CHECK THIS IN until questions about
// obscuring the id are cleared up
package org.eehouse.android.xw4gcm;

public class GCMConsts {
    public static final String SENDER_ID = "${GCM_SENDER_ID}";
}
EOF
