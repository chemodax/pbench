Add-Type @"
    using System;
    using System.Text;
    using System.Runtime.InteropServices;

    public class PInvoke
    {
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool GetProcessIoCounters(IntPtr hProcess, out IO_COUNTERS lpIoCounters);

        [StructLayout(LayoutKind.Sequential)]
        public struct IO_COUNTERS
        {
            public UInt64 ReadOperationCount;
            public UInt64 WriteOperationCount;
            public UInt64 OtherOperationCount;
            public UInt64 ReadTransferCount;
            public UInt64 WriteTransferCount;
            public UInt64 OtherTransferCount;
        };

        public static IO_COUNTERS GetProcessIoCounters (IntPtr handle)
        {
            bool rv = false;
            IO_COUNTERS counters;
            rv = GetProcessIoCounters(handle, out counters);

            return counters;
        }
    }
"@
<#
    Measure-Process
#>
function Measure-Process
{
    [Alias("pbench")]
    Param
    (
        [Parameter(Position = 0, Mandatory = $true)]
        [ValidateNotNullOrEmpty()]
        [string]
        $FilePath,
        [Parameter(Position = 1, Mandatory = $false)]
        [ValidateNotNullOrEmpty()]
        [string[]]
        $ArgumentList
    )
    process
    {
        $pinfo = New-Object System.Diagnostics.ProcessStartInfo
        $pinfo.FileName = $FilePath
        $pinfo.Arguments = $ArgumentList
        $pinfo.RedirectStandardError = $false
        $pinfo.RedirectStandardOutput = $false
        $pinfo.UseShellExecute = $false
        $pinfo.CreateNoWindow = $true
        $p = New-Object System.Diagnostics.Process
        $p.StartInfo = $pinfo

        Write-Verbose "Starting process '$FilePath'"
        $r = $p.Start()
        $p.WaitForExit()
        $p.Refresh()

        $io = [PInvoke]::GetProcessIoCounters($p.Handle)

        $result = [pscustomobject]@{
            TotalTime        = $p.ExitTime - $p.StartTime;
            TotalCPU         = $p.TotalProcessorTime;
            ReadCount        = $io.ReadOperationCount;
            ReadBytes        = $io.ReadTransferCount;
            WriteCount       = $io.WriteOperationCount;
            WriteBytes       = $io.WriteTransferCount;
        }

        $result.PSObject.TypeNames.Insert( 0, "PBench.ProcessStats" )

        $result
    }
}
