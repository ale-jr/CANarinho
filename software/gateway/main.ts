import { SerialPort } from 'serialport';

const PROTO_VERSION = 0x01;

enum MsgType {
  CAN_FRAME = 0x01,
  CTRL_SET_MODE = 0x02,
  CTRL_GET_STATUS = 0x03,
  STATUS = 0x04,
  CTRL_ACK = 0x05,
  CTRL_ERR = 0x06,
}

enum Direction {
  CAN_TO_PC = 0,
  PC_TO_CAN = 1,
}

enum ModeByte {
  strict = 0,
  monitor = 1,
}

enum CtrlErrCode {
  ERR_BAD_FORMAT = 0x01,
  ERR_BAD_DIR = 0x02,
  ERR_BAD_DLC = 0x03,
  ERR_BAD_CAN_ID = 0x04,
  ERR_TWAI_TX_FAIL = 0x05,
  ERR_UNSUPPORTED = 0x06,
}

type GatewayMode = keyof typeof ModeByte;

interface CanIdFields {
  priority: number;
  src: number;
  dst: number;
  msgType: number;
  channel: number;
}

interface MessageBase {
  raw: Buffer;
  version: number;
  msgType: MsgType | number;
  type: string;
  receivedAt: number;
}

export interface CanFrameMessage extends MessageBase, CanIdFields {
  type: 'CAN_FRAME';
  dir: Direction | number;
  dirName: string;
  flags: {
    extended: boolean;
    rtr: boolean;
  };
  canId: number;
  dlc: number;
  data: Buffer;
  seq: number | null;
}

export interface StatusMessage extends MessageBase {
  type: 'STATUS';
  mode: GatewayMode;
  statusCounters: {
    rx_can: number;
    tx_can: number;
    drop_filter: number;
    rx_serial_err: number;
    tx_can_err: number;
  };
}

export interface CtrlAckMessage extends MessageBase {
  type: 'CTRL_ACK';
  seq: number;
}

export interface CtrlErrMessage extends MessageBase {
  type: 'CTRL_ERR';
  seq: number;
  errorCode: number;
  errorName: string;
}

export type GatewayMessage =
  | CanFrameMessage
  | StatusMessage
  | CtrlAckMessage
  | CtrlErrMessage
  | MessageBase;

export interface ConnectOptions {
  port: string;
  baudRate?: number;
}

export interface SendCanFrameParts {
  canId: number;
  data?: Buffer | Uint8Array | number[];
  extended?: boolean;
  rtr?: boolean;
  dir?: Direction;
}

export interface SendCanFrameOptions {
  timeoutMs?: number;
}

export interface SendTestMessageOptions extends SendCanFrameOptions {
  canId?: number;
  data?: Buffer | Uint8Array | number[];
  extended?: boolean;
  rtr?: boolean;
  dir?: Direction;
  label?: string;
}

const MSG_TYPE_NAME: Record<number, string> = {
  [MsgType.CAN_FRAME]: 'CAN_FRAME',
  [MsgType.CTRL_SET_MODE]: 'CTRL_SET_MODE',
  [MsgType.CTRL_GET_STATUS]: 'CTRL_GET_STATUS',
  [MsgType.STATUS]: 'STATUS',
  [MsgType.CTRL_ACK]: 'CTRL_ACK',
  [MsgType.CTRL_ERR]: 'CTRL_ERR',
};

const DIRECTION_NAME: Record<number, string> = {
  [Direction.CAN_TO_PC]: 'CAN_TO_PC',
  [Direction.PC_TO_CAN]: 'PC_TO_CAN',
};

const CTRL_ERR_NAME: Record<number, string> = {
  [CtrlErrCode.ERR_BAD_FORMAT]: 'ERR_BAD_FORMAT',
  [CtrlErrCode.ERR_BAD_DIR]: 'ERR_BAD_DIR',
  [CtrlErrCode.ERR_BAD_DLC]: 'ERR_BAD_DLC',
  [CtrlErrCode.ERR_BAD_CAN_ID]: 'ERR_BAD_CAN_ID',
  [CtrlErrCode.ERR_TWAI_TX_FAIL]: 'ERR_TWAI_TX_FAIL',
  [CtrlErrCode.ERR_UNSUPPORTED]: 'ERR_UNSUPPORTED',
};

interface PendingRequest {
  resolve: (value: CtrlAckMessage) => void;
  reject: (reason?: Error) => void;
}

function packU16LE(v: number): Buffer {
  const b = Buffer.alloc(2);
  b.writeUInt16LE(v & 0xffff, 0);
  return b;
}

function packU32LE(v: number): Buffer {
  const b = Buffer.alloc(4);
  b.writeUInt32LE(v >>> 0, 0);
  return b;
}

function unpackCanIdFields(canId: number): CanIdFields {
  return {
    priority: (canId >>> 26) & 0x7,
    src: (canId >>> 18) & 0xff,
    dst: (canId >>> 10) & 0xff,
    msgType: (canId >>> 6) & 0xf,
    channel: canId & 0x3f,
  };
}

function cobsEncode(input: Buffer): Buffer {
  if (input.length === 0) return Buffer.from([0x01]);

  const out: number[] = [];
  let codeIndex = 0;
  out.push(0);
  let code = 1;

  for (let i = 0; i < input.length; i++) {
    const byte = input[i] as number;
    if (byte === 0) {
      out[codeIndex] = code;
      codeIndex = out.length;
      out.push(0);
      code = 1;
    } else {
      out.push(byte);
      code++;
      if (code === 0xff) {
        out[codeIndex] = code;
        codeIndex = out.length;
        out.push(0);
        code = 1;
      }
    }
  }

  out[codeIndex] = code;
  return Buffer.from(out);
}

function cobsDecode(input: Buffer): Buffer {
  if (input.length === 0) throw new Error('COBS empty frame');

  const out: number[] = [];
  let i = 0;
  while (i < input.length) {
    const code = input[i++] as number;
    if (code === 0) throw new Error('COBS invalid code 0');

    for (let j = 1; j < code; j++) {
      if (i >= input.length) throw new Error('COBS truncated segment');
      out.push(input[i++] as number);
    }

    if (code !== 0xff && i < input.length) {
      out.push(0);
    }
  }

  return Buffer.from(out);
}

export class GatewaySerialClient {
  private port: SerialPort | null = null;
  private callbacks = new Set<(msg: GatewayMessage) => void>();
  private rawFrame: number[] = [];
  private pending = new Map<number, PendingRequest>();
  private nextSeq = 1;

  async connect({ port, baudRate = 2000000 }: ConnectOptions): Promise<void> {
    if (this.port?.isOpen) return;

    this.port = new SerialPort({ path: port, baudRate, autoOpen: false });

    await new Promise<void>((resolve, reject) => {
      this.port?.open((err) => (err ? reject(err) : resolve()));
    });

    this.port.on('data', (chunk: Buffer) => this.onData(chunk));
    this.port.on('error', (err: Error) => this.emitError(err));
    this.port.on('close', () => this.rejectAllPending(new Error('Serial port closed')));
  }

  onMessage(cb: (msg: GatewayMessage) => void): () => void {
    this.callbacks.add(cb);
    return () => this.callbacks.delete(cb);
  }

  async close(): Promise<void> {
    if (!this.port) return;
    this.rejectAllPending(new Error('Client closing'));

    if (this.port.isOpen) {
      await new Promise<void>((resolve, reject) => {
        this.port?.close((err) => (err ? reject(err) : resolve()));
      });
    }

    this.port = null;
  }

  async setMode(mode: GatewayMode): Promise<void> {
    const modeByte = ModeByte[mode];
    if (modeByte === undefined) {
      throw new Error(`Invalid mode: ${mode}`);
    }

    const payload = Buffer.from([PROTO_VERSION, MsgType.CTRL_SET_MODE, modeByte]);
    await this.writeFrame(payload);
  }

  async getStatus(): Promise<void> {
    const payload = Buffer.from([PROTO_VERSION, MsgType.CTRL_GET_STATUS]);
    await this.writeFrame(payload);
  }

  sendCanFrame(parts: SendCanFrameParts, { timeoutMs = 1200 }: SendCanFrameOptions = {}): Promise<CtrlAckMessage> {
    const {
      canId,
      data = Buffer.alloc(0),
      extended = true,
      rtr = false,
      dir = Direction.PC_TO_CAN,
    } = parts;

    if (!Number.isInteger(canId)) {
      return Promise.reject(new Error('canId must be an integer'));
    }

    if ((canId >>> 0) > 0x1fffffff) {
      return Promise.reject(new Error('canId out of 29-bit range'));
    }

    const dataBuf = Buffer.isBuffer(data) ? data : Buffer.from(data);
    if (dataBuf.length > 8) {
      return Promise.reject(new Error('DLC > 8 is not allowed'));
    }

    const seq = this.allocSeq();
    const flags = (extended ? 0x01 : 0x00) | (rtr ? 0x02 : 0x00);

    const payload = Buffer.concat([
      Buffer.from([PROTO_VERSION, MsgType.CAN_FRAME, dir, flags]),
      packU32LE(canId >>> 0),
      Buffer.from([dataBuf.length]),
      packU16LE(seq),
      dataBuf,
    ]);

    return new Promise<CtrlAckMessage>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(seq);
        reject(new Error(`Timeout waiting ACK/ERR for seq=${seq}`));
      }, timeoutMs);

      this.pending.set(seq, {
        resolve: (msg) => {
          clearTimeout(timer);
          resolve(msg);
        },
        reject: (err) => {
          clearTimeout(timer);
          reject(err);
        },
      });

      void this.writeFrame(payload).catch((err: Error) => {
        const pending = this.pending.get(seq);
        if (pending) {
          this.pending.delete(seq);
          pending.reject(err);
        }
      });
    });
  }

  private async writeFrame(rawPayload: Buffer): Promise<void> {
    if (!this.port?.isOpen) {
      throw new Error('Serial port is not open');
    }

    const encoded = cobsEncode(rawPayload);
    const framed = Buffer.concat([encoded, Buffer.from([0x00])]);

    await new Promise<void>((resolve, reject) => {
      this.port?.write(framed, (err) => {
        if (err) return reject(err);
        this.port?.drain((drainErr) => (drainErr ? reject(drainErr) : resolve()));
      });
    });
  }

  private allocSeq(): number {
    const seq = this.nextSeq;
    this.nextSeq = (this.nextSeq + 1) & 0xffff;
    if (this.nextSeq === 0) this.nextSeq = 1;
    return seq;
  }

  private onData(chunk: Buffer): void {
    for (const byte of chunk) {
      if (byte === 0x00) {
        if (this.rawFrame.length > 0) {
          try {
            const decoded = cobsDecode(Buffer.from(this.rawFrame));
            const msg = this.decodeMessage(decoded);
            this.dispatch(msg);
          } catch (err) {
            this.emitError(err as Error);
          }
        }
        this.rawFrame = [];
      } else {
        this.rawFrame.push(byte);
        if (this.rawFrame.length > 512) {
          this.rawFrame = [];
          this.emitError(new Error('Serial frame too large, dropped'));
        }
      }
    }
  }

  private decodeMessage(buf: Buffer): GatewayMessage {
    if (buf.length < 2) throw new Error('Frame too short');
    if (buf[0] !== PROTO_VERSION) throw new Error(`Unsupported protocol version: ${buf[0]}`);

    const msgType = buf[1] as MsgType;
    const base: MessageBase = {
      raw: buf,
      version: buf[0],
      msgType,
      type: MSG_TYPE_NAME[msgType] ?? `UNKNOWN_${msgType}`,
      receivedAt: Date.now(),
    };

    if (msgType === MsgType.CAN_FRAME) {
      if (buf.length < 9) throw new Error('CAN_FRAME too short');

      const dir = buf[2] as Direction;
      const flags = buf[3] as number;
      const canId = buf.readUInt32LE(4) >>> 0;
      const dlc = buf[8] as number;
      if (dlc > 8) throw new Error('CAN_FRAME invalid DLC');

      let seq: number | null = null;
      let dataStart = 9;
      if (buf.length >= 11 + dlc) {
        seq = buf.readUInt16LE(9);
        dataStart = 11;
      }

      if (buf.length !== dataStart + dlc) throw new Error('CAN_FRAME length mismatch');
      const data = buf.subarray(dataStart, dataStart + dlc);

      const msg: CanFrameMessage = {
        ...base,
        type: 'CAN_FRAME',
        dir,
        dirName: DIRECTION_NAME[dir] ?? `UNKNOWN_${dir}`,
        flags: {
          extended: (flags & 0x01) !== 0,
          rtr: (flags & 0x02) !== 0,
        },
        canId,
        ...unpackCanIdFields(canId),
        dlc,
        data,
        seq,
      };
      return msg;
    }

    if (msgType === MsgType.STATUS) {
      if (buf.length !== 23) throw new Error('STATUS length mismatch');
      const msg: StatusMessage = {
        ...base,
        type: 'STATUS',
        mode: buf[2] === 0 ? 'strict' : 'monitor',
        statusCounters: {
          rx_can: buf.readUInt32LE(3),
          tx_can: buf.readUInt32LE(7),
          drop_filter: buf.readUInt32LE(11),
          rx_serial_err: buf.readUInt32LE(15),
          tx_can_err: buf.readUInt32LE(19),
        },
      };
      return msg;
    }

    if (msgType === MsgType.CTRL_ACK) {
      if (buf.length !== 4) throw new Error('CTRL_ACK length mismatch');
      const msg: CtrlAckMessage = {
        ...base,
        type: 'CTRL_ACK',
        seq: buf.readUInt16LE(2),
      };
      return msg;
    }

    if (msgType === MsgType.CTRL_ERR) {
      if (buf.length !== 5) throw new Error('CTRL_ERR length mismatch');
      const code = buf[4];
      const msg: CtrlErrMessage = {
        ...base,
        type: 'CTRL_ERR',
        seq: buf.readUInt16LE(2),
        errorCode: code,
        errorName: CTRL_ERR_NAME[code] ?? `UNKNOWN_ERR_${code}`,
      };
      return msg;
    }

    return base;
  }

  private dispatch(msg: GatewayMessage | any): void {
    if (msg.type === 'CTRL_ACK') {
      const pending = this.pending.get(msg.seq);
      if (pending) {
        this.pending.delete(msg.seq);
        pending.resolve(msg);
      }
    } else if (msg.type === 'CTRL_ERR') {
      const pending = this.pending.get(msg.seq);
      if (pending) {
        this.pending.delete(msg.seq);
        pending.reject(new Error(`ESP error: ${msg.errorName} (seq=${msg.seq})`));
      }
    }

    for (const cb of this.callbacks) {
      try {
        cb(msg);
      } catch (err) {
        this.emitError(err as Error);
      }
    }
  }

  private rejectAllPending(err: Error): void {
    for (const [seq, pending] of this.pending.entries()) {
      this.pending.delete(seq);
      pending.reject(new Error(`${err.message} (seq=${seq})`));
    }
  }

  private emitError(err: Error): void {
    console.error('[GatewaySerialClient] error:', err.message);
  }
}

export interface CreateClientOptions extends ConnectOptions {
  onMessage?: (msg: GatewayMessage) => void;
}

export async function createGatewayClient({
  port,
  baudRate = 2000000,
  onMessage,
}: CreateClientOptions): Promise<GatewaySerialClient> {
  const client = new GatewaySerialClient();
  await client.connect({ port, baudRate });

  if (onMessage) {
    client.onMessage(onMessage);
  }

  return client;
}

export async function sendTestMessage(
  client: GatewaySerialClient,
  {
    canId = (3 << 26) | (0x10 << 18) | (0xff << 10) | (0x0 << 6) | 0x00,
    data = Buffer.from([0xab, 0xcd]),
    extended = true,
    rtr = false,
    dir = Direction.PC_TO_CAN,
    timeoutMs = 1200,
    label = 'default-test-message',
  }: SendTestMessageOptions = {},
): Promise<CtrlAckMessage> {
  console.log(`[TEST] sending ${label}`);
  return client.sendCanFrame({ canId, data, extended, rtr, dir }, { timeoutMs });
}

async function runCli(): Promise<void> {
  const serialPath = process.argv[2];
  if (!serialPath) {
    console.error('Usage: npm run dev -- <serial-port>');
    process.exit(1);
  }

  const client = await createGatewayClient({
    port: serialPath,
    onMessage: (msg) => {
      console.log('[RX]', msg.raw)
    },
  });

  await client.getStatus();

  setInterval(() => {
    sendTestMessage(client)
  }, 2000)

  process.on('SIGINT', async () => {
    await client.close();
    process.exit(0);
  });
}

if (process.argv[1] && import.meta.url === new URL(`file://${process.argv[1]}`).href) {
  void runCli();
}
