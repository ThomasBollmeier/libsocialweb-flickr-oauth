#!/bin/sh

# This is an example program to be used with request-key and the kernel-keyring
# Add a line like this to /etc/request-key.conf
#
# create	user	libsocialweb:*	*		/path/to/libsocialweb/examples/request-api-key.sh %k %d %c %S
#

KEY="THEKEYHERE
THESECRETHERE"

echo "requesting key with args: $@" >> /tmp/request-api-key.log
keyctl instantiate $1 "$KEY" $4 || exit 1
