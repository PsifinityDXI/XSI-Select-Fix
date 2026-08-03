<div align="center">

# XSISelectFix - Build-from-Source (BFS) Version

**Source-build package for correcting Softimage 2015 transform-gizmo picking on affected Intel graphics drivers.**

![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D6)
![Toolchain](https://img.shields.io/badge/toolchain-Visual%20Studio%202022-5C2D91)
![Softimage](https://img.shields.io/badge/Softimage-2015%20SP2%20%7C%20R2--SP2-6D4C7D)

</div>

## Transparency Disclaimer!
The results of this fix came from a lengthy debugging session spearheaded via AI, as not only was discussion about the problem on Google rather scarce, but what was determined to be the problem is also simply well beyond my own capabilities. 
To stress again: **<u>this is purely about debugging and fixing a rare-yet-known bug with Softimage's interaction with newer Intel drivers.</u>**

Naturally, I easily encourage anyone with far more experience in this field to create a tool based off of this so that we have **an AI-free version**. 

I personally only see AI as *an extensive debugger and guidance tool*, and in this SPECIFIC case, it's simply a springboard for someone else to develop something more refined.

## About

XSISelectFix corrects a legacy OpenGL selection problem that can make Softimage's
rotation manipulator continually highlight the Z axis instead of the axis beneath
the pointer. The issue has been observed with Intel UHD Graphics 620 drivers on
modern Windows installations.

The fix is process-local. It does **not** replace Windows' `opengl32.dll`, modify
the Intel graphics driver, or overwrite Autodesk binaries.

This specific package contains the source needed to build the native x64 DLL and prepare either
a direct CMD installation or a conventional Softimage `.xsiaddon` package.

## Package layout

```text
XSISelectFix-source-build\
├── README.md
├── CMD Method\
│   ├── Source\
│   ├── BUILD-FIRST.txt
│   ├── Build.cmd
│   ├── Install.cmd
│   └── Uninstall.cmd
└── XSIAddon Method\
    ├── Source\
    ├── BUILD-FIRST.txt
    ├── Build.cmd
    ├── Install.cmd
    └── Uninstall.cmd
```

Choose **one** method. Each method has its own self-contained source copy and build
front end.

| Method              | Produced artifact                         | Installation behavior                                                   |
| ------------------- | ----------------------------------------- | ----------------------------------------------------------------------- |
| **CMD Method**      | An expanded `XSISelectFix` payload folder | Copies the payload directly into detected Softimage 2015 user profiles. |
| **XSIAddon Method** | `XSISelectFix.xsiaddon`                   | Uses Softimage's closed-application add-on installation mode.           |

## Build requirements

- Windows 10 or Windows 11 x64
- Visual Studio 2022 Community or newer
- **Desktop development with C++** workload
- **C++ CMake tools for Windows** component
- A Windows SDK selected by Visual Studio
- Python 3 for the standalone `.xsiaddon` builder

Softimage itself is not needed to compile the DLL or construct the `.xsiaddon`, but it
must be installed to test or use the finished fix.

## Build and install

### Option A: CMD Method

1. Open an **x64 Developer Command Prompt for VS 2022**.

2. Change to the extracted `CMD Method` folder.

3. Run:
   
   ```bat
   Build.cmd
   ```

4. Wait for the build and payload preparation to complete.

5. Close Softimage completely.

6. Run `Install.cmd` normally, without Administrator privileges.

7. Start Softimage normally.

### Option B: XSIAddon Method

1. Open an **x64 Developer Command Prompt for VS 2022**.

2. Change to the extracted `XSIAddon Method` folder.

3. Run:
   
   ```bat
   Build.cmd
   ```

4. Confirm that `XSISelectFix.xsiaddon` appears beside `Install.cmd`.

5. If open, close Softimage completely.

6. Run `Install.cmd` normally.

7. Start Softimage normally.

The standalone add-on builder constructs and round-trip verifies all three embedded
files. It does not require opening Softimage's Script Editor.

## Build outputs

The native build creates the following files under `Source\build\Release`:

```text
XSISelectFix.dll
XSISelectFix.enable
XSISelectFixLoader.py
XSISelectInjector.exe
```

`XSISelectFix.enable` opts the DLL into the compatibility fix. Removing that marker
returns the native component to diagnostic behavior.

## Verify the installation

Open Softimage's Python Script Editor and run:

```python
Application.XSISelectFixStatus()
```

Expected result:

```text
XSISelectFix status: native runtime loaded
```

Select an object and test the Rotate manipulator. X, Y, and Z should highlight
individually, and only the chosen axis should influence the object throughout a drag.

## Uninstallation

Close Softimage and run the selected method's `Uninstall.cmd`.

The uninstaller removes the dedicated `Addons\XSISelectFix` folder and exact
XSISelectFix entries under `InstalledAddons`. Other add-ons are left untouched.

## Architecture

Softimage uses legacy OpenGL selection mode to identify transform handles. On affected
Intel drivers, the resulting hit data can cause the Z handle to win regardless of the
pointer position. The native component intercepts the relevant OpenGL path, performs
the picking pass with occlusion queries, and synthesizes the hit records Softimage
expects.

The DLL follows a strict lifetime model:

1. `DllMain` remains inert during package probing.
2. Softimage discovers the Python plug-in.
3. A genuine startup event calls the exported `XSISelectFixInitialize` function.
4. The DLL installs its process-local hooks once.
5. The DLL remains resident until `XSI.exe` exits.

This avoids leaving patched imports pointing into an unloaded module during add-on
installation or plug-in refresh operations.

## Development notes

- Target architecture is Windows x64 (`0x8664`).
- The project uses CMake and C++17.
- The MSVC runtime is linked statically for the release DLL.
- The add-on payload contains the DLL, enable marker, and Python startup loader.
- Do not install the native DLL directly into `Application\Plugins`; the Python loader
  keeps native initialization separate from Softimage's plug-in probing.
- The legacy injector remains a diagnostic fallback and is not needed for normal use.

## Troubleshooting

### `build\Release` was not created

Run `Build.cmd` from an x64 Visual Studio Developer Command Prompt. Confirm that the
C++ workload, CMake component, and Windows SDK are installed through Visual Studio
Installer.

### Python 3 was not found

Install Python 3 with the Windows `py` launcher, or run
`Source\build-xsiaddon-standalone.py` with an available Python 3 interpreter.

### Softimage reports an unusual installation return code

Softimage 2015 can return `-1073741819` after writing a complete add-on. The supplied
installer validates the installed files and accepts the result only when the payload
is complete.

### Softimage is reported as running

Close every `XSI.exe` process before installing or uninstalling. The native runtime is
designed to remain loaded until process shutdown.

## Safety

- Test new builds with a disposable scene first.
- Keep Autodesk and Windows OpenGL files unchanged.
- Do not unload the DLL after its hooks have been installed.
- Save important work before testing locally modified source revisions.
