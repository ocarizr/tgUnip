# Telemetria e comandos

Binário v1, 80 bytes/amostra, little-endian, 115200 baud 8N1, 50 Hz sem downsampling intencional: 4000 bytes/s, cerca de 34,7% do canal. Buffer ocupado gera descarte contado e não bloqueia o estimador. A sequência conta leituras realizadas, não conversões internas perdidas. Timestamp é micros() no fim da leitura, uint32 com rollover; a ferramenta de calibração trata diferença modular. Durante desconexão pode cessar a transmissão; ausência de pacotes também deve ser tratada como falha pelo receptor.

| Bytes | Campo |
|---|---|
| 0–3 | Magic A5 5A, versão 1, estado |
| 4–11 | Sequência uint32, timestamp uint32 em µs |
| 12–25 | int16 ax, ay, az, temperatura, gx, gy, gz brutos |
| 26–53 | float32 qw, qx, qy, qz, bx, by, bz; viés em rad/s |
| 54–55 | Identificação da calibração uint16 |
| 56–71 | uint32 falhas, perdas estimadas, descartes UART, máximo de desenho LCD em µs |
| 72–77 | uint16 flags, config_id=1, duração do burst em µs |
| 78–79 | CRC-16/CCITT-FALSE dos primeiros 78 bytes, little-endian |

Flags: bit0 atitude inicializada; bit1 calibração marcada medida; bit2 override LAB habilitado; bit3 tara ativa. **bit0 não substitui estado nem frescor**: SO GIRO é degradado. Estados: 0 não inicializado; 1 calibrando; 2 fusão; 3 só giro; 4 falha de tempo; 5 sensor; 6 saturação; 7 calibração inválida; 8 LAB. Valores de quatérnio em estado não inicializado/falha não constituem saída válida. Temperatura em °C = temp_raw/340 + 36,53.

```powershell
python scripts/telemetry.py recording.bin output.csv
python scripts/telemetry.py output.csv --port COM4 --seconds 60 --raw recording.bin
```

O segundo comando abre a porta e pode reiniciar o Mega por DTR; não envia comandos. O receptor valida CRC e ressincroniza; guardar também binário original. `tests/replay.cpp` aceita linhas `t_us,ax_g,ay_g,az_g,gx_rad_s,gy_rad_s,gz_rad_s` e exige um trecho estacionário inicial; dados brutos precisam de conversão com a calibração correspondente antes do replay.

## Comandos

Enviar um único caractere ASCII minúsculo pela serial, a 115200 baud:

| Caractere | Efeito |
|---|---|
| `c` | Reconfigura o sensor e inicia a calibração estacionária; manter imóvel por pelo menos três segundos |
| `t` | Armazena a tara das coordenadas X/Y se a atitude estiver válida e atualizada |
| `u` | Remove a tara; a inclinação absoluta permanece disponível |
| `r` | Invalida a atitude; exige novo `c` para inicializar |

A calibração estacionária estima o viés do girômetro. Ela não substitui o ajuste
de seis posições do acelerômetro nem a verificação da montagem. O cabeçalho
nominal entregue impede a operação normal até integrar uma calibração medida.
