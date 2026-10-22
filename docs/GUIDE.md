# EUpdate user guide

EUpdate brings the latest releases of GitHub repositories straight to
your Atari Falcon or TT. Find repositories by name, or find an owner (a
person or an organisation) and look through their repositories; follow
the ones you want, each into a folder of your choice. EUpdate shows
which release you have, which is the latest, and whether you are
current. Update one, several or all of them with a click, or let
EUpdate check by itself when it starts and every few hours while it
runs.

## What you need

- A Falcon or a TT (a 68020 or better): EUpdate talks to GitHub over
  TLS itself, and an 8 MHz ST would take minutes over each handshake.
  On a Falcon the DSP does the handshake's sums
- TOS 4 (or TOS 3 on the TT), EmuTOS, MagiC or MiNT, and a screen of
  640×400 or more (the Falcon's 640×480 shows more of each list)
- About 500 KB free when EUpdate starts, and room for the biggest file
  you download: a release's file is held in memory until it is whole,
  then written in one go (TT RAM when there is some)
- A TCP/IP stack: **STinG** on plain TOS, or **MiNTnet** under FreeMiNT
- Under TOS, Atari's `FOLDR100.PRG` first in your `AUTO` folder,
  renamed for more room (`FOLDR400.PRG`): TOS keeps an entry in a small
  pool for every folder a program has looked into, and halts with "OUT
  OF INTERNAL MEMORY" when it runs out
- With STinG, its Delay in `STING.CPX` set to 10 on a Falcon or TT:
  STinG looks at the network card that often, and downloads are about
  twice as fast as with its default

## Installing

The files are in the latest release on EUpdate's GitHub page
(github.com/whomper/EUpdate/releases), each on its own and all together
in `EUPDATE-x.y.zip`. Copy them into a folder of their own, on any
drive:

```
EUPDATE.PRG   the program
EUPDATE.HYP   this guide, for EHypView or ST-Guide (the Help key opens it)
CACERT.PEM    the root certificates GitHub's are checked against
ROOTS.DAT     the same, decoded ahead of time
EUPDATE.INF   settings and the repositories followed (a note above each)
EUPDATE.KEY   the key your GitHub token is encrypted with
EUPDATE.LOG   protocol log, when switched on
CRASH.LOG     what happened, if EUpdate ever crashes
SEED.DAT      randomness kept for the next start
```

You copy the first four; EUpdate makes the others when it needs them.

EUpdate can keep itself up to date: follow `whomper/EUpdate` into
EUpdate's own folder. The program running is in memory, so its file
can be written over; the new one starts next time.

## The window

One window, two lists, and a toolbar:

- **Check** asks GitHub for the latest release of each repository
  followed (of those selected, when some are).
- **Update** downloads the newer releases: of the repositories selected,
  or, with none selected, of all that have one. It is drawn in orange
  when there is something to download, and the toolbar says how much.
- **Find...** looks on GitHub.
- **Stop** (or Esc) ends what EUpdate is doing.

**Following**, the upper list, has a row for each repository followed:
its name, the release you have, the latest GitHub had at the last check,
and what that means:

| State | |
|---|---|
| Current | you have the latest release (teal) |
| New release | there is a newer one (orange): Update downloads it |
| Not downloaded | it has releases, and you have none of them yet |
| Not checked | EUpdate hasn't asked yet |
| No release | the repository has published no release |
| Failed | the last check or update went wrong: the row's tip says why |

The folder its files go to is in the last column, when there is room.
Rest the mouse on a row for when the release was published and when you
downloaded yours.

Click a row to select it, Shift+click to add another, ^A to select them
all. Double-click a row (or Return) for **Get info**. Right-click a row,
or press Insert, for its menu.

**Find**, the lower list, shows what a search found. A tick marks the
repositories you follow already.

The divider between the lists can be dragged; the window remembers its
place and size, and the divider, in `EUPDATE.INF`.

## Finding repositories

**File > Find on GitHub** (^F) asks what to look for, and where:

- **Repositories with it in their names**: GitHub's best matches, the
  most starred first, with their stars, when they last changed and what
  they are. GitHub gives the first 30.
- **Owners**: people and organisations whose login or name has it.
  Double-click one (or Return) for their repositories, the last changed
  first; Backspace goes back to the owners.

Double-click a repository to follow it. If you know its name, **File >
Follow** (^N) takes `owner/name`, or its address
(`https://github.com/whomper/EFTP`).

Text typed in Hebrew (F10) is looked for too.

## Following a repository

Before the Follow dialog opens, EUpdate asks GitHub for the repository's
latest release, so that it can say what it is and suggest where its
files go. The dialog has:

- **Folder**: where its files go (**Choose...** opens the file
  selector). It doesn't have to be there yet: EUpdate makes it, and the
  folders above it, at the first download. The first time it suggests a
  folder named after the repository; after that, one beside the last
  folder you chose.
- **Files**: which of a release's files to download. Empty means the
  `.zip` if the release has one, else all of them; or name them, with
  `*` and `?`, separated by commas: `*.PRG,*.HYP`.
- **Unpack a .zip into the folder**: a zip's files are written into the
  folder (its folders too), and the zip itself isn't kept. Off, the zip
  is written as it is.
- **These go elsewhere**: up to four rules, below.
- **Download the latest release now**.

**Get info** (^I) shows the same for a repository you follow, with the
release you have, and changes it.

## Where the files go

A release often has files that don't all belong in one folder, or
aren't all for your machine: a desk accessory goes in the root of the
boot drive; a release may have an edition for the Falcon and one for the
ST (AERA's `ECAL.ACC` and `ECALST.ACC`). A rule says where some of the
files go instead of the repository's folder, or that they are left out:

```
*.ACC                   ->  C:\
ECALST.ACC,ECALST.PRG   ->  -
```

- On the left, the files' names, with `*` and `?`, separated by commas.
- On the right, a folder (**...** opens the file selector), or `-` to
  leave the files out: they aren't even downloaded.
- The first rule that names a file decides. A file no rule names goes
  into the repository's folder.
- The files inside a zip are named by their own names: `ICONS\ECAL.RSC`
  is `ECAL.RSC` to the rules. A file a rule sends elsewhere goes there
  by its name alone; the others keep their folders inside the
  repository's folder.

When you follow a repository, EUpdate suggests rules from its latest
release: when it has an edition and its ST twin (`ECAL.ACC` and
`ECALST.ACC`), the ST edition is left out, as EUpdate runs on a Falcon or
TT; desk accessories go to the root of the boot drive. Change them as
you like.

## Checking and updating

- **Repository > Check for releases** (^R, or **Check**): asks GitHub
  for the latest release of each repository (of those selected, when
  some are). Nothing is downloaded.
- **Repository > Update** (^U, or **Update**): for each repository
  selected, or each that has a newer release when none is, asks for the
  latest release and downloads it if it is newer than yours.
- **Repository > Update all that are new** (Shift+^U).
- **Repository > Download again**: the latest release of those selected,
  whether you have it or not (after you deleted a file, or changed the
  rules).
- **Repository > Unfollow** (Delete): the repository goes from the list;
  its files stay where they are.

Every file is written safely: first under a name of its own
(`EUPDATE.PR$`), then renamed over the old one, so a reset in the middle
never leaves half a program. Long names are made 8+3 for TOS
(`EFTP-1.45.zip` becomes `EFTP-1_4.ZIP`; Settings > Files).

The info line says what EUpdate is doing, and how far a download has
come; Esc or **Stop** ends it.

## On a schedule

In **Settings > Checks**:

- **Only when I ask**: EUpdate checks when you say.
- **When EUpdate starts** (the first time): it checks every repository
  as it starts.
- **Then every N hours too**: and again every N hours while it runs.
- **Download what is new, not only say so**: a check made by itself
  downloads the newer releases too, as Update would.

EUpdate checks only while it runs. To have it check every time the
Atari starts, install it as an application that starts with the desktop
(TOS 4: **Options > Install application**, Boot status **Auto**), and
iconify its window: the icon says how many repositories have a new
release.

## Your GitHub token

Without a token GitHub answers 60 questions an hour from your address,
and 10 searches a minute; a check asks one question a repository. With
a token, 5,000 an hour and 30 searches a minute. On github.com: your
picture > **Settings > Developer settings > Personal access tokens**,
a fine-grained token with no rights at all (public repositories need
none). Type or paste it (^V) in **Settings > GitHub**: the field takes
up to 99 characters (a fine-grained token has 93) and scrolls as you
type; it shows stars, and the eye shows the token itself.

The token is kept in `EUPDATE.INF` encrypted with `EUPDATE.KEY`, goes
only to GitHub's API (never on to where a download is sent), and never
into the log. The About box says how many questions GitHub still allows
this hour.

## From other programs

A repository's name or address handed to EUpdate by the desktop or
another program (VA_START), or text dropped on its window, opens the
Follow dialog for it: drop a link from a web page or a text file on the
window.

## Settings

**Options > Settings** (^,):

- **Checks**: when EUpdate checks by itself, and whether it downloads
  what it finds (see [On a schedule](#on-a-schedule)).
- **Files**: long names shortened to 8+3, for TOS's disks; off for
  MiNT or MagiC with long names.
- **GitHub**: your token (see [Your GitHub token](#your-github-token)).
- **Look**: colours (new releases in orange, current ones in teal, on
  screens with 16 colours or more), and the Hebrew keyboard at start.
- **Advanced**: whether the Falcon's DSP checks GitHub's certificates
  and does the key exchange (else the 68030 does it all), and the
  protocol log, `EUPDATE.LOG`.

**Options > Font** chooses the font of the lists: the system font, or a
fixed-width GDOS font under NVDI or SpeedoGDOS.

## Keys

| Key | |
|---|---|
| ^F | find on GitHub |
| ^N | follow a repository by its name |
| ^R | check for releases |
| ^U | update (the selected ones, else all that are new) |
| Shift+^U | update all that are new |
| ^I | get info |
| Delete | unfollow |
| ^A | select all |
| ^C | copy the names selected (in dialogs ^C ^X ^V) |
| Return, double-click | get info; in Find, follow a repository or list an owner's |
| Backspace | in Find, back to the owners |
| Tab | the other list |
| Insert | the row's menu |
| ^, | Settings |
| Left, Right | in a dialog's field, the cursor; with Shift, to the start or the end (ClrHome too) |
| F10 | the Hebrew keyboard |
| Esc | stops what EUpdate is doing; in a dialog, Cancel |
| Help | this guide |
| ^W, ^Q | quit |

## When something goes wrong

- **"No network"**: STinG (or MiNTnet) isn't loaded.
- **"EUpdate needs the root certificates"**: copy `CACERT.PEM` and
  `ROOTS.DAT` next to `EUPDATE.PRG`.
- **"GitHub's limit is reached"**: 60 questions an hour without a
  token; wait, or add one (see [Your GitHub token](#your-github-token)).
- **"GitHub didn't take the token"**: it has expired or was mistyped:
  make a new one, or leave the field empty.
- **"No release"**: the repository publishes its programs some other
  way. EUpdate follows releases only.
- **"Can't make its folder"**, **"can't write"**: the drive is full or
  locked, or the path is too long for TOS.
- **"the file is too big for this Atari's memory"**: a release's file is
  held in memory until it is whole; quit other programs, or leave the
  big file out (Get info > Files).
- **A failed row**: rest the mouse on it for what went wrong.

For anything else, switch on **Options > Protocol log**, try again, and
look at `EUPDATE.LOG`. It starts each job with a line about your Atari
(TOS, AES, NVDI, the free ST and TT RAM), then has every question
asked, how GitHub answered, and each step: what is downloaded, every
256 KB of it, every file written. Each step is written at once, so
after a crash its last line says where EUpdate was. The token never
goes into it.

If EUpdate crashes, `CRASH.LOG` in its folder says where. When TOS
itself shows the bombs, start EUpdate again: it finds TOS's record of
the crash and writes it into `CRASH.LOG`.

## EUPDATE.INF

EUpdate's settings and the repositories it follows, as text: the
options under `[OPTIONS]`, then a `[REPO]` part for each repository.
Each setting has a note above it and an empty line below, and most are
set in the dialogs. One that
isn't: `APIBASE=`, GitHub's API address, for a GitHub Enterprise server
(`https://HOST/api/v3`).
