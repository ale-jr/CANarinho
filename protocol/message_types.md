# Estrutura de mensagens

Documentação dos tipos de mensagens utilizados no protocolo CANarinho.

Cada frame CAN possui um campo `TYPE` de 4 bits responsável por definir o comportamento e semântica do payload.

---

# Estrutura

```text
Bit: 9      6
     [ TYPE ]
        4b
```

---

# Tabela de tipos

| Código | Nome | Sigla | Descrição |
|---|---|---|---|
| `0x0` | Command | CMD | Envia comandos para um nó |
| `0x1` | Status | STA | Publica estado atual |
| `0x2` | Event | EVT | Eventos instantâneos |
| `0x3` | Query | QRY | Solicita estado |
| `0x4` | Acknowledge | ACK | Confirma recebimento |
| `0x5` | Heartbeat | HB | Sinal periódico de vida |
| `0x6` | Alarm | ALR | Evento crítico |

---

# Tamanho esperado dos payloads

| Tipo | DLC esperado |
|---|---|
| CMD | 3 bytes |
| STA | variável |
| EVT | 2 bytes |
| QRY | 0 bytes |
| ACK | 0–1 byte |
| HB | 0 bytes |
| ALR | variável |

---

# Endianness

Todos os valores multi-byte utilizam:

```text
Little-endian
```

Exemplo:

```text
23.5°C = 235 = 0x00EB

Payload:
[0xEB, 0x00]
```

---

# CMD — Command

Mensagens de comando utilizadas para controlar dispositivos.

## Características

- Destino normalmente é um nó específico
- Pode alterar estado
- Pode gerar um `STA` após execução

## Exemplos

- ligar relé
- desligar luz
- alterar temperatura
- alterar velocidade de ventilação

## Payload padrão — relés/dimmers

| Byte | Campo | Valores |
|---|---|---|
| `0` | Ação | off/on/toggle/set_level |
| `1` | Valor | 0–255 |
| `2` | Timer | múltiplos de 10s |

### Ações

| Valor | Ação |
|---|---|
| `0x00` | Off |
| `0x01` | On |
| `0x02` | Toggle |
| `0x03` | Set level |

### Exemplo

```text
SRC=0x10 DST=0x22 TYPE=CMD CH=0x01
Payload: [0x01, 0xFF, 0x00]
```

Interpretação:

```text
Ligar saída 0x01 do nó 0x22 em 100%.
```

---

# STA — Status

Mensagens de estado.

Todo estado publicado deve ser considerado a fonte de verdade da rede.

## Características

- Sempre broadcast (`DST=0xFF`)
- Emitido após mudanças
- Pode ser periódico
- Gateway apenas escuta

## Exemplos

- estado de relé
- temperatura
- posição de janela
- set point do ar-condicionado

## Payload — sensores simples

| Byte | Campo | Descrição |
|---|---|---|
| `0-1` | Valor | int16 em décimos |
| `2` | Flags | bateria baixa / erro |

## Flags

| Bit | Significado |
|---|---|
| `0` | bateria baixa |
| `1` | sensor_fault |

## Exemplo

```text
SRC=0x22 DST=0xFF TYPE=STA CH=0x01
Payload: [0x01, 0xFF, 0x00]
```

---

# EVT — Event

Eventos instantâneos normalmente gerados por interação humana.

## Características

- Não representa estado persistente
- Pode disparar automações
- Normalmente enviado ao gateway

## Exemplos

- botão pressionado
- double click
- long press
- sensor PIR disparado

## Payload — botões

| Byte | Campo | Valores |
|---|---|---|
| `0` | Tipo | press/release/long/double |
| `1` | Duração | unidades de 100ms |

## Eventos

| Valor | Evento |
|---|---|
| `0x01` | Press |
| `0x02` | Release |
| `0x03` | Long press |
| `0x04` | Double press |

## Exemplo

```text
SRC=0x15 DST=0xFE TYPE=EVT CH=0x10
Payload: [0x01, 0x00]
```

---

# QRY — Query

Solicita estado de um endpoint.

## Características

- Direcionado
- Utilizado para sincronização
- Resposta normalmente ocorre via `STA`

## Payload

```text
Sem payload
```

## Exemplos

- solicitar temperatura atual
- solicitar estado do relé
- ressincronização após reboot do gateway

## Exemplo

```text
SRC=0xFE DST=0x30 TYPE=QRY CH=0x03
```

Interpretação:

```text
Gateway solicitando set point do ar-condicionado.
```

---

# ACK — Acknowledge

Confirmação explícita de recebimento.

## Características

- Obrigatório para `CMD` que altera estado persistente de `output`
- Opcional para `CMD` efêmero/não crítico
- Não necessário para mensagens periódicas

## Política de ACK para `CMD` (v1)

- `CMD` que altera output persistente: receptor deve responder com `ACK`.
- `CMD` sem impacto persistente: `ACK` pode ser omitido para economizar barramento.
- Em erro de validação/processamento, quando houver `ACK`, usar código de erro apropriado.

## Possíveis usos

- configuração persistente
- OTA
- provisionamento
- comandos críticos

## Payload

| Byte | Campo | Descrição |
|---|---|---|
| `0` | Status | sucesso/erro |

## Status

| Valor | Significado |
|---|---|
| `0x00` | OK |
| `0x01` | Erro genérico |
| `0x02` | Canal inválido |
| `0x03` | Payload inválido |
| `0x04` | Comando não suportado |

## Exemplo

```text
SRC=0x22 DST=0x10 TYPE=ACK CH=0x01
Payload: [0x00]
```

---

# HB — Heartbeat

Sinal periódico de vida do nó.

## Características

- Sempre broadcast
- Canal sempre `0x00`
- Baixa prioridade
- Sem payload obrigatório

## Objetivos

- detectar nós offline
- monitorar estabilidade
- sincronização básica

## Payload

```text
Sem payload
```

## Timing recomendado

| Evento | Valor |
|---|---|
| Intervalo heartbeat | até 30s (ou menor) |
| Timeout offline | 3x o intervalo de heartbeat configurado |

## Exemplo

```text
SRC=0x22 DST=0xFF TYPE=HB CH=0x00
```

---

# ALR — Alarm

Mensagens críticas de falha ou condição importante.

## Características

- Prioridade CAN máxima
- Broadcast obrigatório
- Deve ser raro

## Exemplos

- sobrecorrente
- superaquecimento
- falha de alimentação
- watchdog reset
- falha crítica de hardware

## Payload sugerido

| Byte | Campo | Descrição |
|---|---|---|
| `0` | Código | tipo do alarme |
| `1+` | Dados | opcionais |

## Exemplo

```text
SRC=0x22 DST=0xFF TYPE=ALR CH=0x00
Payload: [0x02]
```

---

# Tratamento de erros

## Payload inválido

Frames com payload inválido devem ser ignorados.

Opcionalmente o nó pode:

- incrementar contador interno de erro;
- emitir `ALR`;
- registrar log local.

---

## Canal inexistente

Caso um nó receba:

- canal inexistente;
- comando não suportado;
- payload inválido;

o frame deve ser ignorado silenciosamente.

Opcionalmente pode responder:

```text
ACK erro
```

---

# Convenções

## Estados são públicos

Todo `STA` deve utilizar:

```text
DST = 0xFF
```

Isso permite:

- sincronização automática;
- múltiplos ouvintes;
- menor acoplamento;
- gateway passivo.

---

## Eventos não substituem estado

`EVT` representa apenas acontecimentos momentâneos.

Estados persistentes devem utilizar `STA`.

---

## Heartbeats usam canal global

Heartbeats sempre utilizam:

```text
CHANNEL = 0x00
```

---

# Fluxos típicos

## Controle direto

```text
CMD → nó
STA → broadcast
```

---

## Automação

```text
EVT → gateway
CMD → múltiplos nós
STA → broadcast
```

---

## Inicialização

```text
QRY → nó
STA → broadcast
```

---

# Futuras extensões

Possíveis tipos futuros:

| Código | Uso |
|---|---|
| `0x7` | OTA |
| `0x8` | Discovery |
| `0x9` | Configuração |
| `0xA` | Streaming |
