# Calibração e parâmetros do MPU-6050

O instrumento utiliza o MPU-6050, uma IMU MEMS com acelerômetro e girômetro de
três eixos. Durante o desenvolvimento original foram realizados ensaios
prolongados em diferentes posições e rotações, em laboratório profissional de
sensores inerciais, sobre superfície com isolamento de vibração e ambiente a
20 °C, conforme relato do autor e a caracterização descrita na monografia.
Os CSVs completos desses ensaios não estão disponíveis neste repositório.
Isso impede recuperar seus coeficientes individuais e repetir a análise dos
registros; não significa que os ensaios históricos não tenham sido realizados.

## Referência de fabricação

Referência: InvenSense, **MPU-6000/MPU-6050 Product Specification**,
PS-MPU-6000A-00, revisão 3.4, 19/08/2013, seções 6.1 e 6.2, páginas 12 e 13.
[Documento do fabricante](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet.pdf)
e [cópia do mesmo documento](https://device.report/m/a4fdf69590b8cbab1cf0a5bcd359327b0ac022061fb233855aff2e191e7e3584).
A cópia foi consultada porque o acesso direto ao fabricante falhou.

| Parâmetro | Valor de referência | Aplicação |
|---|---|---|
| Sensibilidade do acelerômetro, ±2 g | 16384 contagens/g | Conversão nominal |
| Sensibilidade do girômetro, ±250 °/s | 131 contagens/(°/s) | Conversão nominal para rad/s |
| Tolerância inicial de sensibilidade | ±3% | Comparação de plausibilidade do ajuste |
| Offset zero-g X/Y e Z | ±50 e ±80 mg, valores típicos após MSL3 | Motivação da margem de partida; não são máximos garantidos |
| Offset inicial do girômetro a 25 °C | ±20 °/s | Dimensionamento da estimação de viés |
| Variação do offset do giro em −40 a +85 °C | ±20 °/s | Margem conservadora de plausibilidade |

As condições elétricas e de temperatura da tabela do fabricante fazem parte
da especificação. Os ensaios históricos a 20 °C não são ensaios a 25 °C. Não se
interpola uma compensação térmica individual sem dados. Os extremos de catálogo
não são coeficientes de correção nem distribuição estatística de ruído.

## Perfil nominal e partida estacionária

A configuração entregue usa `ALLOW_NOMINAL_CALIBRATION = true`. Com o cabeçalho
nominal, o instrumento inicia com ID zero, `CALIBRATION_MEASURED = false`, escala
nominal e offset acelerométrico assumido zero. Depois da janela estacionária,
exibe **NOMINAL**, ou **NOMINAL SO GIRO** quando rejeita a referência de aceleração.
O perfil nominal é apropriado para demonstrar o algoritmo com a referência de
fábrica, sem declarar a acurácia individual que dependeria dos dados de ensaio.
Definir a opção como `false` exige coeficientes medidos para produzir saída.

Na partida e no comando `c`, manter o sensor fisicamente imóvel. A média de uma
janela de pelo menos três segundos e 150 amostras estima o viés dos três canais
de giro, já em rad/s e no referencial do corpo. Esse viés é subtraído durante a
propagação. Não se estima escala de giro a partir de repouso.

As políticas de partida estão em `Mpu6050Specs.h`. Para o giro, a norma máxima de
plausibilidade é √3 × (20 + 20) + 1 ≈ 70,28 °/s: considera três eixos, a variação
térmica de catálogo e uma margem de engenharia de 1 °/s. Para a aceleração
nominal, aceita-se norma entre 0,85 e 1,15 g; com ajuste medido, a tolerância de
partida permanece 0,08 g. A variância máxima por eixo permanece 0,0004 g² e
0,000025 (rad/s)². Esses gates são heurísticos e não garantias do fabricante.

**Uma rotação constante pode ser confundida com viés**, em especial em torno da
vertical. A verificação de variância não resolve essa ambiguidade. A condição de
repouso deve ser imposta pelo operador. Não se estima o offset dos três eixos de
aceleração de uma única orientação desconhecida, porque isso também confundiria
inclinação real com erro do sensor. O rumo absoluto continua sem referência.

Para gerar uma cópia do perfil nominal com proveniência em JSON:

```powershell
python scripts/calibrate.py --nominal --output .build/CalibrationData.h
```

## Ajuste com seis posições

O caminho para dados recuperados ou novas aquisições continua disponível:

```powershell
python scripts/calibrate.py data/xp.csv data/xn.csv data/yp.csv data/yn.csv data/zp.csv data/zn.csv --g-local 0.9986 --output .build/CalibrationData.h
```

Substituir a gravidade do exemplo pelo valor justificado para o local. Os arquivos
devem corresponder às faces +X, −X, +Y, −Y, +Z e −Z do sensor, com os outros eixos
próximos de zero. Para cada eixo, o offset é a média dos extremos e a
sensibilidade é sua diferença dividida por duas vezes a gravidade local em g.

O ajuste verifica configuração, sequência, timestamps com rollover, duração,
saturação, variância, estabilidade térmica e resíduos nas faces. A diferença das
médias de giro entre as faces deve ser ≤0,5 °/s por eixo, uma política de
consistência. A temperatura interna não calibrada serve para avaliar variação
relativa; não certifica a temperatura do laboratório. Desvios da sensibilidade
de catálogo e dos offsets típicos são registrados como avisos, sem transformar
valores típicos em limites máximos de rejeição.

O JSON preserva hashes, estatísticas das janelas, resíduos, referência do manual
e a gravidade adotada. A identificação incorpora os hashes e a gravidade. Revisar
o relatório e a montagem antes de integrar o cabeçalho. `CALIBRATION_MEASURED`
identifica o ajuste acelerométrico feito com entradas medidas; não certifica a
referência usada nem indica escala giroscópica ou montagem calibradas. O programa
não sobrescreve os fontes por padrão. Dados e resultados locais ficam em `data/`
e `.build/`, ignorados pelo Git.

## Verificação

Os testes sintéticos cobrem sensibilidades de ±3%, offsets típicos, viés de giro
de ±20 °/s, ajuste das seis faces e rejeição de registros inválidos. A simulação
e os testes do firmware não são os CSVs históricos e não substituem uma
reavaliação metrológica. A validação física da revisão dinâmica permanece
descrita no [protocolo de bancada](validacao-bancada.md).
