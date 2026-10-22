param(
    [string]$ArduinoCli = 'arduino-cli',
    [string]$ConfigFile = '',
    [string]$Fqbn = 'arduino:avr:mega',
    [switch]$Upload,
    [string]$Port = ''
)

$ErrorActionPreference = 'Stop'
if ($Upload -and -not $Port) {
    throw 'Use -Upload -Port COM4 para indicar explicitamente a placa que receberá o firmware.'
}

$repo = Split-Path -Parent $PSScriptRoot
if (Test-Path -LiteralPath $ArduinoCli -PathType Leaf) {
    $ArduinoCli = (Resolve-Path -LiteralPath $ArduinoCli).Path
}
if ($ConfigFile) {
    $ConfigFile = (Resolve-Path -LiteralPath $ConfigFile).Path
}

Push-Location $repo
try {
    $buildPath = Join-Path $repo '.build/avr'
    New-Item -ItemType Directory -Force $buildPath | Out-Null
    $common = @('--fqbn', $Fqbn)
    if ($ConfigFile) {
        $common += @('--config-file', $ConfigFile)
    }

    & $ArduinoCli compile @common --warnings all --build-path $buildPath 'src/DynamicIMU'
    if ($LASTEXITCODE -ne 0) {
        throw 'Falha na compilação para Arduino.'
    }

    # Compilar é a operação padrão; a gravação depende dos dois parâmetros explícitos.
    if ($Upload) {
        & $ArduinoCli upload @common --port $Port --input-dir $buildPath 'src/DynamicIMU'
        if ($LASTEXITCODE -ne 0) {
            throw 'Falha ao gravar o firmware na placa.'
        }
    }
}
finally {
    Pop-Location
}
