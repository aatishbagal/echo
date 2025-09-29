# Echo Setup Script for Windows
# Single-click install and build script (equivalent to setup.sh)

$ErrorActionPreference = "Stop"

# Colors
function Write-Step($text) { Write-Host $text -ForegroundColor Cyan }
function Write-Success($text) { Write-Host "✓ $text" -ForegroundColor Green }
function Write-Warning($text) { Write-Host "⚠ $text" -ForegroundColor Yellow }
function Write-Error($text) { Write-Host "✗ $text" -ForegroundColor Red }

Write-Host "===============================================" -ForegroundColor Green
Write-Host "Echo - BitChat Compatible Desktop Messaging" -ForegroundColor Green
Write-Host "Setup Script for Windows" -ForegroundColor Green
Write-Host "===============================================" -ForegroundColor Green
Write-Host "Starting Echo setup...`n"

# Check prerequisites
Write-Step "Checking prerequisites..."

if (!(Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Error "Git not found"
    Write-Host "Please install Git from https://git-scm.com/ and run this script again"
    Read-Host "Press Enter to exit"
    exit 1
}
Write-Success "Git found"

if (!(Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake not found"
    Write-Host "Please install CMake from https://cmake.org/ and run this script again"
    Read-Host "Press Enter to exit"
    exit 1
}
Write-Success "CMake found"

# Check Visual Studio
$vsFound = $false
$vsGenerator = ""
foreach ($year in @("2022", "2019")) {
    foreach ($edition in @("Enterprise", "Professional", "Community")) {
        $vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\$year\$edition\MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path $vsPath) {
            Write-Success "Visual Studio $year $edition found"
            $vsGenerator = "Visual Studio $(if ($year -eq '2022') {'17'} else {'16'}) $year"
            $vsFound = $true
            break
        }
    }
    if ($vsFound) { break }
}

if (-not $vsFound) {
    Write-Error "Visual Studio 2019 or 2022 not found"
    Write-Host "Please install Visual Studio with C++ development tools"
    Read-Host "Press Enter to exit"
    exit 1
}

if (!(Test-Path "CMakeLists.txt")) {
    Write-Error "CMakeLists.txt not found. Are you in the Echo project directory?"
    Read-Host "Press Enter to exit"
    exit 1
}

# Initialize git
if (!(Test-Path ".git")) {
    Write-Step "`nInitializing git repository..."
    git init
    git add .
    git commit -m "Initial Echo project setup"
    Write-Success "Git repository initialized"
}

Write-Host ""

# Install Chocolatey if not present (for automatic dependency installation)
Write-Step "Checking for Chocolatey package manager..."
if (!(Get-Command choco -ErrorAction SilentlyContinue)) {
    Write-Warning "Chocolatey not found. Installing Chocolatey for automatic dependency management..."
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    try {
        Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
        Write-Success "Chocolatey installed"
    } catch {
        Write-Warning "Chocolatey installation failed. Will try vcpkg instead..."
    }
}

Write-Host ""

# Install vcpkg and dependencies
Write-Step "Setting up vcpkg and dependencies..."

$vcpkgPath = "vcpkg"
$useExistingVcpkg = $false

# Check for existing vcpkg
if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
    Write-Success "Using existing vcpkg at: $env:VCPKG_ROOT"
    $vcpkgPath = "$env:VCPKG_ROOT\vcpkg.exe"
    $useExistingVcpkg = $true
} elseif (Test-Path "vcpkg\vcpkg.exe") {
    Write-Success "Using local vcpkg installation"
    $vcpkgPath = "vcpkg\vcpkg.exe"
    $env:VCPKG_ROOT = (Resolve-Path "vcpkg").Path
    $useExistingVcpkg = $true
}

if (-not $useExistingVcpkg) {
    Write-Step "Installing vcpkg..."
    if (Test-Path "vcpkg") {
        Remove-Item -Recurse -Force "vcpkg"
    }
    
    git clone https://github.com/Microsoft/vcpkg.git 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to clone vcpkg"
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    .\vcpkg\bootstrap-vcpkg.bat
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to bootstrap vcpkg"
        Read-Host "Press Enter to exit"
        exit 1
    }
    
    $vcpkgPath = "vcpkg\vcpkg.exe"
    $env:VCPKG_ROOT = (Resolve-Path "vcpkg").Path
    Write-Success "vcpkg installed"
}

Write-Step "Installing dependencies (libsodium, openssl, lz4)..."
$packages = @("libsodium:x64-windows", "openssl:x64-windows", "lz4:x64-windows")
foreach ($pkg in $packages) {
    Write-Host "  Installing $pkg..."
    & $vcpkgPath install $pkg
}
Write-Success "Dependencies installed"

Write-Host ""

# Create directory structure
Write-Step "Creating directory structure..."
$dirs = @(
    "src\core\bluetooth", "src\core\protocol", "src\core\crypto",
    "src\core\mesh", "src\core\commands", "src\ui", "src\utils",
    "external", "tests", "docs", "build"
)
foreach ($dir in $dirs) {
    if (!(Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
}
Write-Success "Directory structure created"

Write-Host ""

# Setup development environment files
Write-Step "Setting up development environment..."

if (!(Test-Path ".gitignore")) {
    @"
# Build directories
build/
*build*/
out/

# Visual Studio
.vs/
*.vcxproj.user
*.sln.docstates

# vcpkg
vcpkg/

# Binaries
*.exe
*.dll
*.lib
*.pdb
*.obj

# Temporary
*.tmp
*.log
"@ | Out-File -FilePath ".gitignore" -Encoding UTF8
    Write-Success "Created .gitignore"
}

if (!(Test-Path ".vscode")) {
    New-Item -ItemType Directory -Path ".vscode" | Out-Null
    @{
        "C_Cpp.default.configurationProvider" = "ms-vscode.cmake-tools"
        "cmake.buildDirectory" = "`${workspaceFolder}/build"
        "files.associations" = @{ "*.h" = "cpp"; "*.cpp" = "cpp" }
    } | ConvertTo-Json -Depth 3 | Out-File -FilePath ".vscode\settings.json" -Encoding UTF8
    Write-Success "Created VS Code configuration"
}

Write-Host ""

# Setup SimpleBLE
Write-Step "Setting up SimpleBLE..."

git rm --cached external/simpleble 2>$null
if (Test-Path "external\simpleble") {
    Remove-Item -Recurse -Force "external\simpleble"
}
if (Test-Path ".gitmodules") {
    Remove-Item ".gitmodules"
}

try {
    git submodule add https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble 2>$null
    git submodule update --init --recursive
    Write-Success "SimpleBLE added as submodule"
} catch {
    Write-Warning "Submodule failed, cloning directly..."
    git clone --recursive https://github.com/OpenBluetoothToolbox/SimpleBLE.git external/simpleble
}

if (Test-Path "external\simpleble\simpleble\CMakeLists.txt") {
    Write-Success "SimpleBLE setup complete!"
} elseif (Test-Path "external\simpleble\CMakeLists.txt") {
    Write-Success "SimpleBLE setup complete!"
} else {
    Write-Error "SimpleBLE setup failed - CMakeLists.txt not found"
    Get-ChildItem "external\simpleble" -ErrorAction SilentlyContinue
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""

# Build project
Write-Step "Building Echo..."

Set-Location build

# Clean previous build
Get-ChildItem -Path . -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force -Recurse -ErrorAction SilentlyContinue

# Configure
Write-Step "Running CMake configuration..."
$vcpkgToolchain = Join-Path (Split-Path $vcpkgPath -Parent) "scripts\buildsystems\vcpkg.cmake"
cmake .. -G $vsGenerator -DCMAKE_TOOLCHAIN_FILE="$vcpkgToolchain"

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed"
    
    Write-Host "`nDebugging information:"
    if (Test-Path "..\external\simpleble\simpleble\CMakeLists.txt") {
        Write-Host "Found: simpleble\simpleble\CMakeLists.txt"
    } else {
        Write-Host "Not found: simpleble\simpleble\CMakeLists.txt"
    }
    
    Set-Location ..
    Read-Host "Press Enter to exit"
    exit 1
}
Write-Success "CMake configuration successful"

# Build
Write-Step "Building project (this may take a few minutes)..."
cmake --build . --config Release --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    Set-Location ..
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Success "Build successful!"

Set-Location ..

# Success message
Write-Host ""
Write-Host "===============================================" -ForegroundColor Green
Write-Host "Setup complete!" -ForegroundColor Green
Write-Host "===============================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Run Echo: .\build\Release\echo.exe"
Write-Host "2. Test Bluetooth scanning: Type 'scan' in Echo"
Write-Host "3. Check docs\ for development guides"
Write-Host ""
Write-Host "Happy coding!" -ForegroundColor Green
Write-Host ""
Read-Host "Press Enter to exit"