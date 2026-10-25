param(
    [ValidateSet('Release', 'RelWithDebInfo', 'Debug')]
    [string]$Configuration = 'Release',
    [ValidateRange(1, 4)]
    [int]$Parallel = 2,
    [switch]$ConfigureOnly
)

$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = Join-Path $sourceRoot 'build-xs-vs2022'
$vsRoot = 'E:/Vs2022-IDE'
$gitExe = (Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$cmakeCandidates = @(
    'E:/CMake3.25.3/bin/cmake.exe',
    (Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
)
$cmakeExe = $cmakeCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $cmakeExe) { throw 'CMake executable was not found in either specified installation.' }
if ($cmakeExe -ne $cmakeCandidates[0]) { Write-Warning "Using the CMake bundled with VS2022: $cmakeExe" }

$botLink = Join-Path $sourceRoot 'modules/mod-playerbots'
$botSource = [IO.Path]::GetFullPath((Join-Path $sourceRoot '../mod-playerbots-xs'))
if (-not (Test-Path -LiteralPath $botLink)) {
    if (-not (Test-Path -LiteralPath (Join-Path $botSource 'src/Script/Playerbots.cpp'))) {
        throw 'Initialize modules/mod-playerbots with git submodule update --init, or provide the sibling mod-playerbots-xs checkout.'
    }
    New-Item -ItemType Junction -Path $botLink -Target $botSource | Out-Null
}
$expectedBotCommit = (& git -C $sourceRoot rev-parse 'HEAD:modules/mod-playerbots' 2>$null)
if ($LASTEXITCODE -eq 0) {
    $actualBotCommit = (& git -C $botLink rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $actualBotCommit -ne $expectedBotCommit) {
        throw 'Playerbots checkout does not match the pinned server submodule commit.'
    }
}

$cmakeArguments = @(
    '-S', $sourceRoot, '-B', $buildRoot, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_GENERATOR_INSTANCE=$vsRoot",
    "-DGIT_EXECUTABLE:FILEPATH=$gitExe",
    '-DBoost_ROOT=E:/boost_1_78_0', '-DBOOST_ROOT=E:/boost_1_78_0',
    '-DBOOST_LIBRARYDIR=E:/boost_1_78_0/lib64-msvc-14.3', '-DBoost_NO_SYSTEM_PATHS=ON',
    '-DOPENSSL_ROOT_DIR=E:/OpenSSL-Win64',
    '-DMYSQL_INCLUDE_DIR=E:/mysql-8.0.12-winx64/include',
    '-DMYSQL_LIBRARY=E:/mysql-8.0.12-winx64/lib/libmysql.lib',
    '-DMYSQL_EXECUTABLE=E:/mysql-8.0.12-winx64/bin/mysql.exe',
    '-DSCRIPTS=static', '-DMODULES=static', '-DBUILD_TESTING=OFF',
    "-DCMAKE_INSTALL_PREFIX=$buildRoot/install"
)
& $cmakeExe @cmakeArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }
if (-not $ConfigureOnly) {
    & $cmakeExe --build $buildRoot --config $Configuration --parallel $Parallel -- /nologo /p:UseMultiToolTask=true /p:EnforceProcessCountAcrossBuilds=true /p:MultiProcMaxCount=4 /p:CL_MPCount=4
    if ($LASTEXITCODE -ne 0) { throw "Server build failed: $LASTEXITCODE" }
}
