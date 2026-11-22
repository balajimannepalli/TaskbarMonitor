# Run on an interactive desktop after installing Setup with its startup task selected.
[CmdletBinding()]
param([string]$InstalledExe="$env:ProgramFiles\Taskbar Monitor\TaskbarMonitor.exe")
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class PackageWindow {
    public delegate bool Visit(IntPtr h, IntPtr p);
    [DllImport("user32")] static extern bool EnumWindows(Visit cb, IntPtr p);
    [DllImport("user32")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
    [DllImport("user32",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,System.Text.StringBuilder s,int n);
    public static IntPtr Find(int process, string cls) {
        IntPtr found=IntPtr.Zero;
        EnumWindows(delegate(IntPtr h,IntPtr p) {
            uint pid; GetWindowThreadProcessId(h,out pid);
            if(pid!=process)return true;
            var name=new System.Text.StringBuilder(80); GetClassName(h,name,80);
            if(name.ToString()!=cls)return true;
            found=h; return false;
        },IntPtr.Zero);
        return found;
    }
    [DllImport("user32")] public static extern bool PostMessage(IntPtr w,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32")] public static extern IntPtr GetDlgItem(IntPtr w,int id);
    [DllImport("user32")] public static extern bool IsWindowEnabled(IntPtr w);
    [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr w);
    [DllImport("user32",CharSet=CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,System.Text.StringBuilder text);
    [DllImport("user32",CharSet=CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,string text);
    public static string ReadFont(IntPtr dlg) {
        var text=new System.Text.StringBuilder(32);
        SendMessage(GetDlgItem(dlg,207),13,(IntPtr)text.Capacity,text);
        return text.ToString();
    }
    public static bool SetFont(IntPtr dlg,string text) {
        return SendMessage(GetDlgItem(dlg,207),12,IntPtr.Zero,text)!=IntPtr.Zero;
    }
}
"@
function Wait-Window($process,$class) {
    $timer=[Diagnostics.Stopwatch]::StartNew()
    do {
        if ($process.HasExited) {throw "Process exited before creating $class"}
        $w=[PackageWindow]::Find($process.Id,$class)
        if($w -ne [IntPtr]::Zero -and ($class -ne '#32770' -or [PackageWindow]::IsWindowVisible($w))){return $w}
        Start-Sleep -Milliseconds 80
    } while ($timer.Elapsed.TotalSeconds -lt 15)
    throw "Timed out waiting for $class"
}
function Close-App($process,$class) {
    $dialog=[PackageWindow]::Find($process.Id,'#32770')
    if($dialog -ne [IntPtr]::Zero){[void][PackageWindow]::PostMessage($dialog,0x10,0,0); Start-Sleep -Milliseconds 200}
    $window=Wait-Window $process $class
    [void][PackageWindow]::PostMessage($window,0x10,0,0)
    if(!$process.WaitForExit(10000) -or $process.ExitCode -ne 0){throw 'Shutdown failed'}
}
function RegistryStartup {
    (Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name TaskbarMonitor -ErrorAction SilentlyContinue).TaskbarMonitor
}
if (Get-Process TaskbarMonitor -ErrorAction SilentlyContinue){throw 'Close existing monitors before the test'}
$root=(Resolve-Path "$PSScriptRoot\..").Path
$temp=Join-Path $root ('build\portable-test-'+[guid]::NewGuid().ToString('N'))
Expand-Archive -LiteralPath "$root\release\TaskbarMonitor-Portable-x64.zip" -DestinationPath $temp
$portable="$temp\TaskbarMonitor.exe"
$config=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'TaskbarMonitor\config.json'
$installedHash=if(Test-Path $config){(Get-FileHash $config).Hash}
$priorStartup=RegistryStartup
$apps=@()
try {
    $installed=Start-Process $InstalledExe -ArgumentList '--settings' -PassThru
    $apps+=$installed
    $dlg=Wait-Window $installed '#32770'
    if(![PackageWindow]::IsWindowEnabled([PackageWindow]::GetDlgItem($dlg,217))){throw 'Installed startup disabled'}
    if($installedHash -and [PackageWindow]::ReadFont($dlg) -ne (Get-Content $config -Raw | ConvertFrom-Json).fontSize){throw "Expected $((Get-Content $config -Raw | ConvertFrom-Json).fontSize) read $([PackageWindow]::ReadFont($dlg))"}
    [void][PackageWindow]::PostMessage($dlg,0x10,0,0)
    $copy=Start-Process $portable -ArgumentList '--settings' -PassThru
    $apps+=$copy
    $dlg=Wait-Window $copy '#32770'
    if([PackageWindow]::IsWindowEnabled([PackageWindow]::GetDlgItem($dlg,217))){throw 'Portable startup enabled'}
    if(![PackageWindow]::SetFont($dlg,'16')){throw 'Portable font edit failed'}
    [void][PackageWindow]::PostMessage($dlg,0x111,1,0)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while ([PackageWindow]::Find($copy.Id,'#32770') -ne [IntPtr]::Zero -and $timer.Elapsed.TotalSeconds -lt 15){Start-Sleep -Milliseconds 100}
    if([PackageWindow]::Find($copy.Id,'#32770') -ne [IntPtr]::Zero){throw 'Portable save did not finish'}
    Close-App $copy 'TaskbarMonitor.Portable'
    if((Get-Content "$temp\Data\config.json" -Raw | ConvertFrom-Json).fontSize -ne 16){throw 'Portable save did not persist'}
    $copy=Start-Process $portable -ArgumentList '--settings' -PassThru
    $apps+=$copy
    $dlg=Wait-Window $copy '#32770'
    if([PackageWindow]::ReadFont($dlg) -ne 16){throw 'Portable setting lost after restart'}
    Close-App $copy 'TaskbarMonitor.Portable'
    if($installedHash -and (Get-FileHash $config).Hash -ne $installedHash){throw 'Portable changed installed settings'}
    if((RegistryStartup) -ne $priorStartup){throw 'Startup changed during portable test'}
    Close-App $installed 'TaskbarMonitor.Phase0'
    Write-Output 'Installed and extracted portable first launch, independent windows, portable Save/relaunch, installed setting load, registry and configuration isolation, and clean exit passed.'
} finally {
    foreach ($app in $apps){if(!$app.HasExited){$app.Kill();$app.WaitForExit()};$app.Dispose()}
}


