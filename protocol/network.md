# Network Behavior

Documentação das regras de comportamento da rede CANarinho.

Este documento define:

- comportamento esperado dos nós;
- estratégias de sincronização;
- roteamento;
- tolerância a falhas;
- heartbeat;
- recuperação após reboot;
- gerenciamento de tráfego;
- filtragem de mensagens CAN.

O objetivo é garantir uma rede distribuída, resiliente e desacoplada.

---

# Filosofia da rede

A rede CANarinho é descentralizada.

Os nós devem continuar operando normalmente mesmo na ausência do gateway Home Assistant.

---

## Princípios

- controle direto entre nós é preferível;
- estados são públicos;
- automações locais devem continuar funcionando sem gateway;
- o gateway atua preferencialmente como observador passivo;
- polling deve ser evitado;
- mudanças devem ser publicadas espontaneamente.

---

# Comunicação

## Unicast

Mensagens direcionadas a um único nó.

Utilizado para:

- comandos;
- queries;
- configurações.

---

## Broadcast

Mensagens públicas para toda a rede.

Utilizado para:

- estados (`STA`);
- heartbeats (`HB`);
- alarmes (`ALR`).

---

# Publicação de estados

Todo nó que mantém estado persistente deve publicar:

```text
TYPE=STA
DST=0xFF
```

após qualquer alteração relevante.

Regra de persistência v1:

- apenas estados de `outputs` devem ser persistidos no nó;
- sensores não exigem persistência local.

---

## Mudanças que exigem publicação

- comando recebido;
- alteração física local;
- reboot;
- recuperação de falha;
- sincronização.

---

## Objetivos

- sincronização automática;
- múltiplos ouvintes;
- desacoplamento;
- recuperação simplificada;
- gateway passivo.

---

# Eventos

Eventos representam acontecimentos momentâneos.

Eventos não substituem estados persistentes.

---

## Exemplos de eventos

- botão pressionado;
- long press;
- sensor PIR disparado;
- encoder rotacionado.

---

## Fluxo típico

```text
Input Node → EVT → Gateway
Gateway → CMD → Outros nós
```

---

# Query e sincronização

## Query simples

Solicita o estado de um único canal.

```text
TYPE=QRY
CHANNEL != 0x00
```

---

## Query global

Solicita sincronização completa do nó.

```text
TYPE=QRY
CHANNEL=0x00
```

---

## Resposta esperada

O nó deve responder publicando múltiplos frames `STA` contendo o estado atual de todos os canais relevantes.

---

## Objetivos

- sincronização inicial;
- recuperação após reboot;
- descoberta prática de capabilities;
- atualização completa de estado.

---

## Exemplo

Gateway solicita sincronização:

```text
SRC=0xFE DST=0x40 TYPE=QRY CH=0x00
```

Nó responde:

```text
STA CH=0x20 → tensão
STA CH=0x21 → corrente
STA CH=0x22 → potência
```

---

# Inicialização do gateway

Quando o gateway iniciar:

1. começa a escutar a rede;
2. limpa estados expirados;
3. envia `QRY CH=0x00` para nós conhecidos;
4. aguarda republicação dos estados;
5. reconstrói estado global.

---

## Objetivo

Permitir recuperação completa sem polling contínuo.

---

# Reboot de nós

Após reboot, um nó deve:

1. inicializar hardware;
2. restaurar estado persistente quando aplicável;
3. iniciar heartbeat;
4. republicar estados relevantes.

---

# Heartbeat

Todos os nós ativos devem publicar heartbeat periodicamente.

---

## Estrutura

```text
TYPE=HB
CHANNEL=0x00
DST=0xFF
```

---

## Intervalos recomendados

| Evento | Valor |
|---|---|
| Intervalo heartbeat | até 30s |
| Timeout offline | 3x o intervalo de heartbeat configurado no nó |

Observações:

- cada nó deve enviar `HB`, com intervalo configurável;
- o intervalo pode ser `30s` ou menor;
- no boot, o nó deve enviar `HB` após inicializar a comunicação.

---

## Objetivos

- detectar nós offline;
- monitorar estabilidade;
- verificar saúde da rede.

---

# Nó offline

Um nó é considerado offline quando:

```text
não houver nenhuma mensagem do nó por 3x o intervalo de heartbeat configurado
```

Esse timeout funciona como watchdog de presença do nó.
Qualquer frame válido recebido do nó (não apenas `HB`) renova o temporizador de presença.

---

## Comportamento esperado do gateway

Quando um nó ficar offline:

- marcar entidade indisponível;
- preservar último estado conhecido;
- aguardar recuperação automática.

---

## Recuperação

Quando um heartbeat (ou qualquer frame válido do nó) voltar a ser recebido:

1. nó é marcado online;
2. gateway pode solicitar sincronização completa.

---

# Tratamento de erros

## Payload inválido

Frames com payload inválido devem ser ignorados.

---

## Canal inexistente

Caso um nó receba:

- canal inexistente;
- comando não suportado;
- payload inválido;

o frame deve ser ignorado silenciosamente.

---

## Flood de erro

Nós não devem gerar floods de mensagens de erro.

---

## ACK de erro

Opcionalmente um nó pode responder:

```text
TYPE=ACK
```

com código de erro apropriado.

Para política de confirmação de comandos (`CMD`), consultar `message_types.md`.

---

# Estratégia de tráfego

O barramento CAN deve ser utilizado de forma eficiente.

---

## Recomendações

- evitar polling contínuo;
- preferir publicação por mudança;
- evitar broadcasts desnecessários;
- evitar mensagens redundantes;
- evitar heartbeats muito frequentes.

---

## Publicação por threshold

Sensores podem publicar apenas quando:

- houver mudança relevante;
- threshold mínimo for atingido;
- timeout periódico expirar.

---

# Jitter

Quando múltiplos nós responderem simultaneamente, recomenda-se adicionar jitter baseado no endereço do nó.

---

## Objetivo

Evitar bursts excessivos no barramento.

---

## Exemplo

```text
delay_ms = node_id * 2
```

---

# Filtragem CAN

Os nós devem filtrar mensagens irrelevantes sempre que possível.

---

## Mensagens que devem ser aceitas

Um nó normalmente deve aceitar:

- mensagens destinadas ao próprio endereço;
- broadcast (`DST=0xFF`);

---

# Gateway filters

O gateway normalmente deve aceitar:

- mensagens destinadas a `0xFE`;
- broadcasts;

---

## Exemplo

```text
Filtro 1:
DST=0xFE

Filtro 2:
DST=0xFF
```

---

# Independência do gateway

A ausência do gateway não deve impedir:

- controle direto entre nós;
- automações locais;
- funcionamento básico da residência.

---

# Prioridade da rede

Mensagens críticas devem possuir prioridade CAN mais alta.

---

## Ordem recomendada

| Prioridade | Uso |
|---|---|
| `0` | alarmes |
| `3` | comandos/eventos |
| `5` | estados |
| `7` | heartbeat |

---

# Fluxos típicos

## Controle direto

```text
Botão → CMD → Relé
Relé → STA broadcast
```

---

## Automação Home Assistant

```text
Botão → EVT → Gateway
Gateway → CMD → Nós
Nós → STA broadcast
```

---

## Recuperação após reboot do gateway

```text
Gateway inicia
→ envia QRY global
→ nós republicam STA
→ estados reconstruídos
```

---

# Escalabilidade

A rede foi projetada para:

- baixo acoplamento;
- múltiplos ouvintes;
- diferentes tipos de nós;
- tolerância a falhas.

---

# Futuras extensões

Possíveis melhorias futuras:

- discovery automático;
- OTA via CAN;
- configuração remota;
