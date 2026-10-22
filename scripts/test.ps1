param(
    [string]$Compiler = 'clang++',
    [string]$Python = 'python',
    [switch]$Sanitize,
    [switch]$Simulate
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$previousPath = $env:PATH
Push-Location $repo
try {
    New-Item -ItemType Directory -Force '.build' | Out-Null
    $flags = @('-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion', '-Wshadow', '-Werror')
    if ($Sanitize) {
        $flags += @('-fsanitize=address', '-g')
        # Clang instala o runtime do AddressSanitizer fora do diretório do executável.
        $runtime = & $Compiler --print-runtime-dir
        if ($LASTEXITCODE -ne 0) {
            throw 'Não foi possível localizar o runtime do Clang.'
        }
        if (-not (Test-Path -LiteralPath $runtime -PathType Container)) {
            $runtime = Join-Path (Split-Path -Parent $runtime) 'windows'
        }
        if (-not (Test-Path -LiteralPath $runtime -PathType Container)) {
            throw 'Runtime do AddressSanitizer não encontrado na instalação do Clang.'
        }
        $windowsRuntime = Join-Path (Split-Path -Parent $runtime) 'windows'
        $env:PATH = "$runtime$([IO.Path]::PathSeparator)$windowsRuntime$([IO.Path]::PathSeparator)$env:PATH"
    }

    foreach ($program in @('core_tests', 'replay')) {
        & $Compiler @flags "tests/$program.cpp" -o ".build/$program.exe"
        if ($LASTEXITCODE -ne 0) {
            throw "Falha na compilação de $program."
        }
    }
    & './.build/core_tests.exe'
    if ($LASTEXITCODE -ne 0) {
        throw 'Falha nos testes numéricos.'
    }
    & $Python -m unittest discover -s tests -p 'test_*.py' -v
    if ($LASTEXITCODE -ne 0) {
        throw 'Falha nos testes de telemetria/calibração.'
    }
    if ($Simulate) {
        & $Python 'scripts/simulate.py' --replay '.build/replay.exe'
        if ($LASTEXITCODE -ne 0) {
            throw 'Falha na simulação independente.'
        }
    }
}
finally {
    $env:PATH = $previousPath
    Pop-Location
}
