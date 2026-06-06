# Estrutura de IDs

Documentação da estrutura de IDs CAN utilizados pelo projeto CANarinho.

O protocolo utiliza **CAN 2.0B com IDs estendidos de 29 bits** para permitir roteamento, priorização e segmentação dos endpoints dos nós.

---

# Estrutura do ID

```text
Bit: 28      26 25              18 17              10 9      6 5        0
     [Prioridade] [   SRC (8 bits)  ] [   DST (8 bits)  ] [ Tipo ] [ Canal ]
          3b              8b                  8b              4b        6b
```

| Campo | Bits | Tamanho | Descrição |
|---|---|---|---|
| Prioridade | 28–26 | 3 bits | Prioridade CAN (0 = maior prioridade) |
| SRC | 25–18 | 8 bits | Endereço do nó de origem |
| DST | 17–10 | 8 bits | Endereço do nó de destino |
| Tipo | 9–6 | 4 bits | Tipo da mensagem |
| Canal | 5–0 | 6 bits | Endpoint dentro do nó |

---

# Layout binário

```text
 28          26 25              18 17              10 9       6 5       0
+--------------+------------------+------------------+----------+---------+
| PRIORITY     | SRC              | DST              | TYPE     | CHANNEL |
+--------------+------------------+------------------+----------+---------+
```

---

# Prioridades

Menor valor = maior prioridade no barramento CAN.

| Valor | Uso |
|---|---|
| `0` | Alarmes críticos |
| `1` | Reservado |
| `2` | Reservado |
| `3` | Comandos e eventos |
| `4` | Reservado |
| `5` | Status periódico |
| `6` | Reservado |
| `7` | Heartbeat / menor prioridade |

---

# Endereços de nós

| Endereço | Uso |
|---|---|
| `0x00` | Reservado |
| `0x01`–`0xFD` | Nós da rede |
| `0xFE` | Gateway Home Assistant |
| `0xFF` | Broadcast |

---

# Tipos de mensagem

| Código | Nome | Sigla |
|---|---|---|
| `0x0` | Command | CMD |
| `0x1` | Status | STA |
| `0x2` | Event | EVT |
| `0x3` | Query | QRY |
| `0x4` | Acknowledge | ACK |
| `0x5` | Heartbeat | HB |
| `0x6` | Alarm | ALR |

---

# Canais

Os canais representam endpoints internos do nó.

Cada nó define livremente seus canais conforme sua função.

| Canal | Uso |
|---|---|
| `0x00` | Escopo do nó (HB, ALR, status global) |
| `0x01+` | Endpoints específicos |

Exemplo:

| Canal | Função |
|---|---|
| `0x01` | Relé 1 |
| `0x02` | Relé 2 |
| `0x03` | Botão 1 |

---

# Regras de roteamento

| Tipo | Destino esperado |
|---|---|
| CMD | Nó específico |
| QRY | Nó específico |
| EVT | Nó específico ou gateway (`0xFE`) |
| STA | Broadcast (`0xFF`) |
| HB | Broadcast (`0xFF`) |
| ALR | Broadcast (`0xFF`) |

---

# Exemplo de frame

## Exemplo

```text
Prioridade: 3
SRC:         0x10
DST:         0x22
Tipo:        CMD (0x0)
Canal:       0x01
```

## Frame lógico

```text
SRC=0x10 DST=0x22 TYPE=CMD CH=0x01
```

## Interpretação

```text
Nó 0x10 enviando um comando para o canal 0x01 do nó 0x22.
```

---

# Montagem do ID

## Fórmula

```c
uint32_t can_id =
    ((priority & 0x7) << 26) |
    ((src      & 0xFF) << 18) |
    ((dst      & 0xFF) << 10) |
    ((type     & 0xF) << 6) |
    ((channel  & 0x3F));
```

---

# Extração dos campos

## Prioridade

```c
(priority) = (can_id >> 26) & 0x7;
```

## SRC

```c
(src) = (can_id >> 18) & 0xFF;
```

## DST

```c
(dst) = (can_id >> 10) & 0xFF;
```

## Tipo

```c
(type) = (can_id >> 6) & 0xF;
```

## Canal

```c
(channel) = can_id & 0x3F;
```

---

# Convenções

- IDs CAN utilizam formato estendido (29 bits)
- Todos os estados (`STA`) devem ser broadcast
- Heartbeats devem utilizar canal `0x00`
- Alarmes devem utilizar prioridade `0`
- O gateway deve operar passivamente sempre que possível


# Futuras extensões

Possíveis expansões futuras do protocolo:

- Segmentação de payloads longos
- OTA via CAN
- Discovery automático de nós
- Versionamento de capabilities
- Provisionamento automático
- Criptografia/autenticação