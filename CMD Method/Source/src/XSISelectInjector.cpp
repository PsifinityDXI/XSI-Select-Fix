#include <windows.h>
#include <tlhelp32.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

DWORD FindProcessId(const wchar_t* executableName) {
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    DWORD pid = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, executableName) == 0) {
                if (pid != 0) {
                    std::wcerr << L"More than one " << executableName
                               << L" process is running. Close the extra instance and retry.\n";
                    CloseHandle(snapshot);
                    return 0;
                }
                pid = entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

std::filesystem::path ExecutableDirectory() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

std::wstring ErrorText(DWORD error) {
    wchar_t* message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, error, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::wstring result = message ? message : L"Unknown error";
    if (message) LocalFree(message);
    return result;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const DWORD pid = argc >= 2 ? wcstoul(argv[1], nullptr, 10) : FindProcessId(L"XSI.exe");
    if (!pid) {
        std::wcerr << L"Could not find exactly one running XSI.exe. You may pass its PID explicitly.\n"
                   << L"Usage: XSISelectInjector.exe [PID] [full-path-to-DLL]\n";
        return 1;
    }

    std::filesystem::path dllPath = argc >= 3
        ? std::filesystem::absolute(argv[2])
        : ExecutableDirectory() / L"XSISelectFix.dll";
    std::error_code ec;
    dllPath = std::filesystem::weakly_canonical(dllPath, ec);
    if (ec || !std::filesystem::is_regular_file(dllPath)) {
        std::wcerr << L"DLL not found: " << dllPath << L"\n";
        return 1;
    }

    const HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                           PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                       FALSE, pid);
    if (!process) {
        const DWORD error = GetLastError();
        std::wcerr << L"OpenProcess failed: " << ErrorText(error)
                   << L"Run the injector at the same privilege level as Softimage.\n";
        return 1;
    }

    const std::wstring pathText = dllPath.native();
    const SIZE_T byteCount = (pathText.size() + 1) * sizeof(wchar_t);
    void* remotePath = VirtualAllocEx(process, nullptr, byteCount, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath || !WriteProcessMemory(process, remotePath, pathText.c_str(), byteCount, nullptr)) {
        const DWORD error = GetLastError();
        std::wcerr << L"Unable to copy DLL path into XSI: " << ErrorText(error) << L"\n";
        if (remotePath) VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }

    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    const auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(kernel32, "LoadLibraryW"));
    const HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remotePath, 0, nullptr);
    if (!thread) {
        const DWORD error = GetLastError();
        std::wcerr << L"CreateRemoteThread failed: " << ErrorText(error) << L"\n";
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }

    WaitForSingleObject(thread, 15000);
    DWORD loadResult = 0;
    GetExitCodeThread(thread, &loadResult);
    CloseHandle(thread);
    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    CloseHandle(process);

    if (!loadResult) {
        std::wcerr << L"XSI could not load XSISelectFix.dll. Check Windows Event Viewer and antivirus history.\n";
        return 1;
    }

    std::wcout << L"Diagnostic hook injected into XSI.exe (PID " << pid << L").\n"
               << L"Log: " << dllPath.parent_path() / L"XSISelectFix.log" << L"\n";
    return 0;
}
