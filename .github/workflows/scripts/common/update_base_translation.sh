#!/bin/bash

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")
source "$SCRIPTDIR/../linux/functions.sh"

set -e

# While we use custom Qt builds for our releases, the Qt6 package will be good enough
# for just updating translations. Saves building it for this action alone.
retry_command sudo apt-get -y install qt6-l10n-tools

PATH=/usr/lib/qt6/bin:$PATH "$SCRIPTDIR/../../../../pcsx2-qt/Translations/update_en_translation.sh"
