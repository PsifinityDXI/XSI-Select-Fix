# Automatic startup loader for XSISelectFix.
#
# Only this Python file belongs in Application\Plugins. The native hook DLL is
# deliberately kept under Application\bin\XSISelectFix so Softimage does not
# mistake it for a native self-installed plug-in during its directory scan.

import ctypes
import os
from ctypes import wintypes


_xsi_select_fix_module = None

# Autodesk Softimage SDK siEventID.siOnStartup. Using the documented numeric
# value avoids depending on late-bound COM constants during plug-in discovery.
SI_ON_STARTUP = 542179351


def _native_runtime_handle():
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
    kernel32.GetModuleHandleW.restype = ctypes.c_void_p
    return kernel32.GetModuleHandleW(u"XSISelectFix.dll")


def _process_creation_time():
    creation = wintypes.FILETIME()
    exit_time = wintypes.FILETIME()
    kernel_time = wintypes.FILETIME()
    user_time = wintypes.FILETIME()
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetCurrentProcess.restype = wintypes.HANDLE
    kernel32.GetProcessTimes.argtypes = [
        wintypes.HANDLE,
        ctypes.POINTER(wintypes.FILETIME),
        ctypes.POINTER(wintypes.FILETIME),
        ctypes.POINTER(wintypes.FILETIME),
        ctypes.POINTER(wintypes.FILETIME)
    ]
    kernel32.GetProcessTimes.restype = wintypes.BOOL
    if not kernel32.GetProcessTimes(
        kernel32.GetCurrentProcess(), ctypes.byref(creation),
        ctypes.byref(exit_time), ctypes.byref(kernel_time),
        ctypes.byref(user_time)
    ):
        return None
    ticks = (creation.dwHighDateTime << 32) | creation.dwLowDateTime
    return (ticks / 10000000.0) - 11644473600.0


def _load_native_runtime_permanently(dll_path):
    # Do not construct ctypes.WinDLL(dll_path). Its Python wrapper owns a DLL
    # reference and can call FreeLibrary when Softimage refreshes/unloads the
    # Python plug-in module. The native runtime patches XSI import entries, so
    # unloading it before process exit would leave dangling function pointers.
    # This raw LoadLibraryW reference is intentionally leaked until XSI exits.
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.LoadLibraryW.argtypes = [ctypes.c_wchar_p]
    kernel32.LoadLibraryW.restype = ctypes.c_void_p
    handle = kernel32.LoadLibraryW(dll_path)
    if not handle:
        error_code = ctypes.get_last_error()
        raise OSError(error_code, ctypes.FormatError(error_code), dll_path)

    kernel32.GetProcAddress.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    kernel32.GetProcAddress.restype = ctypes.c_void_p
    initialize_address = kernel32.GetProcAddress(
        handle, b"XSISelectFixInitialize"
    )
    if not initialize_address:
        error_code = ctypes.get_last_error()
        raise OSError(
            error_code,
            "XSISelectFixInitialize export was not found",
            dll_path
        )

    initialize = ctypes.WINFUNCTYPE(wintypes.BOOL)(initialize_address)
    if not initialize():
        error_code = ctypes.get_last_error()
        raise OSError(
            error_code,
            "XSISelectFixInitialize could not start the hook worker",
            dll_path
        )
    return handle


def XSILoadPlugin(in_reg):
    global _xsi_select_fix_module

    in_reg.Author = "XSISelectFix"
    in_reg.Name = "XSISelectFixLoader"
    in_reg.Major = 1
    in_reg.Minor = 5

    # Registration must be invariant. Softimage may call XSILoadPlugin while
    # installing the add-on and cache the advertised commands immediately.
    # Advertising a different command set after restart can destabilize the
    # live plug-in refresh transaction.
    in_reg.RegisterCommand("XSISelectFixStatus", "XSISelectFixStatus")
    in_reg.RegisterEvent("XSISelectFixStartup", SI_ON_STARTUP)
    return True


def XSISelectFixStartup_OnEvent(in_context):
    global _xsi_select_fix_module

    # This callback is fired only for a genuine Softimage startup. It does not
    # run when the Add-On Installer discovers this Python plug-in mid-session.
    if _native_runtime_handle():
        Application.LogMessage("XSISelectFix startup: native runtime already loaded")
        return True

    application_dir = os.path.dirname(__sipath__)
    dll_path = os.path.join(
        application_dir, "bin", "XSISelectFix", "XSISelectFix.dll"
    )

    # Some Softimage builds can deliver siOnStartup immediately when an event
    # is registered after startup. A freshly written package DLL therefore
    # still means "installation transaction": defer it to the next process.
    process_created = _process_creation_time()
    try:
        dll_timestamp = max(os.path.getctime(dll_path), os.path.getmtime(dll_path))
    except Exception as error:
        Application.LogMessage("XSISelectFix runtime is unavailable: " + str(error), 4)
        return True
    if process_created is None or dll_timestamp >= process_created:
        Application.LogMessage(
            "XSISelectFix installed. Restart Softimage to activate the fix."
        )
        return True

    try:
        _xsi_select_fix_module = _load_native_runtime_permanently(dll_path)
        Application.LogMessage("XSISelectFix loaded at startup: " + dll_path)
    except Exception as error:
        Application.LogMessage("XSISelectFix startup load failed: " + str(error), 4)
    return True


def XSIUnloadPlugin(in_reg):
    # Keep the DLL resident until XSI exits because its IAT hooks remain active.
    return True


def XSISelectFixStatus_Init(in_context):
    command = in_context.Source
    command.Description = "Report whether the XSISelectFix native runtime is loaded."
    command.ReturnValue = True
    return True


def XSISelectFixStatus_Execute():
    # Softimage can refresh a Python plug-in into a new module namespace while
    # the native DLL remains resident. Query Windows rather than trusting the
    # Python wrapper variable, which would otherwise produce a false negative.
    loaded = bool(_native_runtime_handle())
    if loaded:
        Application.LogMessage("XSISelectFix status: native runtime loaded")
    else:
        Application.LogMessage("XSISelectFix status: native runtime not loaded")
    return loaded
