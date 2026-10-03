#!/bin/zsh
# Catherine (Xbox 360) for macOS: one-step setup.
#
# Builds Catherine.app from YOUR OWN copy of the game:
#   1. checks your Mac and installs the build tools (with your OK)
#   2. downloads the ReXGlue SDK and applies this project's fixes
#   3. checks that your game files are the supported version
#   4. recompiles the game for Apple Silicon and builds Catherine.app
#
# Usage:  ./setup.sh [--game /path/to/game/folder] [--yes] [--no-install]

emulate -L zsh
setopt pipefail

REPO=${0:A:h}
SDK_DIR=${REXSDK_DIR:-${REPO:h}/tools/rexglue-sdk}
SDK_URL=https://github.com/rexglue/rexglue-sdk.git
SDK_COMMIT=c94f5eb
XEX_SHA256=571ae5e03bf7385d5cb4ccc244d5fcf4b1bb895fb2d8f373bd562ab59e8f9647
GAME_LINK=${REPO:h}/retail-game
LOG=$REPO/out/setup.log

GAME_ARG="" ; ASSUME_YES=0 ; INSTALL=1
while (( $# )); do
  case $1 in
    --game) GAME_ARG=$2; shift 2 ;;
    --yes|-y) ASSUME_YES=1; shift ;;
    --no-install) INSTALL=0; shift ;;
    -h|--help) sed -n '2,12p' $0; exit 0 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

# ------------------------------------------------------------------ style --
PINK=$'\e[38;2;253;1;106m'; CRIMSON=$'\e[38;2;141;0;55m'; BLUSH=$'\e[38;2;254;203;239m'
PAPER=$'\e[38;2;245;249;252m'; GREY=$'\e[38;2;140;140;150m'; BOLD=$'\e[1m'; R=$'\e[0m'
WIRE="${GREY}──x────x────x────x────x────x────x────x────x────x────x────x──${R}"

banner() {
  print -r -- ""
  print -r -- "  $WIRE"
  print -r -- ""
  print -r -- "      ${PINK}${BOLD}✦  C  A  T  H  E  R  I  N  E  ✦${R}"
  print -r -- "      ${BLUSH}macOS recompilation  ·  setup${R}"
  print -r -- ""
  print -r -- "  $WIRE"
}
STEP=0
step()  { STEP=$((STEP+1)); print -r -- ""; print -r -- "${PINK}${BOLD} ✦ ${STEP}. $1${R}"; }
ok()    { print -r -- "   ${PINK}✔${R} ${PAPER}$1${R}"; }
info()  { print -r -- "   ${BLUSH}$1${R}"; }
warn()  { print -r -- "   ${CRIMSON}${BOLD}!${R} ${PAPER}$1${R}"; }
die()   { print -r -- ""; print -r -- "   ${CRIMSON}${BOLD}✘ $1${R}"; [[ -n $2 ]] && print -r -- "   ${BLUSH}$2${R}"; print -r -- "   ${GREY}Full log: $LOG${R}"; exit 1; }
ask()   { # ask "question" -> 0 for yes
  (( ASSUME_YES )) && return 0
  print -n -- "   ${PINK}?${R} ${PAPER}$1${R} ${GREY}[Y/n]${R} "
  read -r reply; [[ -z $reply || $reply == [Yy]* ]]
}
run_logged() { # run_logged "label" cmd... (shows a spinner, output goes to the log)
  local label=$1; shift
  print -n -- "   ${BLUSH}$label${R} "
  ( "$@" ) >> $LOG 2>&1 &
  local pid=$! frames=('⠋' '⠙' '⠹' '⠸' '⠼' '⠴' '⠦' '⠧' '⠇' '⠏') i=0 start=$SECONDS
  while [[ -t 1 ]] && kill -0 $pid 2>/dev/null; do
    print -n -- "\r   ${BLUSH}$label${R} ${PINK}${frames[i % 10 + 1]}${R} ${GREY}$((SECONDS-start))s${R}"
    i=$((i+1)); sleep 0.2
  done
  wait $pid; local rc=$?
  if (( rc == 0 )); then print -- "\r   ${PINK}✔${R} ${PAPER}$label${R} ${GREY}($((SECONDS-start))s)${R}          "
  else print -- "\r   ${CRIMSON}✘${R} ${PAPER}$label${R}                    "; fi
  return $rc
}

mkdir -p $REPO/out; : > $LOG
banner

# ------------------------------------------------------------ 1. your Mac --
step "Checking your Mac"
[[ $(uname -s) == Darwin ]] || die "This project builds for macOS only."
[[ $(uname -m) == arm64 ]] || die "An Apple Silicon Mac (M1 or newer) is required."
ok "Apple Silicon, macOS $(sw_vers -productVersion)"
avail_gb=$(df -g $REPO | awk 'NR==2 {print $4}')
(( avail_gb >= 8 )) || warn "Only ${avail_gb} GB free; the build needs about 5 GB."

if ! xcode-select -p >/dev/null 2>&1; then
  warn "Apple's Command Line Tools (compiler) are missing."
  xcode-select --install 2>/dev/null
  die "Finish the Command Line Tools install window, then run ./setup.sh again."
fi
ok "Command Line Tools"

if ! command -v brew >/dev/null 2>&1; then
  die "Homebrew is needed to install the build tools." \
      "Install it from https://brew.sh, then run ./setup.sh again."
fi
ok "Homebrew"
missing=()
for t in cmake ninja git; do command -v $t >/dev/null 2>&1 || missing+=$t; done
if (( ${#missing} )); then
  ask "Install ${missing[*]} with Homebrew?" || die "Build tools are required."
  run_logged "Installing ${missing[*]}" brew install ${missing[@]} || die "Homebrew install failed."
fi
ok "cmake, ninja, git"

# ------------------------------------------------------------- 2. the SDK --
step "ReXGlue SDK"
if [[ ! -d $SDK_DIR/.git ]]; then
  info "Downloading the SDK to ${SDK_DIR}"
  mkdir -p ${SDK_DIR:h}
  run_logged "Cloning ReXGlue SDK" git clone $SDK_URL $SDK_DIR || die "Couldn't download the SDK."
fi
cur=$(git -C $SDK_DIR rev-parse --short=7 HEAD 2>/dev/null)
if [[ $cur != $SDK_COMMIT* ]]; then
  if [[ -n $(git -C $SDK_DIR status --porcelain --untracked-files=no) ]]; then
    die "The SDK at $SDK_DIR has local changes on a different version ($cur)." \
        "Move it away or set REXSDK_DIR to another folder, then run ./setup.sh again."
  fi
  run_logged "Switching SDK to $SDK_COMMIT" git -C $SDK_DIR checkout -q $SDK_COMMIT || die "Couldn't switch the SDK version."
fi
run_logged "Fetching SDK components" git -C $SDK_DIR submodule update --init --recursive || die "Couldn't fetch SDK components."
run_logged "Applying Catherine fixes to the SDK" env REXSDK_DIR=$SDK_DIR $REPO/scripts/apply_sdk_patches.sh \
  || die "A fix didn't apply cleanly." "If you changed the SDK yourself, reset it with: git -C $SDK_DIR checkout ."
ok "SDK $SDK_COMMIT with $(ls $REPO/patches/*.patch | wc -l | tr -d ' ') fixes"

# ------------------------------------------------------- 3. game files --
step "Your game files"
info "You need your own extracted ${PAPER}Catherine (USA)${BLUSH} Xbox 360 game folder (the one with default.xex)."
game=$GAME_ARG
if [[ -z $game && -e $GAME_LINK/default.xex ]]; then game=${GAME_LINK:A}; fi
while true; do
  if [[ -z $game ]]; then
    if (( ASSUME_YES )); then die "No game folder given (use --game /path)."; fi
    info "A folder window is opening..."
    game=$(osascript -e 'POSIX path of (choose folder with prompt "Choose your Catherine (USA) game folder - it contains default.xex")' 2>/dev/null)
    [[ -z $game ]] && die "No folder chosen."
  fi
  game=${game%/}
  if [[ ! -f $game/default.xex ]]; then
    warn "No default.xex in: $game"; game=""; (( ASSUME_YES )) && die "Wrong folder."; continue
  fi
  print -n -- "   ${BLUSH}Checking the game version${R} "
  h=$(shasum -a 256 "$game/default.xex" | cut -d' ' -f1)
  if [[ $h == $XEX_SHA256 ]]; then print -- "\r   ${PINK}✔${R} ${PAPER}Catherine (USA) verified${R}            "; break; fi
  print -- ""
  warn "This default.xex is a different version (fingerprint ${h[1,16]}...)."
  info "Only the retail Catherine (USA) release is supported."
  game=""; (( ASSUME_YES )) && die "Unsupported game version."
done

# Files in Downloads/Documents/Desktop make macOS ask for permission again
# after every rebuild, so offer to copy them next to the project.
case ${game:A} in
  $HOME/Downloads/*|$HOME/Documents/*|$HOME/Desktop/*)
    warn "Your game files are in a folder macOS protects (${game:A:h:t}...)."
    info "Catherine would ask for permission to read it every time it is rebuilt."
    if ask "Copy the game files into ${REPO:h}/game-files (about $(du -sh "$game" | cut -f1))?"; then
      dest=${REPO:h}/game-files
      mkdir -p $dest
      run_logged "Copying game files" cp -R "$game/." "$dest/" || die "Copy failed."
      [[ $(shasum -a 256 $dest/default.xex | cut -d' ' -f1) == $XEX_SHA256 ]] || die "The copy doesn't match the original."
      game=$dest
      ok "Copied (your original is untouched)"
    fi ;;
esac
if [[ -L $GAME_LINK || ! -e $GAME_LINK ]]; then
  ln -sfn "$game" $GAME_LINK
elif [[ ${GAME_LINK:A} != ${game:A} ]]; then
  die "$GAME_LINK already exists and is not a link." "Rename it, then run ./setup.sh again."
fi
ok "Game folder: $game"

# ----------------------------------------------------------- 4. build --
step "Building Catherine"
info "Recompiling the game for Apple Silicon. The first build takes a while (about 10 min on an M4; longer on older Macs)."
caffeinate -i true 2>/dev/null
run_logged "Recompiling and building" env REXSDK_DIR=$SDK_DIR caffeinate -i $REPO/scripts/build.sh \
  || die "The build failed." "The last lines of $LOG usually say why."
[[ -d $REPO/out/Catherine.app ]] || die "Catherine.app wasn't created."
ok "Catherine.app built"

# Remember the game folder for the app.
cfg_dir="$HOME/Library/Application Support/Catherine"; mkdir -p $cfg_dir
cfg="$cfg_dir/catherine.toml"
if [[ -f $cfg ]] && grep -q '^game_data_root' $cfg; then
  sed -i '' "s|^game_data_root = .*|game_data_root = \"$game\"|" $cfg
else
  [[ -f $cfg ]] || print -- "# Catherine settings" > $cfg
  print -- "game_data_root = \"$game\"" >> $cfg
fi

# --------------------------------------------------------- 5. install --
app=$REPO/out/Catherine.app
if (( INSTALL )) && ask "Copy Catherine.app to your Applications folder?"; then
  rm -rf /Applications/Catherine.app && cp -R $app /Applications/ && app=/Applications/Catherine.app \
    && ok "Installed to /Applications" || warn "Couldn't copy to /Applications; use $app instead."
fi

print -r -- ""
print -r -- "  $WIRE"
print -r -- "   ${PINK}${BOLD}All set.${R} ${PAPER}Open ${BOLD}Catherine${R}${PAPER} from ${app:h}.${R}"
print -r -- "   ${BLUSH}In game: Esc (or Back + Start) opens the settings menu.${R}"
print -r -- ""
