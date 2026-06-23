# Deploy OpenSSL 1.1 runtime DLLs required by Qt HTTPS on Windows.
param(
    [Parameter(Mandatory = $true)]
    [string]$TargetDir,

    [Parameter(Mandatory = $true)]
    [ValidateSet("x64", "x86")]
    [string]$Arch,

    [switch]$Required
)

if ($Arch -eq "x86") {
    $opensslSubdir = "Win_x86"
    $requiredPairs = @()
    $requiredPairs += ,@("libssl-1_1.dll", "libcrypto-1_1.dll")
} else {
    $opensslSubdir = "Win_x64"
    $requiredPairs = @()
    $requiredPairs += ,@("libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll")
    $requiredPairs += ,@("libssl-1_1.dll", "libcrypto-1_1.dll")
}

$candidateNames = @()
foreach ($pair in $requiredPairs) {
    $candidateNames += $pair
}
$candidateNames = $candidateNames | Select-Object -Unique
$expectedMachine = if ($Arch -eq "x86") { 0x014c } else { 0x8664 }

$searchPaths = @()
if ($env:Qt5_Dir) {
    $qtRoot = (Resolve-Path (Join-Path $env:Qt5_Dir "..\..\") -ErrorAction SilentlyContinue).Path
    if ($qtRoot) {
        $searchPaths += (Join-Path $qtRoot "Tools\OpenSSL\$opensslSubdir\bin")
        $searchPaths += (Join-Path $qtRoot "Tools\OpenSSL\$opensslSubdir")
        $searchPaths += (Join-Path $qtRoot "Tools\OpenSSL\Win_x64\bin")
        $searchPaths += (Join-Path $qtRoot "Tools\OpenSSL\Win_x64")
        $searchPaths += (Join-Path $qtRoot "bin")
    }
}
$searchPaths += @(
    "C:\Program Files\OpenSSL\bin",
    "C:\Program Files\OpenSSL-Win64\bin",
    "C:\Program Files\OpenSSL-Win32\bin",
    "${env:ProgramFiles(x86)}\OpenSSL-Win32\bin",
    "${env:ProgramFiles(x86)}\OpenSSL-Win64\bin",
    "C:\OpenSSL-Win64\bin",
    "C:\OpenSSL-Win32\bin",
    "C:\Qt\Tools\OpenSSL\$opensslSubdir\bin",
    "C:\Qt\Tools\OpenSSL\$opensslSubdir",
    "C:\Qt\Tools\OpenSSL\Win_x64\bin"
)
$searchPaths = $searchPaths | Where-Object { $_ } | Select-Object -Unique

function Get-DllMachine {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = $null
    $reader = $null
    try {
        $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        $reader = New-Object System.IO.BinaryReader($stream)
        $stream.Seek(0x3c, [System.IO.SeekOrigin]::Begin) | Out-Null
        $peOffset = $reader.ReadInt32()
        if ($peOffset -le 0 -or $peOffset -gt ($stream.Length - 6)) {
            return $null
        }

        $stream.Seek($peOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $signature = $reader.ReadUInt32()
        if ($signature -ne 0x00004550) {
            return $null
        }

        return $reader.ReadUInt16()
    } catch {
        return $null
    } finally {
        if ($reader) {
            $reader.Close()
        } elseif ($stream) {
            $stream.Close()
        }
    }
}

function Test-DllArchitecture {
    param([Parameter(Mandatory = $true)][string]$Path)

    $machine = Get-DllMachine -Path $Path
    return ($machine -eq $expectedMachine)
}

function Test-CompletePair {
    param(
        [Parameter(Mandatory = $true)][string]$Dir,
        [Parameter(Mandatory = $true)][string[]]$Pair
    )

    foreach ($name in $Pair) {
        $path = Join-Path $Dir $name
        if (-not (Test-Path -LiteralPath $path -ErrorAction SilentlyContinue)) {
            return $false
        }
        if (-not (Test-DllArchitecture -Path $path)) {
            return $false
        }
    }

    return $true
}

function Select-OpenSslSource {
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    foreach ($p in $Paths) {
        if (-not $p -or -not (Test-Path -LiteralPath $p -ErrorAction SilentlyContinue)) { continue }
        foreach ($pair in $requiredPairs) {
            if (Test-CompletePair -Dir $p -Pair $pair) {
                $script:sourceDir = $p
                $script:selectedPair = $pair
                Write-Host "[OK] Found OpenSSL at: $p"
                Write-Host "[OK] Selected DLL pair: $($pair -join ', ')"
                return $true
            }
        }
    }

    return $false
}

function Search-DiskForOpenSsl {
    Write-Host "[INFO] Searching entire disk for OpenSSL 1.1 DLLs..."

    $seenDirs = @()
    foreach ($name in $candidateNames) {
        $hits = Get-ChildItem -Path "C:\" -Recurse -Filter $name -ErrorAction SilentlyContinue
        foreach ($hit in $hits) {
            if (-not $hit -or -not $hit.DirectoryName) { continue }
            if ($seenDirs -contains $hit.DirectoryName) { continue }
            $seenDirs += $hit.DirectoryName

            foreach ($pair in $requiredPairs) {
                if (Test-CompletePair -Dir $hit.DirectoryName -Pair $pair) {
                    $script:sourceDir = $hit.DirectoryName
                    $script:selectedPair = $pair
                    Write-Host "[OK] Found via disk scan: $script:sourceDir"
                    Write-Host "[OK] Selected DLL pair: $($pair -join ', ')"
                    return $true
                }
            }
        }
    }

    return $false
}

$sourceDir = $null
$selectedPair = $null
Select-OpenSslSource -Paths $searchPaths | Out-Null

if (-not $sourceDir) {
    Search-DiskForOpenSsl | Out-Null
}

if (-not $sourceDir) {
    Write-Host "[WARN] Complete OpenSSL 1.1 runtime pair not found for $Arch."
    Write-Host "[WARN] Qt HTTPS (QNetworkAccessManager) will not work."
    Write-Host "[WARN] CEF browser HTTPS is unaffected. Desktop auth retries in background."
    if ($Required) {
        exit 1
    }
    exit 0
}

if (-not (Test-Path -LiteralPath $TargetDir -ErrorAction SilentlyContinue)) {
    New-Item -ItemType Directory -Force -Path $TargetDir -ErrorAction SilentlyContinue | Out-Null
}

if (-not (Test-Path -LiteralPath $TargetDir -ErrorAction SilentlyContinue)) {
    Write-Host "[ERROR] Target directory could not be created: $TargetDir"
    exit 1
}

$copied = 0
$copyFailed = $false
foreach ($name in $selectedPair) {
    $src = Join-Path $sourceDir $name
    $dst = Join-Path $TargetDir $name
    try {
        Copy-Item -LiteralPath $src -Destination $dst -Force
        $size = (Get-Item -LiteralPath $src).Length
        Write-Host "[OK] $name ($([math]::Round($size/1KB, 1)) KB)"
        $copied++
    } catch {
        Write-Host "[WARN] Failed to copy $name : $_"
        $copyFailed = $true
    }
}

if ($copyFailed -or $copied -ne $selectedPair.Count) {
    Write-Host "[ERROR] OpenSSL copy incomplete"
    if ($Required) {
        exit 1
    }
} elseif ($copied -gt 0) {
    Write-Host "[SUCCESS] OpenSSL: $copied DLLs deployed to $TargetDir"
} else {
    Write-Host "[WARN] No OpenSSL DLLs copied"
    if ($Required) {
        exit 1
    }
}
exit 0
