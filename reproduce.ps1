# Private editor/reviewer reproduction, Windows PowerShell 5.1 or PowerShell 7.
# Builds from source; requires a C++14 compiler and Python 3.9+ (standard library).
# No download, network request, installation, or permanent environment change.
[CmdletBinding()]
param(
    [string]$OutputDirectory = '',
    [string]$Python = '',
    [ValidateSet('Auto', 'MSVC', 'GCC')][string]$Compiler = 'Auto',
    [switch]$ChecksOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$packageRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$dataDirectory = Join-Path $packageRoot 'reference\exact'
$programNames = @('roman_torus_exact', 'verify_exact_results', 'verify_manuscript',
    'verify_phase_barrier', 'bridge_all_lengths', 'check_type4_12_8')
$csvFields = @('m', 'n', 'gamma_R', 'projection_lower_bound', 'excess_times_6', 'status')
$utf8 = New-Object Text.UTF8Encoding($false)
$summaryLines = New-Object 'System.Collections.Generic.List[string]'
$logDirectory = $null
$runDirectory = $null
$watch = [Diagnostics.Stopwatch]::StartNew()

function Assert-Condition([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Add-Summary([string]$Message) {
    Write-Host $Message
    $script:summaryLines.Add($Message)
    if ($null -ne $script:logDirectory) {
        [IO.File]::WriteAllLines((Join-Path $script:logDirectory 'SUMMARY.txt'), $script:summaryLines, $script:utf8)
    }
}
function Get-Sha256([string]$Path) {
    # .NET is available in both Windows PowerShell 5.1 and PowerShell 7; this
    # avoids optional-module discovery differences between the two launchers.
    $algorithm = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $stream.Dispose(); $algorithm.Dispose() }
}
function Quote-NativeArgument([string]$Text) {
    # Windows CRT argument quoting, including embedded quotes and final slashes.
    $quoted = [regex]::Replace($Text, '(\\*)"', {
        param($match)
        return ('\' * (2 * $match.Groups[1].Length)) + '\"'
    })
    $quoted = [regex]::Replace($quoted, '(\\+)$', '$1$1')
    return '"' + $quoted + '"'
}
function Invoke-Native {
    param([string]$Executable, [string[]]$ProgramArguments = @(),
        [string]$WorkingDirectory = $packageRoot, [string]$LogName = '', [switch]$AllowFailure)
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $Executable
    $start.WorkingDirectory = $WorkingDirectory
    $start.Arguments = (($ProgramArguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' ')
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    Assert-Condition ($process.Start()) "Cannot start $Executable."
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    $nativeExitCode = $process.ExitCode
    $process.Dispose()
    $text = $stdout
    if ($stderr.Length -gt 0) { $text += "`r`n[standard error]`r`n" + $stderr }
    if ($LogName.Length -gt 0) {
        [IO.File]::WriteAllText((Join-Path $logDirectory $LogName), $text, $utf8)
    }
    if (-not $AllowFailure) {
        Assert-Condition ($nativeExitCode -eq 0) "Program failed with exit code $nativeExitCode`: $Executable. See $LogName."
    }
    return [pscustomobject]@{ Code = $nativeExitCode; Text = $text; Stdout = $stdout }
}
function Find-Python {
    $candidates = @()
    if ($Python.Length -gt 0) {
        $command = Get-Command $Python -ErrorAction SilentlyContinue
        Assert-Condition ($null -ne $command) "Python executable not found: $Python"
        $candidates += ,@($command.Source, @())
    } else {
        foreach ($name in @('py.exe', 'python.exe', 'python3.exe')) {
            $command = Get-Command $name -ErrorAction SilentlyContinue
            # Windows Store aliases can launch the Store instead of Python.
            if ($null -ne $command -and $command.Source -notmatch '\\Microsoft\\WindowsApps\\') {
                $prefix = @()
                if ($name -eq 'py.exe') { $prefix = @('-3') }
                $candidates += ,@($command.Source, $prefix)
            }
        }
        foreach ($registryRoot in @('HKCU:\Software\Python\PythonCore','HKLM:\Software\Python\PythonCore')) {
            if (Test-Path -LiteralPath $registryRoot) {
                foreach ($versionKey in Get-ChildItem -LiteralPath $registryRoot -ErrorAction SilentlyContinue) {
                    $installKey = Join-Path $versionKey.PSPath 'InstallPath'
                    if (Test-Path -LiteralPath $installKey) {
                        $properties = Get-ItemProperty -LiteralPath $installKey
                        if ($properties.PSObject.Properties.Name -contains 'ExecutablePath' -and (Test-Path -LiteralPath $properties.ExecutablePath -PathType Leaf)) {
                            $candidates += ,@($properties.ExecutablePath, @())
                        }
                    }
                }
            }
        }
        # Reuse an already installed Codex document runtime when present on the
        # author's machine; this is optional and never downloaded or required.
        $documentRuntime = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
        if (Test-Path -LiteralPath $documentRuntime -PathType Leaf) { $candidates += ,@($documentRuntime, @()) }
    }
    foreach ($candidate in $candidates) {
        $args = @($candidate[1]) + @('-c', 'import sys; print(sys.version); sys.exit(0 if sys.version_info >= (3,9) else 1)')
        try {
            $result = Invoke-Native -Executable $candidate[0] -ProgramArguments $args -AllowFailure
            if ($result.Code -eq 0) {
                return [pscustomobject]@{ Executable = $candidate[0]; Prefix = @($candidate[1]); Version = $result.Stdout.Trim() }
            }
        } catch { }
    }
    throw 'Python 3.9+ is required for the table and manuscript checks. Install Python separately, or supply -Python "C:\path\python.exe". No packages are needed.'
}
function Find-Compiler {
    if ($Compiler -ne 'GCC') {
        $cl = Get-Command 'cl.exe' -ErrorAction SilentlyContinue
        if ($null -eq $cl -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
            $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
            if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
                $found = Invoke-Native -Executable $vswhere -ProgramArguments @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath')
                $installation = $found.Stdout.Trim()
                if ($installation.Length -gt 0) {
                    $developerScript = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
                    if (Test-Path -LiteralPath $developerScript -PathType Leaf) {
                        # This child cmd only reads the developer environment. Import it
                        # into this PowerShell process; the user's permanent settings stay intact.
                        $start = New-Object Diagnostics.ProcessStartInfo
                        $start.FileName = $env:ComSpec
                        $start.Arguments = '/d /s /c ""' + $developerScript + '" -no_logo -arch=x64 -host_arch=x64 && set"'
                        $start.UseShellExecute = $false; $start.CreateNoWindow = $true
                        $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
                        $process = New-Object Diagnostics.Process
                        $process.StartInfo = $start
                        Assert-Condition ($process.Start()) 'Cannot load the Visual Studio developer environment.'
                        $outTask = $process.StandardOutput.ReadToEndAsync()
                        $errTask = $process.StandardError.ReadToEndAsync()
                        $process.WaitForExit()
                        $environmentText = $outTask.GetAwaiter().GetResult()
                        $errorText = $errTask.GetAwaiter().GetResult()
                        $exitCode = $process.ExitCode; $process.Dispose()
                        Assert-Condition ($exitCode -eq 0) ('Visual Studio environment setup failed: ' + $errorText)
                        $developerPaths = New-Object 'System.Collections.Generic.List[string]'
                        foreach ($line in ($environmentText -split "`r?`n")) {
                            if ($line -match '^([^=]+)=(.*)$') {
                                # Some launchers pass both PATH and Path. The batch
                                # bootstrap can then print an updated PATH followed
                                # by an old Path; importing the latter would erase
                                # the compiler. Merge all path spellings first.
                                if ($matches[1] -ieq 'PATH') {
                                    foreach ($entry in ($matches[2] -split ';')) {
                                        if ($entry.Length -gt 0 -and -not $developerPaths.Contains($entry)) { $developerPaths.Add($entry) }
                                    }
                                } else {
                                    [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
                                }
                            }
                        }
                        if ($developerPaths.Count -gt 0) { $env:Path = $developerPaths -join ';' }
                    }
                }
            }
            $cl = Get-Command 'cl.exe' -ErrorAction SilentlyContinue
            if ($null -eq $cl -and $env:VCToolsInstallDir) {
                $developerCompiler = Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x64\cl.exe'
                if (Test-Path -LiteralPath $developerCompiler -PathType Leaf) {
                    $cl = Get-Command $developerCompiler -ErrorAction SilentlyContinue
                }
            }
        }
        if ($null -ne $cl -and $env:VSCMD_ARG_TGT_ARCH -eq 'x64') {
            return [pscustomobject]@{ Kind = 'MSVC'; Executable = $cl.Source }
        }
        if ($Compiler -eq 'MSVC') { throw 'MSVC x64 not found. Install Visual Studio Build Tools with Desktop development with C++, then rerun.' }
    }
    $gcc = Get-Command 'g++.exe' -ErrorAction SilentlyContinue
    if ($null -ne $gcc) {
        $target = Invoke-Native -Executable $gcc.Source -ProgramArguments @('-dumpmachine')
        Assert-Condition ($target.Stdout -match 'x86_64.*mingw') 'GCC must be an x64 MinGW-w64 compiler on Windows.'
        return [pscustomobject]@{ Kind = 'GCC'; Executable = $gcc.Source }
    }
    throw 'A C++14 compiler is required. Install Visual Studio Build Tools with Desktop development with C++, or put x64 MinGW-w64 g++ on PATH. No external solver or C++ libraries are needed.'
}
function Invoke-ReviewerProgram {
    param([string]$Name, [string[]]$ProgramArguments = @(), [string]$LogName)
    Add-Summary "RUN $Name -> logs/$LogName"
    $result = Invoke-Native -Executable (Join-Path $binaryDirectory ($Name + '.exe')) -ProgramArguments $ProgramArguments -WorkingDirectory $runDirectory -LogName $LogName
    return $result.Text
}
function Read-ResultTable([string]$Path) {
    Assert-Condition (Test-Path -LiteralPath $Path -PathType Leaf) "Missing CSV: $Path"
    $header = Get-Content -LiteralPath $Path -TotalCount 1
    $headerNames = @($header.Split(',') | ForEach-Object { $_.Trim('"') })
    Assert-Condition (($headerNames -join ',') -ceq ($csvFields -join ',')) "Unexpected CSV columns: $Path"
    $rows = @(Import-Csv -LiteralPath $Path)
    Assert-Condition ($rows.Count -gt 0) "Empty CSV: $Path"
    $keys = @{}
    foreach ($row in $rows) {
        $key = '{0},{1}' -f $row.m, $row.n
        Assert-Condition (-not $keys.ContainsKey($key)) "Duplicate CSV key $key in $Path"
        $keys[$key] = $true
    }
    return $rows
}
function Compare-ResultTables {
    param([object[]]$Expected, [object[]]$Actual, [string]$Description)
    Assert-Condition ($Expected.Count -eq $Actual.Count) "$Description has a different row count."
    $index = @{}
    foreach ($row in $Expected) { $index[('{0},{1}' -f $row.m, $row.n)] = $row }
    foreach ($row in $Actual) {
        $key = '{0},{1}' -f $row.m, $row.n
        Assert-Condition ($index.ContainsKey($key)) "$Description has an unexpected key: $key"
        foreach ($field in $csvFields) {
            Assert-Condition ([string]$index[$key].$field -ceq [string]$row.$field) "$Description differs at ($key), field $field."
        }
    }
    Add-Summary "PASS $Description`: $($Actual.Count) rows; all six CSV fields agree."
}
function Directional-Bound([long]$M, [long]$N) {
    $k = [long][math]::Floor($M / 5)
    switch ($M % 5) {
        0 { return 2 * $k * $N }
        1 { return 2 * $k * $N + [long][math]::Ceiling(2 * $N / 3) }
        2 { return (2 * $k + 1) * $N + $N % 2 }
        3 { return 2 * $k * $N + [long][math]::Ceiling(3 * $N / 2) }
        4 { return (2 * $k + 2) * $N }
    }
}
function Check-NumericFields([object[]]$Rows, [string]$Description) {
    foreach ($row in $Rows) {
        $m = [long]$row.m; $n = [long]$row.n; $value = [long]$row.gamma_R
        Assert-Condition ($m -ge 3 -and $m -le 10 -and $n -ge 3 -and $n -le 1000) "Invalid dimensions in $Description."
        $bound = [math]::Max((Directional-Bound $m $n), (Directional-Bound $n $m))
        $slopeTimesSix = 12 * [long][math]::Floor($m / 5) + @(0, 4, 6, 9, 12)[$m % 5]
        Assert-Condition ([long]$row.projection_lower_bound -eq $bound) "Incorrect projection field at ($m,$n)."
        Assert-Condition ([long]$row.excess_times_6 -eq 6 * $value - $slopeTimesSix * $n) "Incorrect excess field at ($m,$n)."
        Assert-Condition ($value -ge $bound) "Optimum below projection bound at ($m,$n)."
        Assert-Condition ($row.status -cin @('direct_exact', 'certified_recurrence')) "Unknown status at ($m,$n)."
    }
    Add-Summary "PASS $Description`: integer lower bounds and excess fields independently recomputed."
}

try {
    $requiredData = @('m7_full_check.csv','m8_full_check.csv','witness_7_23.txt','witness_8_31.txt','witness_9_17.txt','witness_10_19.txt')
    $requiredData += @(3..10 | ForEach-Object { "m${_}_exact.csv" })
    foreach ($name in $requiredData) {
        Assert-Condition (Test-Path -LiteralPath (Join-Path $dataDirectory $name) -PathType Leaf) "Missing reference/exact/$name."
    }
    foreach ($name in $programNames) {
        Assert-Condition (Test-Path -LiteralPath (Join-Path $packageRoot "src\$name.cpp") -PathType Leaf) "Missing src/$name.cpp."
    }
    $tableScript = Join-Path $packageRoot 'scripts\generate_bound_tables.py'
    $manuscript = Join-Path $packageRoot 'manuscript\roman_torus_mpcR3.tex'
    Assert-Condition (Test-Path -LiteralPath $tableScript -PathType Leaf) 'Missing scripts/generate_bound_tables.py.'
    Assert-Condition (Test-Path -LiteralPath $manuscript -PathType Leaf) 'Missing manuscript/roman_torus_mpcR3.tex.'
    if ($OutputDirectory.Length -eq 0) { $OutputDirectory = 'runs\' + [DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff') }
    if ([IO.Path]::IsPathRooted($OutputDirectory)) { $runDirectory = [IO.Path]::GetFullPath($OutputDirectory) }
    else { $runDirectory = [IO.Path]::GetFullPath((Join-Path $packageRoot $OutputDirectory)) }
    $runDirectory = $runDirectory.TrimEnd([char[]]'\/')
    Assert-Condition (-not $runDirectory.Equals($packageRoot.TrimEnd([char[]]'\/'), [StringComparison]::OrdinalIgnoreCase)) 'The package root cannot be the output directory.'
    foreach ($name in @('reference','src','scripts','manuscript','audit')) {
        $protected = Join-Path $packageRoot $name
        Assert-Condition (-not ($runDirectory.Equals($protected, [StringComparison]::OrdinalIgnoreCase) -or $runDirectory.StartsWith($protected + '\', [StringComparison]::OrdinalIgnoreCase))) "Output directory must not overwrite $name."
    }
    if (Test-Path -LiteralPath $runDirectory) {
        Assert-Condition (Test-Path -LiteralPath $runDirectory -PathType Container) 'Output location is not a directory.'
        Assert-Condition (@(Get-ChildItem -LiteralPath $runDirectory -Force).Count -eq 0) 'Output directory is not empty; choose a new directory to preserve earlier runs.'
    }
    $pythonRuntime = Find-Python
    $compilerRuntime = Find-Compiler
    $binaryDirectory = Join-Path $runDirectory 'bin'
    $objectDirectory = Join-Path $runDirectory 'obj'
    $resultDirectory = Join-Path $runDirectory 'results'
    $tableDirectory = Join-Path $runDirectory 'tables'
    $logDirectory = Join-Path $runDirectory 'logs'
    foreach ($directory in @($binaryDirectory,$objectDirectory,$resultDirectory,$tableDirectory,$logDirectory)) { [IO.Directory]::CreateDirectory($directory) | Out-Null }
    Add-Summary 'PRIVATE REVIEWER RUN: no data are uploaded or published.'
    Add-Summary "Started: $([DateTime]::Now.ToString('o'))"
    Add-Summary "Mode: $(if ($ChecksOnly) { 'CHECKS ONLY: supplied exact tables are audited, not regenerated' } else { 'FULL: source build, complete recomputation, table/manuscript comparisons' })"
    Add-Summary "Outputs: $runDirectory"
    Add-Summary "Compiler: $($compilerRuntime.Kind) $($compilerRuntime.Executable)"
    Add-Summary "Python: $($pythonRuntime.Version)"
    $environmentReport = @("OS: $([Environment]::OSVersion.VersionString)","64-bit process: $([Environment]::Is64BitProcess)","PowerShell: $($PSVersionTable.PSVersion)","Compiler: $($compilerRuntime.Executable)","Python: $($pythonRuntime.Executable)","Python version: $($pythonRuntime.Version)")
    if ($compilerRuntime.Kind -eq 'MSVC') { $environmentReport += "MSVC tools version: $env:VCToolsVersion" }
    else { $environmentReport += (Invoke-Native -Executable $compilerRuntime.Executable -ProgramArguments @('--version')).Text }
    [IO.File]::WriteAllLines((Join-Path $logDirectory 'environment.txt'), $environmentReport, $utf8)
    $inputHashes = @()
    foreach ($directory in @('src','scripts','reference')) {
        foreach ($file in Get-ChildItem -LiteralPath (Join-Path $packageRoot $directory) -File -Recurse | Sort-Object FullName) {
            $relative = $file.FullName.Substring($packageRoot.Length + 1).Replace('\','/')
            $inputHashes += (Get-Sha256 $file.FullName) + '  ' + $relative
        }
    }
    foreach ($relative in @('reproduce.ps1','reproduce.cmd','manuscript/roman_torus_mpcR3.tex')) {
        $inputHashes += (Get-Sha256 (Join-Path $packageRoot $relative)) + '  ' + $relative
    }
    [IO.File]::WriteAllLines((Join-Path $logDirectory 'input_sha256.txt'), $inputHashes, $utf8)
    foreach ($name in $programNames) {
        Add-Summary "BUILD src/$name.cpp"
        $source = Join-Path $packageRoot "src\$name.cpp"
        $executable = Join-Path $binaryDirectory "$name.exe"
        if ($compilerRuntime.Kind -eq 'MSVC') {
            $arguments = @('/nologo','/std:c++14','/EHsc','/O2','/W4','/MT',$source,('/Fo' + (Join-Path $objectDirectory "$name.obj")),('/Fe' + $executable))
        } else { $arguments = @('-std=c++14','-O3','-Wall','-Wextra','-pedantic',$source,'-o',$executable) }
        $null = Invoke-Native -Executable $compilerRuntime.Executable -ProgramArguments $arguments -WorkingDirectory $runDirectory -LogName "build_$name.log"
    }
    Add-Summary 'PASS source build: all six programs compiled locally.'
    if (-not $ChecksOnly) {
        foreach ($m in 3..10) {
            $arguments = @('--m',[string]$m,'--n-max','1000','--csv',"results\m${m}_exact.csv")
            if ($m -eq 7) { $arguments += @('--witness','23','--witness-file','results\witness_7_23.txt') }
            $text = Invoke-ReviewerProgram 'roman_torus_exact' $arguments "m${m}_exact.log"
            Assert-Condition ($text -match '(?m)^CERTIFICATE:') "No matrix recurrence certificate for width $m."
        }
        foreach ($pair in @(@(8,31),@(9,17),@(10,19))) {
            $m = $pair[0]; $n = $pair[1]
            $arguments = @('--m',[string]$m,'--n-max',[string]$n,'--witness',[string]$n,'--witness-file',"results\witness_${m}_${n}.txt",'--csv',"results\m${m}_witness_run.csv")
            $null = Invoke-ReviewerProgram 'roman_torus_exact' $arguments "witness_${m}_${n}.log"
        }
        foreach ($pair in @(@(7,80),@(8,50))) {
            $m = $pair[0]; $n = $pair[1]
            $arguments = @('--m',[string]$m,'--n-max',[string]$n,'--no-symmetry','--no-recurrence','--csv',"results\m${m}_full_check.csv")
            $null = Invoke-ReviewerProgram 'roman_torus_exact' $arguments "m${m}_full_check.log"
        }
    } else {
        # Stage ASCII relative filenames so the C++14 narrow-path reader also
        # works when the package was extracted beneath a Unicode directory.
        foreach ($name in $requiredData) { Copy-Item -LiteralPath (Join-Path $dataDirectory $name) -Destination (Join-Path $resultDirectory $name) }
    }
    $null = Invoke-ReviewerProgram 'verify_exact_results' @('results') 'verify_exact_results.log'
    foreach ($name in @('verify_manuscript','verify_phase_barrier','check_type4_12_8')) {
        $null = Invoke-ReviewerProgram $name @() "$name.log"
    }
    $bridgeText = Invoke-ReviewerProgram 'bridge_all_lengths' @('2','16','2','400000') 'bridge_all_lengths.log'
    $bridgeComplete = $bridgeText -notmatch '(?m)^STOP:' -and $bridgeText -match '(?m)^CERTIFICATE:'
    if ($bridgeComplete) { Add-Summary 'PASS complete-map bridge certificate: m=12, budget=2; even and odd endpoints.' }
    else { Add-Summary 'INCONCLUSIVE bridge completeness: STOP or absent CERTIFICATE. Missing residues cannot be excluded.' }
    $exactRows = @()
    foreach ($m in 3..10) {
        $original = @(Read-ResultTable (Join-Path $dataDirectory "m${m}_exact.csv"))
        $regenerated = @(Read-ResultTable (Join-Path $resultDirectory "m${m}_exact.csv"))
        Assert-Condition ($regenerated.Count -eq 998) "Expected 998 rows for width $m."
        foreach ($row in $regenerated) { Assert-Condition ([int]$row.m -eq $m) "Wrong width in m${m}_exact.csv." }
        if (-not $ChecksOnly) { Compare-ResultTables $original $regenerated "m${m}_exact.csv" }
        Check-NumericFields $regenerated "m${m}_exact.csv"
        $exactRows += $regenerated
    }
    Assert-Condition ($exactRows.Count -eq 7984) 'Expected 7,984 exact rows.'
    foreach ($m in @(7,8)) {
        $original = @(Read-ResultTable (Join-Path $dataDirectory "m${m}_full_check.csv"))
        $regenerated = @(Read-ResultTable (Join-Path $resultDirectory "m${m}_full_check.csv"))
        Assert-Condition ($regenerated.Count -eq $(if ($m -eq 7) {78} else {48})) "Wrong direct-check count for width $m."
        if (-not $ChecksOnly) { Compare-ResultTables $original $regenerated "m${m}_full_check.csv" }
        Check-NumericFields $regenerated "m${m}_full_check.csv"
        $matching = @($exactRows | Where-Object { [int]$_.m -eq $m -and [int]$_.n -le $(if ($m -eq 7) {80} else {50}) })
        foreach ($row in $matching) {
            $direct = @($regenerated | Where-Object { [int]$_.n -eq [int]$row.n })
            Assert-Condition ($direct.Count -eq 1 -and $direct[0].gamma_R -eq $row.gamma_R) "Direct and reduced values differ at ($m,$($row.n))."
        }
        Add-Summary "PASS width $m`: every full-state value agrees with its symmetry-reduced value."
    }
    foreach ($pair in @(@(7,23),@(8,31),@(9,17),@(10,19))) {
        $m = $pair[0]; $n = $pair[1]; $name = "witness_${m}_${n}.txt"
        $original = [IO.File]::ReadAllText((Join-Path $dataDirectory $name)).Replace("`r`n","`n")
        $regenerated = [IO.File]::ReadAllText((Join-Path $resultDirectory $name)).Replace("`r`n","`n")
        if ($original -ceq $regenerated) { Add-Summary "PASS $name`: labels and metadata match; coverage and weight independently audited." }
        else { Add-Summary "PASS $name`: another optimal witness; coverage and weight independently audited against the exact table." }
        if (-not $ChecksOnly -and $m -ne 7) {
            $expected = @($exactRows | Where-Object { [int]$_.m -eq $m -and [int]$_.n -le $n })
            $actual = @(Read-ResultTable (Join-Path $resultDirectory "m${m}_witness_run.csv"))
            Compare-ResultTables $expected $actual "m${m}_witness_run.csv"
        }
    }
    Add-Summary 'RUN Appendix D generation and cell-by-cell comparison against the current manuscript.'
    $arguments = @($pythonRuntime.Prefix) + @($tableScript,'--data-dir',$resultDirectory,'--output-dir',$tableDirectory,'--manuscript',$manuscript,'--reference-dir',(Join-Path $packageRoot 'reference\tables'))
    $null = Invoke-Native -Executable $pythonRuntime.Executable -ProgramArguments $arguments -WorkingDirectory $runDirectory -LogName 'generate_bound_tables.log'
    Add-Summary 'PASS Appendix D: 1,246 manuscript cells, nine literature-summary rows, and all four reference table files agree.'
    if ($ChecksOnly) { Add-Summary 'LIMITS: seven tiny graphs are independently optimized by exhaustive center-subset enumeration; the 7,984 supplied rows are audited, not recomputed.' }
    else { Add-Summary 'LIMITS: seven tiny graphs are independently optimized by exhaustive center-subset enumeration; 7,984 rows are recomputed by transfer matrices with checked recurrence certificates.' }
    Add-Summary 'LIMITS: bridge completeness concerns the fixed-width positive-to-positive bridge problem; the phase barrier and concentrated-profile check retain their stated hypotheses.'
    Add-Summary "Elapsed seconds: $([math]::Round($watch.Elapsed.TotalSeconds,2))"
    Add-Summary "Finished: $([DateTime]::Now.ToString('o'))"
    if (-not $bridgeComplete) { Add-Summary 'INCONCLUSIVE: other requested checks passed, but no complete bridge certificate. Exit code 2.'; exit 2 }
    if ($ChecksOnly) { Add-Summary 'CHECKS-ONLY SUCCESS: supplied tables audited and Appendix D regenerated; exact tables were NOT recomputed. Exit code 0.' }
    else { Add-Summary 'FULL SUCCESS: all source builds, 7,984 recomputed exact rows, 126 direct full-state rows, four witnesses, finite mathematical checks, bridge certificate, and Appendix D comparisons passed. Exit code 0.' }
    exit 0
} catch {
    $message = 'FAILURE: ' + $_.Exception.Message
    Write-Host $message
    $summaryLines.Add($message)
    if ($null -ne $logDirectory -and (Test-Path -LiteralPath $logDirectory -PathType Container)) {
        [IO.File]::WriteAllLines((Join-Path $logDirectory 'SUMMARY.txt'), $summaryLines, $utf8)
    }
    exit 1
}
