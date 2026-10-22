# Determinação de verticalidade com IMU

Trabalho de Engenharia da Computação da UNIP, Rafael José Mota Ocariz, 2018.
Revisão técnica de **22 de outubro de 2026**: estimação contínua com acelerômetro
e girômetro, quatérnios, calibração explícita e estados de falha. O hardware
original é mantido. A revisão foi verificada em software e compilada para
Arduino Mega; o desempenho físico dinâmico ainda depende de ensaios de bancada.

## Organização

| Caminho | Conteúdo |
|---|---|
| [monografia.md](monografia.md) | Fonte editável da monografia, com conteúdo histórico e revisão dinâmica |
| [images/](images/) | Figuras, páginas do anexo e desenho mecânico referenciados pela monografia |
| [src/DynamicIMU/](src/DynamicIMU/) | Firmware atual, núcleo matemático portátil e configuração |
| [src/legacy/](src/legacy/) | Implementações históricas com formatação revisada; limitações na monografia |
| [scripts/](scripts/) | Compilação, testes, telemetria, calibração e simulação reproduzível |
| [tests/](tests/) | Verificação numérica do núcleo, replay e testes das ferramentas |
| [docs/telemetria.md](docs/telemetria.md) | Pacotes e comandos do firmware |
| [docs/validacao-bancada.md](docs/validacao-bancada.md) | Validação física e critérios provisórios |

A migração para Markdown preserva legendas, tabelas, equações e imagens. A figura
15, originalmente um diagrama editável do Word, foi reconstituída em SVG. O DOCX
e a apresentação PowerPoint foram retirados. `Monografia.pdf` permanece como
export histórico de 2018, sem atualização; a fonte atual é o Markdown. A revisão
não representa nova aprovação acadêmica. Os originais estão no histórico Git,
no commit `9a5279f5528d752bc9bc3225ec33e3b0dc0fcffa`.

## Arquitetura do firmware

```text
src/DynamicIMU/
├── DynamicIMU.ino        # setup/loop: entrada do Arduino
├── Config.h             # opções da montagem e construção da calibração
├── CalibrationData.h    # parâmetros de calibração do instrumento
├── ImuCore.h            # interface pública do núcleo portátil para testes/replay
└── src/
    ├── application/     # composição dos módulos, ciclo principal e comandos
    ├── hardware/        # inicialização de barramentos, pinagem e LCD/toque
    ├── sensors/         # MPU-6050: configuração, aquisição I²C e amostras brutas
    ├── processing/      # calibração, estatística, quatérnios, observador e falhas
    ├── telemetry/       # codificação dos pacotes e transporte serial
    └── ui/              # apresentação dos valores e ações dos botões
```

A pasta interna `src/` segue a estrutura de sketch do Arduino, que compila os
arquivos `.cpp` recursivamente nesse diretório. As pastas de testes e scripts
continuam na raiz do repositório; o legado permanece em `src/legacy/`.

`Application` coordena a sequência: adquirir uma amostra, processar, enfileirar
a telemetria, verificar falhas, atender a serial e atualizar uma linha da tela
quando necessário. `Mpu6050` disponibiliza dados brutos e timestamps sem conhecer
o estimador. `Instrument` aplica a calibração e controla o `Observer`, recebendo
tempo como dado e sem dependência de Arduino. A tela recebe um `ViewModel` e
devolve ações de tara/calibração; a aplicação executa essas ações. Pinagem e
inicialização dos controladores ficam em `hardware`, enquanto o desenho fica em
`ui`. O codificador de pacotes também é independente do hardware e pode ser
verificado junto do decodificador Python.

## Hardware e dependências

Arduino Mega 2560, MPU-6050/GY-521 em I²C no endereço `0x68`, TFT-320QVT SSD1289
com shield MEGA V2.2. Pinagem preservada: UTFT `(SSD1289, 38, 39, 40, 41)`,
URTouch `(6, 5, 4, 3, 2)`, SDA20/SCL21. Conferir montagem, alimentação e níveis
elétricos do módulo. O INT do MPU fica desconectado; o pino 2 já é usado pelo toque.

Ambiente de compilação: Arduino CLI 1.3.1, core `arduino:avr@1.8.6`, AVR GCC
7.3.0-atmel3.6.1-arduino7, UTFT 2.83 e URTouch 2.02. Instalar as bibliotecas de
Henning Karlsen pelas páginas do autor: [UTFT](http://www.rinkydinkelectronics.com/library.php?id=51)
e [URTouch](http://www.rinkydinkelectronics.com/library.php?id=92). Habilitar
SSD1289 em `memorysaver.h` da UTFT. As bibliotecas declaram CC BY-NC-SA 3.0 e
condições próprias para uso comercial; consultar as respectivas distribuições.
Elas não são redistribuídas neste repositório.

SHA-256 dos pacotes usados na verificação:

```text
UTFT.zip     96fe427632777048ff865d9636d0615b215b94e03937a8d57e17d3db1de3589b
URTouch.zip  25d16371a14485801eda5ee94703d345695253eb95789b6fe339a8751e3fbd99
```

## Compilar e gravar

Executar em PowerShell, com Arduino CLI no PATH e as bibliotecas instaladas no
sketchbook do Arduino:

```powershell
arduino-cli core install arduino:avr@1.8.6
./scripts/build.ps1
```

O build fica em `.build/avr/`. Para usar uma instalação isolada:

```powershell
./scripts/build.ps1 -ArduinoCli .tools/arduino-cli/arduino-cli.exe -ConfigFile .tools/arduino-cli.yaml
```

A gravação exige `-Upload` e a porta da placa. Substituir `COM4` pela porta correta:

```powershell
./scripts/build.ps1 -Upload -Port COM4
```

O firmware usa aquisição nominal de 50 Hz, faixas ±2 g e ±250 °/s, telemetria
binária a 115200 baud e atualização parcial do LCD. A configuração entregue em
[CalibrationData.h](src/DynamicIMU/CalibrationData.h) contém valores **nominais,
sem calibração física**; por padrão o estimador permanece sem saída válida até
receber dados de calibração revisados. O modo LAB é experimental e explicitamente
identificado. Recuperar falhas exige calibração explícita, conforme a
[telemetria](docs/telemetria.md).

## Testes e simulação

Os testes de host exigem Clang++ e Python 3. A simulação usa NumPy; a captura
serial usa pyserial. Decodificação de arquivos e calibração usam a biblioteca
padrão do Python.

```powershell
python -m pip install -r requirements.txt
./scripts/test.ps1
./scripts/test.ps1 -Sanitize -Simulate
```

Se os executáveis não estiverem no PATH, usar `-Compiler` e `-Python` com seus
caminhos. `-Sanitize` habilita AddressSanitizer do Clang. O teste compila o núcleo
com avisos tratados como erros e verifica os cálculos, transições de estado,
compatibilidade entre codificador C++ e decodificador Python, CRC,
ressincronização e ajuste de seis posições. `-Simulate` executa quatro trajetórias sintéticas com
referência independente de matrizes de Rodrigues em NumPy. JSON, CSV e gráficos
SVG são gerados em `.build/results/`. Esses resultados verificam o software e
não substituem medidas físicas.

## Coleta e calibração

Guardar aquisições em `data/`, diretório ignorado pelo Git:

```powershell
New-Item -ItemType Directory -Force data
python scripts/telemetry.py data/captura.csv --port COM4 --seconds 60 --raw data/captura.bin
python scripts/telemetry.py data/captura.bin data/decodificada.csv
python scripts/calibrate.py data/xp.csv data/xn.csv data/yp.csv data/yn.csv data/zp.csv data/zn.csv --output .build/CalibrationData.h
```

A abertura da porta pode reiniciar o Mega por DTR. A coleta não envia comandos
de calibração. O ajuste gera um cabeçalho separado e um JSON com identificação,
hashes e parâmetros; revisar ambos, confirmar a montagem e só então integrar a
calibração ao firmware. Seguir o [protocolo de bancada](docs/validacao-bancada.md)
para os ensaios físicos, a gravidade local e a referência angular.

## Manutenção

A formatação C++/Arduino é definida em [.clang-format](.clang-format), com quatro
espaços, blocos explícitos e limite de 100 colunas. Scripts operacionais ficam em
`scripts/`; documentos de apoio permanentes ficam em `docs/`. Ferramentas
temporárias de edição de Office, caches, builds e resultados gerados não fazem
parte dos fontes versionados. Os achados técnicos da revisão estão consolidados
no apêndice C da monografia.
