// Copyright (C) 2016-2017 Ivan Zhakov
#include "stdafx.h"

#define PBENCH_VERSION_MAJOR 0
#define PBENCH_VERSION_MINOR 4
#define PBENCH_VERSION_PATCH 0

typedef LONG (WINAPI * NtSetTimerResolutionProc)(
    ULONG ReqRes,
    BOOL  Acquire,
    ULONG *pNewRes);

class ProcessCounters
{
public:
    ProcessCounters()
    {
        ZeroMemory(&ioCounters, sizeof(ioCounters));
        ZeroMemory(&memCounters, sizeof(memCounters));
    }

    ProcessCounters(HANDLE hProcess)
    {
        ZeroMemory(&ioCounters, sizeof(ioCounters));
        ZeroMemory(&memCounters, sizeof(memCounters));

        BOOL b;
        CFileTime kernelTime;
        CFileTime userTime;
        b = GetProcessTimes(hProcess, &m_StartTime, &m_ExitTime, &kernelTime, &userTime);
        if (!b)
        {
            fwprintf(stderr, L"Failed to get process time information: %s\n",
                     (LPCWSTR)AtlGetErrorDescription(HRESULT_FROM_WIN32(GetLastError())));
        }
        m_KernelTime = kernelTime.GetTime();
        m_UserTime = userTime.GetTime();

        b = GetProcessIoCounters(hProcess, &ioCounters);
        if (!b)
        {
            fwprintf(stderr, L"Failed to get process IO information: %s\n",
                     (LPCWSTR)AtlGetErrorDescription(HRESULT_FROM_WIN32(GetLastError())));
        }

        b = GetProcessMemoryInfo(hProcess, &memCounters, sizeof(memCounters));
        if (!b)
        {
            fwprintf(stderr, L"Failed to get process memory information: %s\n",
                     (LPCWSTR)AtlGetErrorDescription(HRESULT_FROM_WIN32(GetLastError())));
        }
    }

    const CFileTime & StartTime() { return m_StartTime; }
    const CFileTime & ExitTime() { return m_ExitTime; }
    const CFileTimeSpan & KernelTime() { return m_KernelTime; }
    const CFileTimeSpan & UserTime() { return m_UserTime; }
    const IO_COUNTERS & IOCounters() { return ioCounters; }
    const PROCESS_MEMORY_COUNTERS & MemCounters() { return memCounters; }

private:
    CFileTime m_StartTime;
    CFileTime m_ExitTime;
    CFileTimeSpan m_KernelTime;
    CFileTimeSpan m_UserTime;
    IO_COUNTERS ioCounters;
    PROCESS_MEMORY_COUNTERS memCounters;
};

struct AdditionalProcessInfo
{
    AdditionalProcessInfo(HANDLE hProcess)
        : m_hProcess(hProcess)
    {
        DWORD dw = MAX_PATH * sizeof(WCHAR);
        BOOL b = QueryFullProcessImageName(hProcess, 0, m_ImageFileName.GetBuffer(MAX_PATH), &dw);
        if (b)
        {
            m_ImageFileName.ReleaseBuffer();
        }
    }

    HANDLE m_hProcess;
    CString m_ImageFileName;
    ProcessCounters m_startCounters;
    ProcessCounters m_endCounters;
    const LPCWSTR ImageFileName() { return m_ImageFileName; }
};

struct SamplingProcessInfo
{
    SamplingProcessInfo(HANDLE hProcess)
        : m_hProcess(hProcess)
    {
    }

    HANDLE m_hProcess;
    ProcessCounters m_Counters;
};

CString FormatTime(ULONGLONG ft)
{
    CString result;

    ULONGLONG ms  = ft / CFileTime::Millisecond;
    result.Format(L"%I64d.%03d", ms / 1000ULL, (int) (ms % 1000ULL));
    return result;
}

template<typename  T>
void GetTotalMinMax(const std::list<T> & list, T & total, T & min, T & max)
{
    auto minmax = std::minmax_element(list.begin(), list.end());
    min = *minmax.first;
    max = *minmax.second;

    total = 0;
    for (auto t : list)
    {
        total += t;
    }
}

CString FormatAvgMinMaxTime(const std::list<CFileTimeSpan> & times)
{
    CFileTimeSpan min;
    CFileTimeSpan max;
    CFileTimeSpan total;

    GetTotalMinMax(times, total, min, max);

    CString result;
    if (times.size() > 1)
    {
        result.Format(L"%8s s  %8s s  %8s s",
                      FormatTime(total.GetTimeSpan() / times.size()),
                      FormatTime(min.GetTimeSpan()),
                      FormatTime(max.GetTimeSpan()));
    }
    else
    {
        result.Format(L"%8s s", FormatTime(total.GetTimeSpan()).GetString());
    }

    return result;
}

CString FormatAvgMinMaxIO(std::list<ULONGLONG> count, std::list<ULONGLONG> bytes)
{
    ULONGLONG countTotal, countMin, countMax;
    ULONGLONG bytesTotal, bytesMin, bytesMax;
    
    GetTotalMinMax(count, countTotal, countMin, countMax);
    GetTotalMinMax(bytes, bytesTotal, bytesMin, bytesMax);

    CString result;
    if (count.size() > 1)
    {
        result.Format(L"%8lld (%8lld KB)  %8lld (%8lld KB)  %8lld (%8lld KB)",
                      countTotal / count.size(), bytesTotal / bytes.size() / 1024,
                      countMin, bytesMin / 1024,
                      countMax, bytesMax / 1024);
    }
    else
    {
        result.Format(L"%8lld (%8lld KB)", countTotal, bytesTotal / 1024) ;
    }

    return result;
}

CString FormatAvgMinMaxMem(std::list<LONGLONG> list)
{
    LONGLONG total, min, max;

    GetTotalMinMax(list, total, min, max);

    CString result;
    if (list.size() > 1)
    {
        result.Format(L"%8lld KB  %8lld KB  %8lld KB",
                      total / list.size() / 1024, min / 1024, max / 1024);
    }
    else
    {
        result.Format(L"%8lld KB", total / 1024);
    }

    return result;
}

DWORD SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
{
    LUID luid;
    BOOL bRet = FALSE;

    if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid))
        return GetLastError();

    TOKEN_PRIVILEGES tp;

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = (bEnablePrivilege) ? SE_PRIVILEGE_ENABLED : 0;

    //  Enable the privilege or disable all privileges.
    AdjustTokenPrivileges(hToken, FALSE, &tp, NULL, (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);

    return GetLastError();
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc <= 1)
    {
        wprintf(L"Process benchmark utility version %d.%d.%d. Copyright (C) 2017 Ivan Zhakov\n\n",
                PBENCH_VERSION_MAJOR, PBENCH_VERSION_MINOR, PBENCH_VERSION_PATCH);
        wprintf(L"usage: pbench [OPTIONS] COMMAND [ARGS]\n");
        wprintf(L"Available options:\n");
        wprintf(L"  --pid PID      Also benchmark process with specified process ID\n");
        wprintf(L"  --count COUNT  Run command COUNT times\n");
        wprintf(L"  --concurrent   Run command in parallel\n");
        wprintf(L"  --stdout-nul   Redirect command output to NUL\n");
        return 0;
    }

    // Try obtain debug privilege to be able to measure service processes.
    HANDLE processToken;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &processToken);
    SetPrivilege(processToken, SE_DEBUG_NAME, TRUE);

    ULONG newRes;
    NtSetTimerResolutionProc fnSetTimerResolution;
    HMODULE ntdll = LoadLibrary(L"ntdll.dll");
    fnSetTimerResolution = (NtSetTimerResolutionProc) GetProcAddress(ntdll, "NtSetTimerResolution");

    fnSetTimerResolution(5000, TRUE, &newRes);

    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    BOOL b;
    CString cmd;
    std::list<SamplingProcessInfo> samplingProcess;
    std::list<AdditionalProcessInfo> additionalProcess;

    int i = 1;
    BOOL concurrent = FALSE;
    int count = 1;
    BOOL stdoutNull = FALSE;

    CHandle nullFile;
    {
        SECURITY_ATTRIBUTES sa = { 0 };
        sa.bInheritHandle = TRUE;
        nullFile = CHandle(CreateFile(L"NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL));
    }

    for (; i < argc && argv[i][0] == L'-'; i++)
    {
        if (wcscmp(argv[i], L"--pid") == 0)
        {
            i++;
            if (i >= argc)
            {
                fwprintf(stderr, L"--pid option requires argument\n");
                return 1;
            }

            DWORD pid = (DWORD) _wtoi64(argv[i]);

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!hProcess)
            {
                fwprintf(stderr, L"Unable to open process %u: %s\n",
                    pid,
                    (LPCWSTR)AtlGetErrorDescription(HRESULT_FROM_WIN32(GetLastError())));
                return 1;
            }

            additionalProcess.push_back(AdditionalProcessInfo(hProcess));
        }
        else if (wcscmp(argv[i], L"--count") == 0)
        {
            i++;
            if (i >= argc)
            {
                fwprintf(stderr, L"--count option requires argument\n");
                return 1;
            }

            count = (int)_wtoi64(argv[i]);
            if (count < 1)
            {
                fwprintf(stderr, L"Invalid value for --count argument specified\n");
                return 1;
            }
        }
        else if (wcscmp(argv[i], L"--concurrent") == 0)
        {
            concurrent = TRUE;
        }
        else if (wcscmp(argv[i], L"--stdout-nul") == 0)
        {
            stdoutNull = TRUE;
        }
        else
        {
            fwprintf(stderr, L"Unknown option '%s\n", argv[i]);
            return 1;
        }
    }

    for (; i < argc; i++)
    {
        if (cmd.GetLength() > 0)
            cmd.Append(L" ");

        if (wcschr(argv[i], L' ') || wcslen(argv[i]) == 0)
            cmd.AppendFormat(L"\"%s\"", argv[i]);
        else
            cmd.Append(argv[i]);
    }

    if (stdoutNull)
    {
        si.hStdOutput = nullFile;
    }

    wprintf(L"==== %s ====\n", cmd.GetString());

    // Snapshot state of additional processes if any.
    for (auto it = additionalProcess.begin(); it != additionalProcess.end(); ++it)
    {
        it->m_startCounters = ProcessCounters(it->m_hProcess);
    }


    CFileTime startTime = CFileTime::GetTickCount();
    for (i = 0; i < count; i++)
    {
        PROCESS_INFORMATION pi = { 0 };

        b = CreateProcess(NULL, cmd.GetBuffer(), NULL, NULL,
                          TRUE, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
        if (!b)
        {
            fwprintf(stderr, L"Failed to start process: %s\n",
                (LPCWSTR)AtlGetErrorDescription(HRESULT_FROM_WIN32(GetLastError())));
            return 1;
        }

        samplingProcess.push_back(SamplingProcessInfo(pi.hProcess));

        if (!concurrent)
        {
            WaitForSingleObject(pi.hProcess, INFINITE);
        }
    }

    if (concurrent)
    {
        for (auto it = samplingProcess.begin(); it != samplingProcess.end(); ++it)
        {
            WaitForSingleObject(it->m_hProcess, INFINITE);
        }
    }

    CFileTime endTime = CFileTime::GetTickCount();

    wprintf(L"\n== Performance of %s\n", cmd.GetString());

    for (auto it = samplingProcess.begin(); it != samplingProcess.end(); ++it)
    {
        it->m_Counters = ProcessCounters(it->m_hProcess);
    }

    // Snapshot state of additional processes if any.
    for (auto it = additionalProcess.begin(); it != additionalProcess.end(); ++it)
    {
        it->m_endCounters = ProcessCounters(it->m_hProcess);
    }

    CFileTimeSpan wallTime(endTime - startTime);

    std::list<CFileTimeSpan> realTime;
    std::list<CFileTimeSpan> cpuTime;
    std::list<CFileTimeSpan> userTime;
    std::list<CFileTimeSpan> kernelTime;

    std::list<ULONGLONG> readCount;
    std::list<ULONGLONG> readBytes;
    std::list<ULONGLONG> writeCount;
    std::list<ULONGLONG> writeBytes;
    std::list<ULONGLONG> otherCount;
    std::list<ULONGLONG> otherBytes;

    std::list<LONGLONG> peakWorkingSize;

    for (auto it = samplingProcess.begin(); it != samplingProcess.end(); ++it)
    {
        ProcessCounters & counters = it->m_Counters;

        realTime.push_back(counters.ExitTime() - counters.StartTime());
        cpuTime.push_back(counters.KernelTime() + counters.UserTime());
        userTime.push_back(counters.UserTime());
        kernelTime.push_back(counters.KernelTime());

        const IO_COUNTERS & ioCounters = counters.IOCounters();
        readCount.push_back(ioCounters.ReadOperationCount);
        readBytes.push_back(ioCounters.ReadTransferCount);
        writeCount.push_back(ioCounters.WriteOperationCount);
        writeBytes.push_back(ioCounters.WriteTransferCount);
        otherCount.push_back(ioCounters.OtherOperationCount);
        otherBytes.push_back(ioCounters.OtherTransferCount);

        const PROCESS_MEMORY_COUNTERS & memCounters = counters.MemCounters();
        peakWorkingSize.push_back(memCounters.PeakWorkingSetSize);
    }

    wprintf(L"   Real time: %s\n", FormatAvgMinMaxTime(realTime).GetString());
    wprintf(L"    CPU time: %s\n", FormatAvgMinMaxTime(cpuTime).GetString());
    wprintf(L"   User time: %s\n", FormatAvgMinMaxTime(userTime).GetString());
    wprintf(L" Kernel time: %s\n", FormatAvgMinMaxTime(kernelTime).GetString());

#if 0
    ULONG64 cpuCycleCount;
    QueryProcessCycleTime(pi.hProcess, &cpuCycleCount);
    printf(" CPU cycles: %lld\n", cpuCycleCount);
#endif

    wprintf(L"     Read IO: %s\n", FormatAvgMinMaxIO(readCount, readBytes).GetString());
    wprintf(L"    Write IO: %s\n", FormatAvgMinMaxIO(writeCount, writeBytes).GetString());
    wprintf(L"    Other IO: %s\n", FormatAvgMinMaxIO(otherCount, otherBytes).GetString());

    wprintf(L"Peak WS size: %s\n", FormatAvgMinMaxMem(peakWorkingSize).GetString());

    // Report state of additional processes if any.
    for (auto it = additionalProcess.begin(); it != additionalProcess.end(); ++it)
    {
        wprintf(L"\n== Performance of process %d (%s)\n", GetProcessId(it->m_hProcess), it->ImageFileName());

        CFileTimeSpan userTime = it->m_endCounters.UserTime() - it->m_startCounters.UserTime();
        CFileTimeSpan kernelTime = it->m_endCounters.KernelTime() - it->m_startCounters.KernelTime();
        wprintf(L"    CPU time: %8s s\n",
                FormatTime((userTime.GetTimeSpan() + kernelTime.GetTimeSpan())).GetString());
        wprintf(L"   User time: %8s s\n", FormatTime(userTime.GetTimeSpan()).GetString());
        wprintf(L" Kernel time: %8s s\n", FormatTime(kernelTime.GetTimeSpan()).GetString());

        IO_COUNTERS ioCounters;

        ioCounters.ReadOperationCount = it->m_endCounters.IOCounters().ReadOperationCount - it->m_startCounters.IOCounters().ReadOperationCount;
        ioCounters.WriteOperationCount = it->m_endCounters.IOCounters().WriteOperationCount - it->m_startCounters.IOCounters().WriteOperationCount;
        ioCounters.OtherOperationCount = it->m_endCounters.IOCounters().OtherOperationCount - it->m_startCounters.IOCounters().OtherOperationCount;
        ioCounters.ReadTransferCount = it->m_endCounters.IOCounters().ReadTransferCount - it->m_startCounters.IOCounters().ReadTransferCount;
        ioCounters.WriteTransferCount = it->m_endCounters.IOCounters().WriteTransferCount - it->m_startCounters.IOCounters().WriteTransferCount;
        ioCounters.OtherTransferCount = it->m_endCounters.IOCounters().OtherTransferCount - it->m_startCounters.IOCounters().OtherTransferCount;

        wprintf(L"     Read IO: %8lld (%lld KB)\n", ioCounters.ReadOperationCount, ioCounters.ReadTransferCount / 1024);
        wprintf(L"    Write IO: %8lld (%lld KB)\n", ioCounters.WriteOperationCount, ioCounters.WriteTransferCount / 1024);
        wprintf(L"    Other IO: %8lld (%lld KB)\n", ioCounters.OtherOperationCount, ioCounters.OtherTransferCount / 1024);
    }

    wprintf(L"\n");
    wprintf(L"  Total time: %8s s\n", FormatTime(wallTime.GetTimeSpan()).GetString());

    fnSetTimerResolution(5000, FALSE, &newRes);
    return 0;
}
