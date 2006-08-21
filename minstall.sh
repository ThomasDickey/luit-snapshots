#!/bin/sh
# $XTermId: minstall.sh,v 1.1 2006/08/20 20:28:23 tom Exp $
#
# Install manpages, substituting a reasonable section value since X doesn't use
# constants...
#
# Parameters:
#	$1 = program to invoke as "install"
#	$2 = manpage to install
#	$3 = final installed-path
#	$4 = top-level application directory

MINSTALL="$1"
OLD_FILE="$2"
END_FILE="$3"
ROOT_DIR="$4"

suffix=`echo "$END_FILE" | sed -e 's%^[^.]*.%%'`
NEW_FILE=temp$$

sed	-e 's%__vendorversion__%"X Window System"%' \
	-e "s%__projectroot__%$ROOT_DIR%" \
	-e "s%__mansuffix__%$suffix%g" \
	$OLD_FILE >$NEW_FILE

echo "$MINSTALL $OLD_FILE $END_FILE"
eval "$MINSTALL $NEW_FILE $END_FILE"

rm -f $NEW_FILE
