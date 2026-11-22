# Run from an interactive Windows desktop with a visible horizontal taskbar.
[CmdletBinding()]
param(
    [string]$Exe = "$PSScriptRoot\..\build\Release\TaskbarMonitor.exe",
    [ValidateRange(2,3600)][int]$Seconds = 30,
    [switch]$RestartExplorer,
    [switch]$RevealBottomTaskbar,
    [string]$CaptureDirectory,
    [switch]$SettingsOpen,
    [switch]$ExerciseSettings,
    [switch]$NormalMode,
    [switch]$SelectPrimaryMonitor
)
$ErrorActionPreference = 'Stop'
if (-not ('Phase0Desktop' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class Phase0Desktop {
    [StructLayout(LayoutKind.Sequential)] public struct Rect {
        public int Left, Top, Right, Bottom;
        public override string ToString() { return String.Format("({0},{1})-({2},{3})",Left,Top,Right,Bottom); }
    }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
    [DllImport("user32", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c, string n);
    [DllImport("user32", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
    [DllImport("user32")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
    [DllImport("user32")] public static extern IntPtr GetDlgItem(IntPtr w,int id);
    [DllImport("user32",CharSet=CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr w,uint msg,IntPtr arg,string text);
    [DllImport("user32",CharSet=CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr w,uint msg,IntPtr arg,System.Text.StringBuilder text);
    public static void SetDlgItemText(IntPtr w,int id,string s) {
        IntPtr control=GetDlgItem(w,id);
        SendMessage(control,0x0c,IntPtr.Zero,s);
        var read=new System.Text.StringBuilder(128);
        SendMessage(control,0x0d,(IntPtr)128,read);
        if(read.ToString()!=s) throw new InvalidOperationException("Could not edit field "+id);
    }
    [DllImport("user32",CharSet=CharSet.Unicode)] public static extern IntPtr SendDlgItemMessage(IntPtr w,int id,uint msg,IntPtr wp,IntPtr lp);
    public static IntPtr Overlay() { return FindWindow("TaskbarMonitor.Phase0",null); }
    [DllImport("user32", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr p, IntPtr after, string c, string n);
    [DllImport("user32")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32")] public static extern bool GetCursorPos(out Point p);
    [DllImport("user32")] public static extern bool SetCursorPos(int x,int y);
    [DllImport("user32")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr w);
    [DllImport("user32")] public static extern uint GetDpiForWindow(IntPtr w);
    [DllImport("user32")] public static extern bool GetWindowRect(IntPtr w, out Rect r);
    [DllImport("user32")] public static extern uint GetWindowThreadProcessId(IntPtr w, out uint pid);
    [DllImport("user32")] public static extern IntPtr WindowFromPoint(Point p);
    [DllImport("user32")] static extern IntPtr SendMessageTimeout(IntPtr w,uint m,IntPtr wp,IntPtr lp,uint flags,uint ms,out IntPtr result);
    public static long Send(IntPtr w,uint msg) {
        IntPtr result;
        if (SendMessageTimeout(w,msg,IntPtr.Zero,IntPtr.Zero,2,2000,out result)==IntPtr.Zero)
            throw new InvalidOperationException("Window did not respond: "+msg);
        return result.ToInt64();
    }
    public static Rect Bounds(IntPtr w) {
        Rect r;
        if (!GetWindowRect(w,out r)) throw new InvalidOperationException("Window geometry unavailable");
        return r;
    }
    public static uint Owner(IntPtr w) { uint id; GetWindowThreadProcessId(w,out id); return id; }
    public static bool TopAtCenter(IntPtr w,Rect r) {
        return WindowFromPoint(new Point { X=(r.Left+r.Right)/2,Y=(r.Top+r.Bottom)/2 })==w;
    }
    [StructLayout(LayoutKind.Sequential)] struct MouseInput {
        public int x,y;
        public uint data,flags,time;
        public UIntPtr extra;
    }
    [StructLayout(LayoutKind.Sequential)] struct Input { public uint type; public MouseInput mouse; }
    [DllImport("user32")] static extern uint SendInput(uint count,Input[] inputs,int size);
    public static void ClickSample(IntPtr w,Rect r) {
        int x=(r.Left+r.Right)/2, y=(r.Top+r.Bottom)/2;
        Point cursor;
        if (!SetCursorPos(x,y) || !GetCursorPos(out cursor) || cursor.X!=x || cursor.Y!=y ||
            WindowFromPoint(cursor)!=w) throw new InvalidOperationException("Sample is not under the pointer");
        Input down=new Input(), up=new Input();
        down.mouse.flags=2;
        up.mouse.flags=4;
        if (SendInput(2,new Input[]{down,up},Marshal.SizeOf(typeof(Input)))!=2)
            throw new InvalidOperationException("Mouse click failed");
    }
}
'@
}
function Find-Primary { [Phase0Desktop]::FindWindow('Shell_TrayWnd', $null) }
function Save-Taskbar($Name,$Bar) {
    if (!$CaptureDirectory) { return }
    Add-Type -AssemblyName System.Drawing
    $r = [Phase0Desktop]::Bounds($Bar)
    $bitmap = [Drawing.Bitmap]::new($r.Right-$r.Left,$r.Bottom-$r.Top)
    try {
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try { $graphics.CopyFromScreen($r.Left,$r.Top,0,0,$bitmap.Size) }
        finally { $graphics.Dispose() }
        $bitmap.Save((Join-Path (Resolve-Path $CaptureDirectory) $Name),[Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }
}
function Test-StablePixels($Window) {
    Add-Type -AssemblyName System.Drawing
    # The first submitted frame can precede the compositor's window-show transition.
    Start-Sleep -Milliseconds 750
    $r = [Phase0Desktop]::Bounds($Window)
    $bitmap = [Drawing.Bitmap]::new($r.Right-$r.Left,$r.Bottom-$r.Top)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $bytes = [IO.MemoryStream]::new()
    try {
        $previous = $null
        for ($i=0; $i -lt 20; $i++) {
            $graphics.CopyFromScreen($r.Left,$r.Top,0,0,$bitmap.Size)
            $bytes.SetLength(0)
            $bitmap.Save($bytes,[Drawing.Imaging.ImageFormat]::Bmp)
            $current = [Convert]::ToBase64String($bytes.ToArray())
            if ($previous -and $current -cne $previous) { throw 'Visible sample changed during idle frame check' }
            $previous = $current
            Start-Sleep -Milliseconds 100
        }
    } finally {
        $bytes.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    Write-Output '20 desktop samples at 100 ms intervals had identical overlay pixels.'
}
function Assert-Placement($Window, $Bar) {
    $bounds = [Phase0Desktop]::Bounds($Bar)
    $r = [Phase0Desktop]::Bounds($Window)
    if (-not [Phase0Desktop]::IsWindowVisible($Window) -or $r.Left -lt $bounds.Left -or
        $r.Right -gt $bounds.Right -or $r.Top -lt $bounds.Top -or $r.Bottom -gt $bounds.Bottom -or
        $r.Right -le $r.Left -or $r.Bottom -le $r.Top -or ![Phase0Desktop]::TopAtCenter($Window,$r)) {
        throw "Overlay $r is hidden, covered, or outside taskbar $bounds"
    }
    return $r
}
function Wait-Until([scriptblock]$Condition, [string]$Failure) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    do {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 100
    } while ($timer.Elapsed.TotalSeconds -lt 20)
    throw $Failure
}
$proc = $null
$window = [IntPtr]::Zero
$oldDpi = [Phase0Desktop]::SetThreadDpiAwarenessContext([IntPtr](-4))
$savedCursor = New-Object Phase0Desktop+Point
$restoreCursor = $false
try {
    $bar = Find-Primary
    if (!$bar.ToInt64()) { throw 'No primary taskbar on this desktop' }
    if ($RevealBottomTaskbar) {
        $restoreCursor = [Phase0Desktop]::GetCursorPos([ref]$savedCursor)
        if (!$restoreCursor) { throw 'Cannot save cursor position' }
        $edgeY = [Phase0Desktop]::GetSystemMetrics(1) - 1
        if (![Phase0Desktop]::SetCursorPos(160,$edgeY)) { throw 'Cannot reach screen edge' }
        Wait-Until { [Phase0Desktop]::Bounds($bar).Bottom -le $edgeY + 1 } 'Bottom taskbar did not reveal'
    }
    Save-Taskbar 'taskbar-revealed.png' $bar
    $secondary = [IntPtr]::Zero
    do {
        $secondary = [Phase0Desktop]::FindWindowEx([IntPtr]::Zero,$secondary,'Shell_SecondaryTrayWnd',$null)
        if ($secondary.ToInt64()) { Write-Output "Secondary $secondary $([Phase0Desktop]::Bounds($secondary))" }
    } while ($secondary.ToInt64())
    Write-Output "Primary $bar $([Phase0Desktop]::Bounds($bar)), DPI $([Phase0Desktop]::GetDpiForWindow($bar))"
    if ([Phase0Desktop]::Overlay().ToInt64()) {
        throw 'Close the existing prototype before running the isolated smoke test'
    }
    $before = [Phase0Desktop]::GetForegroundWindow()
    $launch = @{ FilePath=(Resolve-Path $Exe).Path; PassThru=$true }
    if (!$NormalMode) { $launch.ArgumentList='--phase0-test' }
    if ($SelectPrimaryMonitor) {
        Add-Type -AssemblyName System.Windows.Forms
        $missing=Start-Process -FilePath $launch.FilePath -ArgumentList '--phase0-test --monitor missing-desktop-test-device' -PassThru
        try {
            Wait-Until { [Phase0Desktop]::Owner([Phase0Desktop]::Overlay()) -eq $missing.Id } 'Missing-monitor instance failed to initialize'
            $hidden=[Phase0Desktop]::Overlay()
            [void][Phase0Desktop]::Send($hidden,0x8001)
            if ([Phase0Desktop]::IsWindowVisible($hidden)) {throw 'Missing monitor fell back to a visible overlay'}
            [void][Phase0Desktop]::Send($hidden,0x10)
            if (!$missing.WaitForExit(3000) -or $missing.ExitCode -ne 0) {throw 'Hidden overlay did not shut down'}
        } finally {
            if (!$missing.HasExited) { $missing.Kill() }
            $missing.Dispose()
        }
        $device=[Windows.Forms.Screen]::PrimaryScreen.DeviceName
        $launch.ArgumentList=@($launch.ArgumentList | Where-Object {$_}) + @('--monitor',$device)
        Write-Output "Explicit monitor selection $device"
    }
    $proc = Start-Process @launch
    Wait-Until {
        if ($proc.HasExited) { throw "Prototype exited with $($proc.ExitCode)" }
        $w = [Phase0Desktop]::Overlay()
        [Phase0Desktop]::Owner($w) -eq $proc.Id -and [Phase0Desktop]::IsWindowVisible($w) -and
            [Phase0Desktop]::Send($w,0x8002) -gt 0
    } 'No painted visible overlay (check auto-hide, fullscreen and display geometry)'
    $window = [Phase0Desktop]::Overlay()
    if ($ExerciseSettings) {
        function Open-Settings {
            [void][Phase0Desktop]::PostMessage($window,0x8005,[IntPtr]::Zero,[IntPtr]::Zero)
            Wait-Until { [Phase0Desktop]::IsWindowVisible([Phase0Desktop]::FindWindow("#32770","Taskbar Monitor Settings")) } "Settings did not open"
            return [Phase0Desktop]::FindWindow("#32770","Taskbar Monitor Settings")
        }
        function Click-Setting($dlg,$id) {
            $control=[Phase0Desktop]::GetDlgItem($dlg,$id)
            if (!$control.ToInt64()) {throw "Missing settings control $id"}
            [void][Phase0Desktop]::Send($control,0xF5)
        }
        $configPath=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'TaskbarMonitor\config.json'
        $beforeSettings=[IO.File]::ReadAllText($configPath)
        $dlg=Open-Settings
        Click-Setting $dlg 218
        Click-Setting $dlg 2
        Wait-Until { ![Phase0Desktop]::FindWindow("#32770","Taskbar Monitor Settings").ToInt64() } "Cancel did not close settings"
        if([IO.File]::ReadAllText($configPath) -cne $beforeSettings){throw 'Cancel persisted the defaults draft'}
        $dlg=Open-Settings
        $wasStartup=[Phase0Desktop]::Send([Phase0Desktop]::GetDlgItem($dlg,217),0xF0)
        Click-Setting $dlg 218
        if($wasStartup){Click-Setting $dlg 217}
        [void][Phase0Desktop]::SetDlgItemText($dlg,207,'99')
        Click-Setting $dlg 1
        if(![Phase0Desktop]::IsWindowVisible($dlg) -or [IO.File]::ReadAllText($configPath) -cne $beforeSettings){throw 'Invalid font size accepted'}
        [void][Phase0Desktop]::SetDlgItemText($dlg,207,'14')
        [void][Phase0Desktop]::SetDlgItemText($dlg,206,'Consolas')
        [void][Phase0Desktop]::SendDlgItemMessage($dlg,205,0x14E,[IntPtr]3,[IntPtr]::Zero)
        [void][Phase0Desktop]::SetDlgItemText($dlg,208,'#EEFFEE')
        [void][Phase0Desktop]::SetDlgItemText($dlg,209,'#173321')
        [void][Phase0Desktop]::SetDlgItemText($dlg,220,'800')
        Click-Setting $dlg 204
        Click-Setting $dlg 1
        Wait-Until { ![Phase0Desktop]::IsWindowVisible($dlg) } 'Settings save did not finish'
        $saved=Get-Content -Raw $configPath | ConvertFrom-Json
        if($saved.fontFamily -ne 'Consolas' -or $saved.fontSize -ne 14 -or $saved.metricOrder[0] -ne 'upload' -or
           $saved.theme -ne 'custom' -or $saved.refreshIntervals.network -ne 800){throw 'Saved settings did not match draft'}
        Write-Output 'Defaults, Cancel, invalid Save, and live custom appearance/order/interval Save passed.'
        if ($RevealBottomTaskbar) {
            [void][Phase0Desktop]::SetCursorPos(160,$edgeY-200)
            Wait-Until { [Phase0Desktop]::Bounds($bar).Bottom -gt $edgeY+1 } 'Taskbar did not retract after editing'
            [void][Phase0Desktop]::SetCursorPos(160,$edgeY)
            Wait-Until { [Phase0Desktop]::Bounds($bar).Bottom -eq $edgeY+1 -and [Phase0Desktop]::IsWindowVisible($window) } 'Taskbar did not return after editing'
        }
    }
    $duplicate = Start-Process -FilePath (Resolve-Path $Exe).Path -ArgumentList '--phase0-test' -PassThru
    try {
        if (!$duplicate.WaitForExit(5000) -or $duplicate.ExitCode -ne 0) { throw 'Duplicate launch failed' }
        if ([Phase0Desktop]::Owner([Phase0Desktop]::Overlay()) -ne $proc.Id) { throw 'Primary instance changed' }
    } finally {
        if (!$duplicate.HasExited) { $duplicate.Kill() }
        $duplicate.Dispose()
    }
    Write-Output 'Second process exited successfully and retained the original overlay.'
    $first = Assert-Placement $window $bar
    if ($SelectPrimaryMonitor) {
        # Deliver notifications to the test window only. Physical mode stays unchanged.
        $shellMessages=[Phase0Desktop]::Send($window,0x8003)
        [void][Phase0Desktop]::Send($window,0x007e)
        [void][Phase0Desktop]::Send($window,[Phase0Desktop]::RegisterWindowMessage('TaskbarCreated'))
        Wait-Until { [Phase0Desktop]::Send($window,0x8003) -gt $shellMessages } 'Shell notification not received'
        [void][Phase0Desktop]::Send($window,0x8001)
        $resynced=Assert-Placement $window $bar
        if ($first.ToString() -ne $resynced.ToString()) {throw 'Bounds changed after rediscovery'}
        Write-Output 'Targeted display and shell notifications retained placement on the selected monitor.'
    }
    if ($NormalMode) { Start-Sleep -Seconds 3 } else { Test-StablePixels $window }
    Save-Taskbar 'taskbar-overlay.png' $bar
    $after = [Phase0Desktop]::GetForegroundWindow()
    if ($after -eq $window -or [Phase0Desktop]::Send($window,0x21) -ne 3) { throw 'Activation check failed' }
    if ($RevealBottomTaskbar) {
        $focus = [Phase0Desktop]::GetForegroundWindow()
        [Phase0Desktop]::ClickSample($window,$first)
        Wait-Until { [Phase0Desktop]::Bounds($window).Left -ne $first.Left } 'Pointer click did not reposition sample'
        if ([Phase0Desktop]::GetForegroundWindow() -ne $focus) { throw 'Mouse click changed foreground window' }
        [void][Phase0Desktop]::SetCursorPos(160,$edgeY)
        Write-Output 'Actual pointer click preserved foreground focus.'
    } else {
        [void][Phase0Desktop]::Send($window,0x0202)
    }
    $second = Assert-Placement $window $bar
    if ($first.Left -eq $second.Left -or [Phase0Desktop]::GetForegroundWindow() -eq $window) {
        throw 'Reposition or focus check failed'
    }
    [void][Phase0Desktop]::Send($window,0x0202)
    Write-Output "Placement $first -> $second. Focus before/after launch $before/$after. MA_NOACTIVATE passed."
    Start-Sleep -Seconds 2
    if ($SettingsOpen) {
        [void][Phase0Desktop]::PostMessage($window,0x8005,[IntPtr]::Zero,[IntPtr]::Zero)
        Wait-Until { [Phase0Desktop]::IsWindowVisible([Phase0Desktop]::FindWindow("#32770","Taskbar Monitor Settings")) } "Settings did not open"
        Start-Sleep -Milliseconds 900
        Save-Taskbar 'settings-window.png' ([Phase0Desktop]::FindWindow("#32770","Taskbar Monitor Settings"))
    }
    $paints = [Phase0Desktop]::Send($window,0x8002)
    $changes = [Phase0Desktop]::Send($window,0x8004)
    $scheduler = [Phase0Desktop]::Send($window,0x8006)
    $proc.Refresh()
    [uint32]$windowProcess=0
    $uiId=[Phase0Desktop]::GetWindowThreadProcessId($window,[ref]$windowProcess)
    $uiBefore=($proc.Threads | Where-Object Id -eq $uiId).TotalProcessorTime
    $cpu = $proc.TotalProcessorTime
    $working = $proc.WorkingSet64
    $private = $proc.PrivateMemorySize64
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $resourceSamples = @(
        for ($remaining=$Seconds; ; $remaining-=$delay) {
            $proc.Refresh()
            if ($proc.HasExited) {throw 'Overlay exited during sampling'}
            [pscustomobject]@{Handles=$proc.HandleCount; Threads=$proc.Threads.Count;
                Working=$proc.WorkingSet64; Private=$proc.PrivateMemorySize64}
            if ($remaining -eq 0) {break}
            $delay=[Math]::Min(10,$remaining)
            Start-Sleep -Seconds $delay
        }
    )
    $proc.Refresh()
    $consumed = ($proc.TotalProcessorTime - $cpu).TotalMilliseconds
    $elapsed = $clock.Elapsed.TotalMilliseconds
    $uiAfter=($proc.Threads | Where-Object Id -eq $uiId).TotalProcessorTime
    Write-Output ("Overlay UI thread CPU: {0:N2} ms" -f ($uiAfter-$uiBefore).TotalMilliseconds)
    foreach ($property in 'Handles','Threads','Working','Private') {
        $range=$resourceSamples | Measure-Object -Property $property -Minimum -Maximum
        Write-Output "$property start/min/max/end: $($resourceSamples[0].$property) / $($range.Minimum) / $($range.Maximum) / $($resourceSamples[-1].$property)"
    }
    $repaints = [Phase0Desktop]::Send($window,0x8002) - $paints
    Write-Output ("Idle {0:N2}s, CPU {1:N2} ms ({2:N4}% of one core), working set {3:N2} -> {4:N2} MiB, private bytes {5:N2} -> {6:N2} MiB, additional paints {7}" -f ($elapsed/1000),$consumed,(100*$consumed/$elapsed),($working/1MB),($proc.WorkingSet64/1MB),($private/1MB),($proc.PrivateMemorySize64/1MB),$repaints)
    $updates = [Phase0Desktop]::Send($window,0x8004) - $changes
    Write-Output "Displayed text changes: $updates"
    Write-Output "Scheduler overhead (us): $([Phase0Desktop]::Send($window,0x8006)-$scheduler)"
    if ($SettingsOpen) { [void][Phase0Desktop]::Send([Phase0Desktop]::FindWindow("#32770","Taskbar Monitor Settings"),0x10) }
    if ($repaints -gt $updates+2) { throw 'Unexpected recurring paints during the idle sample' }
    if ($RevealBottomTaskbar) {
        [void][Phase0Desktop]::SetCursorPos(160,$edgeY-200)
        Wait-Until {
            [Phase0Desktop]::Bounds($bar).Bottom -gt $edgeY+1 -and ![Phase0Desktop]::IsWindowVisible($window)
        } 'Auto-hide did not retract both the taskbar and overlay'
        [void][Phase0Desktop]::SetCursorPos(160,$edgeY)
        Wait-Until { [Phase0Desktop]::IsWindowVisible($window) } 'Overlay did not return on taskbar reveal'
        [void](Assert-Placement $window $bar)
        Write-Output 'Auto-hide retract and reveal passed.'
    }
    if ($RestartExplorer) {
        $oldId = [Phase0Desktop]::Owner($bar)
        $explorer = [Diagnostics.Process]::GetProcessById($oldId)
        if ($explorer.ProcessName -ne 'explorer' -or $explorer.SessionId -ne $proc.SessionId) {
            throw 'Taskbar owner is not Explorer in the test session'
        }
        $messages = [Phase0Desktop]::Send($window,0x8003)
        Stop-Process -InputObject $explorer -Force
        if (!$explorer.WaitForExit(5000)) { throw 'Explorer did not exit' }
        # Give the Windows shell watchdog first opportunity to relaunch it.
        Start-Sleep -Seconds 4
        if (!(Find-Primary).ToInt64()) {
            Start-Process -FilePath "$env:WINDIR\explorer.exe" -WindowStyle Hidden
        }
        Wait-Until { (Find-Primary).ToInt64() } 'Replacement shell has no primary taskbar'
        if ($RevealBottomTaskbar) {
            [void][Phase0Desktop]::SetCursorPos(160,$edgeY-200)
            Start-Sleep -Milliseconds 300
            [void][Phase0Desktop]::SetCursorPos(160,$edgeY)
        }
        Wait-Until {
            $newBar = Find-Primary
            $newId = [Phase0Desktop]::Owner($newBar)
            $newId -and $newId -ne $oldId -and [Phase0Desktop]::Send($window,0x8003) -gt $messages -and
                [Phase0Desktop]::IsWindowVisible($window)
        } 'Live shell replacement or TaskbarCreated reception failed'
        $newBar = Find-Primary
        $recovered = Assert-Placement $window $newBar
        Write-Output "Recovery Explorer PID $oldId -> $([Phase0Desktop]::Owner($newBar)); taskbar $bar -> $newBar; overlay PID $($proc.Id) at $recovered."
    }
    [void][Phase0Desktop]::Send($window,$(if ($NormalMode){0x10}else{0x0205}))
    if (!$proc.WaitForExit(3000) -or $proc.ExitCode -ne 0) { throw 'Right-click shutdown failed' }
    Write-Output 'Clean shutdown passed.'
} finally {
    if ($restoreCursor) { [void][Phase0Desktop]::SetCursorPos($savedCursor.X,$savedCursor.Y) }
    if ($proc -and !$proc.HasExited) {
        if ($window.ToInt64() -and [Phase0Desktop]::Owner($window) -eq $proc.Id) {
            [void][Phase0Desktop]::Send($window,0x10)
        }
        if (!$proc.WaitForExit(2000)) { $proc.Kill() }
    }
    if ($proc) { $proc.Dispose() }
    [void][Phase0Desktop]::SetThreadDpiAwarenessContext($oldDpi)
}

