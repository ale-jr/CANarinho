# Tipos de nó

Documentação dos tipos de nós utilizados na rede CANarinho.

Cada nó possui um endereço único na rede CAN e pode implementar um ou mais tipos de comportamento.

O protocolo não restringe rigidamente a função dos nós, mas define convenções para manter interoperabilidade e previsibilidade.

---

# Tipos de nó

| Tipo | Função principal |
|---|---|
| Input Node | Geração de eventos e comandos |
| Output Node | Controle de cargas e atuadores |
| Sensor Node | Publicação de medições |
| Mixed Node | Combinação de múltiplas funções |
| Gateway Node | Integração com Home Assistant |

---

# Convenções gerais

## Endereço único

Cada nó deve possuir um endereço único:

```text
0x01 – 0xFD
```

---

## Canal `0x00`

Reservado para funções globais do nó:

- heartbeat;
- alarmes;
- status global;
- sincronização completa.

---

## Publicação de estado

Nós que mantêm estado persistente devem publicar:

```text
STA → DST=0xFF
```

após qualquer mudança relevante.

---

## Heartbeat

Todos os nós ativos devem emitir:

```text
TYPE=HB
CHANNEL=0x00
```

periodicamente.

---

# Query global

Quando um nó receber:

```text
TYPE=QRY
CHANNEL=0x00
```

ele deve publicar o estado atual de todos os canais relevantes através de múltiplos frames `STA`.

---

## Objetivos

- sincronização inicial do gateway;
- ressincronização após reboot;
- descoberta prática de capabilities;
- atualização completa de estado.

---

## Exemplo

Gateway solicita sincronização completa:

```text
SRC=0xFE DST=0x40 TYPE=QRY CH=0x00
```

Sensor responde com múltiplos `STA`:

```text
SRC=0x40 DST=0xFF TYPE=STA CH=0x20
Payload: tensão

SRC=0x40 DST=0xFF TYPE=STA CH=0x21
Payload: corrente

SRC=0x40 DST=0xFF TYPE=STA CH=0x22
Payload: potência
```

---

# Input Node

Nós responsáveis por gerar comandos ou eventos.

Normalmente utilizados para:

- botões;
- interruptores;
- teclados;
- sensores de presença;
- interfaces físicas.

---

## Características

- normalmente não possuem estado persistente;
- podem enviar `CMD`;
- podem enviar `EVT`;
- geralmente não recebem comandos;
- baixo tráfego.

---

## Casos de uso

| Dispositivo | Exemplo |
|---|---|
| Botão de parede | ligar luz |
| Controle físico | alterar temperatura |
| Sensor PIR | disparar automação |

---

## Fluxo típico

```text
Input Node
    ↓ CMD / EVT
Outro nó ou gateway
```

---

## Exemplo — controle direto

Botão controlando luz diretamente:

```text
SRC=0x10 DST=0x22 TYPE=CMD CH=0x01
Payload: [0x01, 0xFF, 0x00]
```

---

## Exemplo — automação via gateway

Botão enviando evento para o Home Assistant:

```text
SRC=0x15 DST=0xFE TYPE=EVT CH=0x10
Payload: [0x01, 0x00]
```

Interpretação:

```text
Botão pressionado.

O gateway recebe o evento e o Home Assistant dispara
uma automação.
```

---

# Output Node

Nós responsáveis por controlar atuadores físicos.

Normalmente utilizados para:

- relés;
- dimmers;
- LEDs;
- motores;
- válvulas;
- cargas em geral.

---

## Características

- recebem `CMD`;
- publicam `STA`;
- mantêm estado persistente;
- podem possuir controle físico local.

---

## Comportamento esperado

Após qualquer mudança:

- comando CAN;
- botão físico;
- recuperação após reboot;

o nó deve publicar:

```text
STA → DST=0xFF
```

---

## Casos de uso

| Dispositivo | Exemplo |
|---|---|
| Relé | iluminação |
| Dimmer | controle de intensidade |
| Driver MOSFET | fita LED |
| Ventilador | controle PWM |

---

## Fluxo típico

```text
CMD → Output Node
STA → Broadcast
```

---

## Exemplo

```text
SRC=0xFE DST=0x22 TYPE=CMD CH=0x01
Payload: [0x01, 0xFF, 0x00]
```

---

# Sensor Node

Nós responsáveis por medições e telemetria.

Normalmente utilizados para:

- temperatura;
- umidade;
- abertura;
- luminosidade;
- consumo elétrico;
- monitoramento energético.

---

## Características

- normalmente apenas transmitem;
- publicam `STA`;
- podem operar por mudança ou periodicidade;

---

## Estratégias de publicação

### Periódica

```text
Publica a cada intervalo fixo.
```

### Por mudança

```text
Publica apenas quando o valor muda acima de um threshold.
```

---

## Casos de uso

| Sensor | Exemplo |
|---|---|
| Temperatura | ambiente |
| Reed switch | porta/janela |
| PZEM | monitoramento elétrico |

---

## Sensores complexos

Sensores que possuem múltiplos parâmetros devem utilizar múltiplos canais independentes.

Cada atributo é tratado como um endpoint separado.

---

## Exemplo — PZEM

| Canal | Medição |
|---|---|
| `0x20` | tensão |
| `0x21` | corrente |
| `0x22` | potência |
| `0x23` | energia acumulada |
| `0x24` | frequência |
| `0x25` | fator de potência |

---

## Vantagens

- atualizações granulares;
- menor tráfego CAN;
- queries independentes;
- melhor integração com Home Assistant.

---

## Exemplo

```text
SRC=0x40 DST=0xFF TYPE=STA CH=0x20
Payload: [0xEB, 0x00, 0x00]
```

---

# Mixed Node

Nós que combinam múltiplas funções.

Um mesmo nó pode simultaneamente:

- receber comandos;
- controlar saídas;
- publicar sensores;
- gerar eventos.

---

## Casos de uso

| Dispositivo | Funções |
|---|---|
| Termostato | sensor + botões |
| Interruptor inteligente | relé + botão |
| Painel de parede | display + sensores + inputs |

---

## Estratégia recomendada

Cada endpoint deve possuir:

- canal dedicado;
- semântica independente;
- payload consistente.

---

## Exemplo — termostato

| Canal | Função |
|---|---|
| `0x01` | temperatura |
| `0x10` | botão + |
| `0x11` | botão - |
| `0x12` | modo |

---

# Gateway Node

Nó responsável pela integração da rede CANarinho com o Home Assistant.

---

## Endereço reservado

```text
0xFE
```

---

## Características

- escuta toda a rede;
- normalmente opera de forma passiva;
- envia `CMD` quando necessário;
- pode realizar sincronização;
- mantém estado global.

---

## Casos de uso

- integração com Home Assistant;
- automações;
- logging;
- debug da rede.

---

## Responsabilidades

- descoberta de estados;
- monitoramento de heartbeats;
- marcação de nós offline;
- integração com automações;
- logging e diagnóstico.

---

## Fluxo típico

```text
EVT → Gateway
Gateway → CMD
Nós → STA broadcast
```

---

# Organização de canais

Os canais são definidos individualmente por cada nó.

A semântica do canal depende exclusivamente do firmware implementado naquele dispositivo.

---

# Convenções

## Estados devem ser públicos

Todo estado persistente deve utilizar:

```text
DST=0xFF
TYPE=STA
```

---

## Eventos não substituem estados

Eventos representam acontecimentos momentâneos.

Estados persistentes devem utilizar `STA`.

---

## Nós devem tolerar reboot do gateway

A rede deve continuar operando normalmente sem o gateway.

Controle direto entre nós deve permanecer funcional.

---

# Futuras extensões

Possíveis tipos futuros de nó:

- display node;