# XSI-Select-Fix

XSI-Select-Fix works around incorrect legacy OpenGL selection results produced by Intel
graphics drivers in 64-bit Autodesk Softimage 2015. It replaces only Softimage's
selection pass with occlusion queries and synthesizes the hit records expected by the
transform gizmos.

## Transparency Disclaimer!
The results of this fix came from a lengthy debugging session spearheaded via AI, specifically one involving disassembly to find the root of the problem. as not only was discussion about the problem on Google rather scarce, but what was determined to be the problem is also simply well beyond my own capabilities.

To stress again: **<u>this is purely about debugging and fixing a rare-yet-known bug with Softimage's selection interaction with newer Intel drivers.</u>**

Naturally, I easily encourage anyone with far more experience in this field to create a tool based off of this so that we have **an AI-free version**. 

I personally only see AI as *an extensive debugger and guidance tool*, and in this SPECIFIC case, it's simply a springboard for someone else to develop something more refined.

## End-user release: three-item ZIP

The preferred public release does not use Softimage's Add-On Installer. After the
developer builds the DLL once, run `package-release.cmd`. This creates:

```text
dist\XSISelectFix-release.zip
```

The ZIP presents only three top-level items:

```text
XSISelectFix\
Install-XSISelectFix.cmd
Uninstall-XSISelectFix.cmd
```

`XSISelectFix\` contains the ready-to-install `Application` payload and a `Source`
subfolder with the diagnostic/source project. The end user extracts the ZIP, closes
Softimage, and double-clicks `Install-XSISelectFix.cmd`. The installer copies and
verifies the payload directly under every detected per-user Softimage 2015 profile.
It does not launch Softimage, produce a `dist` folder, invoke CMake, require Visual
Studio, or require Administrator access. `Uninstall-XSISelectFix.cmd` removes those
same verified per-user add-on folders.

Only the developer producing a new binary needs to run `build-release.cmd`. Neither
`build-xsiaddon.py` nor an open Softimage session is required to build the public ZIP.

`package-release.cmd` stages into a uniquely named directory under `%TEMP%`, never
inside the source project. This prevents a source-copy operation from recursively
copying its own output. It displays progress from 0 to 100 percent and writes the full
packaging transcript to `dist\XSISelectFix-package.log`.

## Recommended: closed-Softimage `.xsiaddon` installation

1. Run `build-release.cmd`.
2. In Softimage, choose **File > Run Script** and run `build-xsiaddon.py` from this
   source directory.
3. The genuine package produced by Softimage is written to:

   ```text
   dist\XSISelectFix.xsiaddon
   dist\Install-XSISelectFix.cmd
   dist\Uninstall-XSISelectFix.cmd
   ```

4. Close Softimage completely.
5. Double-click `dist\Install-XSISelectFix.cmd`. This is the intended one-click
   front end. It refuses to proceed while `XSI.exe` is running, locates a standard
   Softimage 2015 installation, initializes Softimage's command environment, invokes
   Autodesk's `XSI.exe -i` add-on installation mode, and verifies all three installed
   files before reporting success. Do not run it as Administrator for the normal
   per-user installation.
6. Start Softimage normally, run `Application.XSISelectFixStatus()` in the Python
   Script Editor, and verify X, Y, and Z gizmo highlighting.

Do not use **File > Add-On > Install** on a system where live installation faults in
`syflex_ice.64.dll_unloaded`. The package files can install successfully, but the
interactive plug-in refresh can unload Syflex ICE and then call stale Syflex code,
crashing that Softimage session. This failure is separate from XSISelectFix; installing
from the command line while Softimage is closed avoids refreshing Syflex inside an
existing interactive session.

To remove the add-on, close Softimage and double-click
`dist\Uninstall-XSISelectFix.cmd`. Softimage 2015 does not consistently create an
`InstalledAddons` record for this package and can return success without removing the
files. The uninstaller therefore validates and removes only the dedicated per-user
`Addons\XSISelectFix` directory, after applying the same running-process safety check.

Softimage's closed-app installer may return `-1073741819` (`0xC0000005`) after it has
already written a complete, working payload. The wrapper treats that value as a warning
when all three installed files pass verification; an incomplete or missing payload is
still reported as a real failure.

The package installs into the per-user add-on root under an `XSISelectFix` subdirectory:

- `Application\Plugins\XSISelectFixLoader.py`
- `Application\bin\XSISelectFix\XSISelectFix.dll`
- `Application\bin\XSISelectFix\XSISelectFix.enable`

The Add-On Installer may display all three packaged files under its broad **Others**
category. This is expected for the `AddOtherItem` entries used for Softimage 2015
compatibility; verify the destination directories instead. The loader still resides in
`Application\Plugins`, while the DLL and enable marker remain support files under the
separate runtime directory.

The loader registers an `XSISelectFixStatus` command. Its status check queries Windows
for the resident `XSISelectFix.dll` module, so it remains accurate even if Softimage
refreshes the Python plug-in into a new module namespace.

During packaging, `build-xsiaddon.py` uses Softimage's `Addon` object model, explicitly
sets `SubDirectory` to `XSISelectFix`, and assigns a destination directory to each of
the three source files. It does not create a staging tree at all, so an installed copy
does not need to be removed before rebuilding. Softimage 2015 may label the files as
**Others** in its preview; the explicit destination directories remain authoritative.

The `.xsiaddon` uses the immediate loader confirmed by testing. No timer, external
injector, administrator access, or modification of the Softimage factory installation
is required.

## Factory-directory installation

1. Close Softimage.
2. Run `build-release.cmd`.
3. Run `install-startup.cmd` as Administrator once. If Softimage is installed in a
   nonstandard location, pass its install root, for example:

   ```text
   install-startup.cmd "D:\Autodesk\Softimage 2015 SP2"
   ```

4. Start Softimage normally. Do **not** run the injector.
5. Confirm that X, Y, and Z highlight separately before opening important work.

The installer adds a Python loader to Softimage's scanned plug-in directory and keeps
the native hook in a separate, non-scanned runtime directory. It does not replace any
Autodesk or Windows file:

- `Application\Plugins\XSISelectFixLoader.py`
- `Application\bin\XSISelectFix\XSISelectFix.dll`
- `Application\bin\XSISelectFix\XSISelectFix.enable`

The Python plug-in is discovered during normal startup and immediately loads the DLL.
The DLL waits for `opengl32.dll` and `graphicogl.dll` before installing its process-local
hooks.

The DLL must not be placed directly in `Application\Plugins`: Softimage treats DLLs
there as native self-installed plug-ins and loads them immediately during application
startup, bypassing the deferred Python timer.

To remove it, close Softimage and run `uninstall-startup.cmd` as Administrator. The
manual injector remains available as a fallback and for diagnostics.

### Immediate-startup comparison test

`install-startup-immediate-test.cmd` installs the same split file layout but replaces
the timed Python loader with `XSISelectFixLoaderImmediate.py`. This test loader calls
`ctypes.WinDLL` directly from `XSILoadPlugin` and does not register a timer. Use it only
to compare startup behavior with the proven deferred loader. Running the normal
`install-startup.cmd` again restores the deferred version.

## Safety and scope

- Keep your original Softimage installation files unchanged.
- Do not copy any replacement `opengl32.dll` into Softimage's `Application\\bin` directory.
- The DLL lives only inside the current XSI process. Closing Softimage unloads it.
- This is an experimental diagnostic. Save important scenes before testing.
- Run the injector at the same privilege level as Softimage. Normally, run both without
  Administrator privileges.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 Community with **Desktop development with C++**
- The workload's **CMake tools for Windows** component

## Build

1. Extract this directory somewhere writable.
2. Double-click `build-release.cmd`.
3. Find these files in `build\\Release`:
   - `XSISelectFix.dll`
   - `XSISelectInjector.exe`

If Windows or antivirus quarantines a locally built injector, inspect the alert rather
than disabling protection globally. The injector uses `OpenProcess`,
`VirtualAllocEx`, and `CreateRemoteThread` solely to load the diagnostic DLL into the
selected XSI process, so heuristic scanners may flag it.

## Manual injector / diagnostic test

1. Start Softimage normally.
2. Open a disposable scene, select an object, and activate Rotate.
3. Leave Softimage running. Do not attach WinDbg for this first injection test.
4. Run `XSISelectInjector.exe` from `build\\Release`.
5. Run `run-injector.cmd`; it keeps the result visible. The console should report a
   successful injection.
6. Hover away from the gizmo, then over X, Y, and Z for a few seconds each.
7. Close Softimage normally.
8. Open `XSISelectFix.log` beside the DLL and share its contents.

The current diagnostic also logs selection-time `glBegin`/`glEnd`, display-list calls,
draw calls, and whether the Intel context exposes occlusion queries. This determines
which interception path the replacement build should use.

If more than one `XSI.exe` exists, pass the intended PID:

```text
XSISelectInjector.exe 12345
```

## Expected log evidence

The log should contain entries such as:

```text
glSelectBuffer size=...
glRenderMode 1C02 -> result=0 (previous=1C00)
glLoadName 00000021
glRenderMode 1C00 -> result=3 (previous=1C02)
hit[0] min=7FFFFC84 max=7FFFFC84 names=[FFFFFFFF,00000021]
```

OpenGL constants used above:

- `0x1C00`: `GL_RENDER`
- `0x1C02`: `GL_SELECT`
- `0x21`: X rotation handle
- `0x22`: Y rotation handle
- `0x24`: Z rotation handle

## Removal

Close Softimage and delete the extracted `XSISelectFix` directory. No system or
Softimage files are installed or replaced.

## Next stage

The package now contains an **opt-in experimental replacement**. Diagnostic-only mode
remains the default.

To enable the replacement, create an empty file named:

```text
XSISelectFix.enable
```

beside `XSISelectFix.dll` in `build\\Release`, then inject into a newly started XSI
process. The replacement keeps Intel in ordinary render mode, suppresses color/depth/
stencil writes, wraps each named gizmo section in an occlusion query, and synthesizes
Softimage-compatible hit records. Delete `XSISelectFix.enable` to return to logging-only
mode.

Use a disposable scene and hover only during the first test—do not click or drag a
manipulator until the log confirms stable synthesized hits. Close XSI to unload the DLL.

Fix mode uses quiet logging by default and writes a startup line plus a summary every
1,000 selection passes. Create an empty `XSISelectFix.verbose` file beside the DLL to
restore detailed per-call and per-query logging for troubleshooting. Query objects are
reused for the lifetime of XSI to avoid per-pass allocation overhead.
# Standalone .xsiaddon build

The optional conventional Softimage package can now be generated without
opening Softimage:

```text
build-xsiaddon-standalone.cmd
```

This creates `dist\XSISelectFix.xsiaddon` and verifies every embedded payload
by decoding it again. The package follows the Softimage 2015 add-on format used
by known-good packages: all three files are `Others` items with explicit
destination directories. This is separate from the tested direct-copy release
packager; `package-release.cmd` is intentionally unchanged.

The packaged Python plug-in is registration-only during discovery and live
installation. It registers an `siOnStartup` event, and that event loads the
native runtime only during the next genuine Softimage startup. This avoids
activating process hooks inside the Add-On Installer's live refresh transaction.
The native DLL is loaded with a deliberately process-lifetime `LoadLibraryW`
reference rather than a managed `ctypes.WinDLL` object. Softimage may unload and
reload the Python plug-in during refresh, but it must never unload a DLL whose
hook addresses remain installed in XSI's import table.

`DllMain` itself never installs hooks. Softimage's Add-On Installer probes DLL
payloads with `LoadLibraryExW` followed by `FreeLibrary`; performing hook work
from `DllMain` would leave dangling import-table pointers after that probe. The
startup loader explicitly calls the exported `XSISelectFixInitialize` function
only after loading the DLL for a genuine runtime session.
