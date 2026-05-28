# Payload Contracts (v1)

Contrato de payload por dispositivo para a rede CANarinho.

Este arquivo complementa a documentação geral do protocolo.

Objetivo:

- definir payloads concretos por `TYPE + CHANNEL` de cada dispositivo;
- manter o protocolo de rede flexível sem engessar o mapa global de canais;
- alinhar firmware de nós e gateway (registry/comissionamento).

---

# Princípios

- o mapa de canais é definido por dispositivo;
- `CMD` ponto a ponto precisa conhecer apenas o canal correto no nó alvo;
- o gateway aprende/usa os canais via `registry` de comissionamento;
- não há discovery automático nesta versão.

---

# Template por dispositivo

Preencher uma seção como esta para cada modelo/tipo de nó.

## Device

```text
name: <nome do dispositivo>
node_type: <input|output|sensor|mixed>
heartbeat_interval_s: <valor do registry, <= 30>
offline_timeout_s: 3x heartbeat_interval_s
```

## Canais

| CH | Tipo | Direção | Persistente | Payload | Unidade/escala | Observações |
|---|---|---|---|---|---|---|
| `0x01` | CMD | RX | sim/não | bytes e semântica | quando aplicável | regra de negócio |
| `0x01` | STA | TX | sim/não | bytes e semântica | quando aplicável | publicado após mudança |
| `0x10` | EVT | TX | não | bytes e semântica | n/a | evento instantâneo |

Direção:

- `RX`: nó recebe esse frame;
- `TX`: nó publica esse frame.

Persistência:

- apenas estados de output devem ser persistidos;
- sensores não exigem persistência local.

## Regras de ACK (CMD)

- `CMD` com alteração de estado persistente: `ACK` obrigatório;
- `CMD` efêmero/não crítico: `ACK` opcional.

## Códigos de ACK usados no dispositivo

| Valor | Nome | Quando usar |
|---|---|---|
| `0x00` | OK | comando aplicado com sucesso |
| `0x01` | ERR_GENERIC | falha não classificada |
| `0x02` | ERR_CHANNEL | canal inválido/inexistente |
| `0x03` | ERR_PAYLOAD | payload inválido |
| `0x04` | ERR_UNSUPPORTED | comando não suportado |

---

# Exemplo rápido (output dimmer)

```text
name: dimmer_2ch_v1
node_type: output
heartbeat_interval_s: 10
offline_timeout_s: 30
```

| CH | Tipo | Direção | Persistente | Payload | Unidade/escala | Observações |
|---|---|---|---|---|---|---|
| `0x01` | CMD | RX | sim | `[acao, valor, timer]` | valor: 0-255 | controla canal 1 |
| `0x01` | STA | TX | sim | `[estado, nivel, flags]` | nivel: 0-255 | broadcast após mudança |
| `0x02` | CMD | RX | sim | `[acao, valor, timer]` | valor: 0-255 | controla canal 2 |
| `0x02` | STA | TX | sim | `[estado, nivel, flags]` | nivel: 0-255 | broadcast após mudança |

---

# Integração com registry do gateway

O `registry` deve conter, no mínimo:

- `node_id`;
- `device_type`/`profile`;
- mapeamento de `channels`;
- `heartbeat_interval_s`;
- parâmetros de interpretação de payload (quando necessário).

