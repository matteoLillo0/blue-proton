#!/usr/bin/env bash
# Scarica in data/videos/ video VERI di bovini, con licenza libera (Wikimedia Commons),
# per provare detector e tracker finche' non abbiamo riprese nostre.
#
#   cow_grid.webm  "Cow crosses cattle grid"          CC BY 3.0  - una mucca attraversa la scena
#   jersey.webm    "Jersey dairy cattle near Biei"    CC BY 4.0 (Nesnad) - mandria al pascolo
# Attribuzione completa: pagine https://commons.wikimedia.org/wiki/File:<nome originale>
set -euo pipefail

dir="$(cd "$(dirname "$0")/.." && pwd)/data/videos"
mkdir -p "$dir"
base="https://upload.wikimedia.org/wikipedia/commons"

get() {
    if [[ -s "$dir/$1" ]]; then
        echo "gia' presente: $1"
    else
        curl -fsSL --retry 3 -A "blue-proton-test-download/1.0" -o "$dir/$1" "$base/$2"
        echo "scaricato: $1"
    fi
}

get cow_grid.webm "1/15/Cow_crosses_cattle_grid.webm"
get jersey.webm "2/2b/Jersey_dairy_cattle_nearbieihokkaido-2022-08-11.webm"
