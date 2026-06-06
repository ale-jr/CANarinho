import { SerialPort } from 'serialport';
const PROTO_VERSION = 0x01;
var MsgType;
(function (MsgType) {
    MsgType[MsgType["CAN_FRAME"] = 1] = "CAN_FRAME";
    MsgType[MsgType["CTRL_SET_MODE"] = 2] = "CTRL_SET_MODE";
    MsgType[MsgType["CTRL_GET_STATUS"] = 3] = "CTRL_GET_STATUS";
    MsgType[MsgType["STATUS"] = 4] = "STATUS";
    MsgType[MsgType["CTRL_ACK"] = 5] = "CTRL_ACK";
    MsgType[MsgType["CTRL_ERR"] = 6] = "CTRL_ERR";
})(MsgType || (MsgType = {}));
var Direction;
(function (Direction) {
    Direction[Direction["CAN_TO_PC"] = 0] = "CAN_TO_PC";
    Direction[Direction["PC_TO_CAN"] = 1] = "PC_TO_CAN";
})(Direction || (Direction = {}));
var ModeByte;
(function (ModeByte) {
    ModeByte[ModeByte["strict"] = 0] = "strict";
    ModeByte[ModeByte["monitor"] = 1] = "monitor";
})(ModeByte || (ModeByte = {}));
var CtrlErrCode;
(function (CtrlErrCode) {
    CtrlErrCode[CtrlErrCode["ERR_BAD_FORMAT"] = 1] = "ERR_BAD_FORMAT";
    CtrlErrCode[CtrlErrCode["ERR_BAD_DIR"] = 2] = "ERR_BAD_DIR";
    CtrlErrCode[CtrlErrCode["ERR_BAD_DLC"] = 3] = "ERR_BAD_DLC";
    CtrlErrCode[CtrlErrCode["ERR_BAD_CAN_ID"] = 4] = "ERR_BAD_CAN_ID";
    CtrlErrCode[CtrlErrCode["ERR_TWAI_TX_FAIL"] = 5] = "ERR_TWAI_TX_FAIL";
    CtrlErrCode[CtrlErrCode["ERR_UNSUPPORTED"] = 6] = "ERR_UNSUPPORTED";
})(CtrlErrCode || (CtrlErrCode = {}));
const MSG_TYPE_NAME = {
    [MsgType.CAN_FRAME]: 'CAN_FRAME',
    [MsgType.CTRL_SET_MODE]: 'CTRL_SET_MODE',
    [MsgType.CTRL_GET_STATUS]: 'CTRL_GET_STATUS',
    [MsgType.STATUS]: 'STATUS',
    [MsgType.CTRL_ACK]: 'CTRL_ACK',
    [MsgType.CTRL_ERR]: 'CTRL_ERR',
};
const DIRECTION_NAME = {
    [Direction.CAN_TO_PC]: 'CAN_TO_PC',
    [Direction.PC_TO_CAN]: 'PC_TO_CAN',
};
const CTRL_ERR_NAME = {
    [CtrlErrCode.ERR_BAD_FORMAT]: 'ERR_BAD_FORMAT',
    [CtrlErrCode.ERR_BAD_DIR]: 'ERR_BAD_DIR',
    [CtrlErrCode.ERR_BAD_DLC]: 'ERR_BAD_DLC',
    [CtrlErrCode.ERR_BAD_CAN_ID]: 'ERR_BAD_CAN_ID',
    [CtrlErrCode.ERR_TWAI_TX_FAIL]: 'ERR_TWAI_TX_FAIL',
    [CtrlErrCode.ERR_UNSUPPORTED]: 'ERR_UNSUPPORTED',
};
function packU16LE(v) {
    const b = Buffer.alloc(2);
    b.writeUInt16LE(v & 0xffff, 0);
    return b;
}
function packU32LE(v) {
    const b = Buffer.alloc(4);
    b.writeUInt32LE(v >>> 0, 0);
    return b;
}
function unpackCanIdFields(canId) {
    return {
        priority: (canId >>> 26) & 0x7,
        src: (canId >>> 18) & 0xff,
        dst: (canId >>> 10) & 0xff,
        msgType: (canId >>> 6) & 0xf,
        channel: canId & 0x3f,
    };
}
function cobsEncode(input) {
    if (input.length === 0)
        return Buffer.from([0x01]);
    const out = [];
    let codeIndex = 0;
    out.push(0);
    let code = 1;
    for (let i = 0; i < input.length; i++) {
        const byte = input[i];
        if (byte === 0) {
            out[codeIndex] = code;
            codeIndex = out.length;
            out.push(0);
            code = 1;
        }
        else {
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
function cobsDecode(input) {
    if (input.length === 0)
        throw new Error('COBS empty frame');
    const out = [];
    let i = 0;
    while (i < input.length) {
        const code = input[i++];
        if (code === 0)
            throw new Error('COBS invalid code 0');
        for (let j = 1; j < code; j++) {
            if (i >= input.length)
                throw new Error('COBS truncated segment');
            out.push(input[i++]);
        }
        if (code !== 0xff && i < input.length) {
            out.push(0);
        }
    }
    return Buffer.from(out);
}
export class GatewaySerialClient {
    port = null;
    callbacks = new Set();
    rawFrame = [];
    pending = new Map();
    nextSeq = 1;
    async connect({ port, baudRate = 2000000 }) {
        if (this.port?.isOpen)
            return;
        this.port = new SerialPort({ path: port, baudRate, autoOpen: false });
        await new Promise((resolve, reject) => {
            this.port?.open((err) => (err ? reject(err) : resolve()));
        });
        this.port.on('data', (chunk) => this.onData(chunk));
        this.port.on('error', (err) => this.emitError(err));
        this.port.on('close', () => this.rejectAllPending(new Error('Serial port closed')));
    }
    onMessage(cb) {
        this.callbacks.add(cb);
        return () => this.callbacks.delete(cb);
    }
    async close() {
        if (!this.port)
            return;
        this.rejectAllPending(new Error('Client closing'));
        if (this.port.isOpen) {
            await new Promise((resolve, reject) => {
                this.port?.close((err) => (err ? reject(err) : resolve()));
            });
        }
        this.port = null;
    }
    async setMode(mode) {
        const modeByte = ModeByte[mode];
        if (modeByte === undefined) {
            throw new Error(`Invalid mode: ${mode}`);
        }
        const payload = Buffer.from([PROTO_VERSION, MsgType.CTRL_SET_MODE, modeByte]);
        await this.writeFrame(payload);
    }
    async getStatus() {
        const payload = Buffer.from([PROTO_VERSION, MsgType.CTRL_GET_STATUS]);
        await this.writeFrame(payload);
    }
    sendCanFrame(parts, { timeoutMs = 1200 } = {}) {
        const { canId, data = Buffer.alloc(0), extended = true, rtr = false, dir = Direction.PC_TO_CAN, } = parts;
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
        return new Promise((resolve, reject) => {
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
            void this.writeFrame(payload).catch((err) => {
                const pending = this.pending.get(seq);
                if (pending) {
                    this.pending.delete(seq);
                    pending.reject(err);
                }
            });
        });
    }
    async writeFrame(rawPayload) {
        if (!this.port?.isOpen) {
            throw new Error('Serial port is not open');
        }
        const encoded = cobsEncode(rawPayload);
        const framed = Buffer.concat([encoded, Buffer.from([0x00])]);
        await new Promise((resolve, reject) => {
            this.port?.write(framed, (err) => {
                if (err)
                    return reject(err);
                this.port?.drain((drainErr) => (drainErr ? reject(drainErr) : resolve()));
            });
        });
    }
    allocSeq() {
        const seq = this.nextSeq;
        this.nextSeq = (this.nextSeq + 1) & 0xffff;
        if (this.nextSeq === 0)
            this.nextSeq = 1;
        return seq;
    }
    onData(chunk) {
        for (const byte of chunk) {
            if (byte === 0x00) {
                if (this.rawFrame.length > 0) {
                    try {
                        const decoded = cobsDecode(Buffer.from(this.rawFrame));
                        const msg = this.decodeMessage(decoded);
                        this.dispatch(msg);
                    }
                    catch (err) {
                        this.emitError(err);
                    }
                }
                this.rawFrame = [];
            }
            else {
                this.rawFrame.push(byte);
                if (this.rawFrame.length > 512) {
                    this.rawFrame = [];
                    this.emitError(new Error('Serial frame too large, dropped'));
                }
            }
        }
    }
    decodeMessage(buf) {
        if (buf.length < 2)
            throw new Error('Frame too short');
        if (buf[0] !== PROTO_VERSION)
            throw new Error(`Unsupported protocol version: ${buf[0]}`);
        const msgType = buf[1];
        const base = {
            raw: buf,
            version: buf[0],
            msgType,
            type: MSG_TYPE_NAME[msgType] ?? `UNKNOWN_${msgType}`,
            receivedAt: Date.now(),
        };
        if (msgType === MsgType.CAN_FRAME) {
            if (buf.length < 9)
                throw new Error('CAN_FRAME too short');
            const dir = buf[2];
            const flags = buf[3];
            const canId = buf.readUInt32LE(4) >>> 0;
            const dlc = buf[8];
            if (dlc > 8)
                throw new Error('CAN_FRAME invalid DLC');
            let seq = null;
            let dataStart = 9;
            if (buf.length >= 11 + dlc) {
                seq = buf.readUInt16LE(9);
                dataStart = 11;
            }
            if (buf.length !== dataStart + dlc)
                throw new Error('CAN_FRAME length mismatch');
            const data = buf.subarray(dataStart, dataStart + dlc);
            const msg = {
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
            if (buf.length !== 23)
                throw new Error('STATUS length mismatch');
            const msg = {
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
            if (buf.length !== 4)
                throw new Error('CTRL_ACK length mismatch');
            const msg = {
                ...base,
                type: 'CTRL_ACK',
                seq: buf.readUInt16LE(2),
            };
            return msg;
        }
        if (msgType === MsgType.CTRL_ERR) {
            if (buf.length !== 5)
                throw new Error('CTRL_ERR length mismatch');
            const code = buf[4];
            const msg = {
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
    dispatch(msg) {
        if (msg.type === 'CTRL_ACK') {
            const pending = this.pending.get(msg.seq);
            if (pending) {
                this.pending.delete(msg.seq);
                pending.resolve(msg);
            }
        }
        else if (msg.type === 'CTRL_ERR') {
            const pending = this.pending.get(msg.seq);
            if (pending) {
                this.pending.delete(msg.seq);
                pending.reject(new Error(`ESP error: ${msg.errorName} (seq=${msg.seq})`));
            }
        }
        for (const cb of this.callbacks) {
            try {
                cb(msg);
            }
            catch (err) {
                this.emitError(err);
            }
        }
    }
    rejectAllPending(err) {
        for (const [seq, pending] of this.pending.entries()) {
            this.pending.delete(seq);
            pending.reject(new Error(`${err.message} (seq=${seq})`));
        }
    }
    emitError(err) {
        console.error('[GatewaySerialClient] error:', err.message);
    }
}
export async function createGatewayClient({ port, baudRate = 2000000, onMessage, }) {
    const client = new GatewaySerialClient();
    await client.connect({ port, baudRate });
    if (onMessage) {
        client.onMessage(onMessage);
    }
    return client;
}
export async function sendTestMessage(client, { canId = (3 << 26) | (0x10 << 18) | (0xff << 10) | (0x0 << 6) | 0x00, data = Buffer.from([0xab, 0xcd]), extended = true, rtr = false, dir = Direction.PC_TO_CAN, timeoutMs = 1200, label = 'default-test-message', } = {}) {
    console.log(`[TEST] sending ${label}`);
    return client.sendCanFrame({ canId, data, extended, rtr, dir }, { timeoutMs });
}
async function runCli() {
    const serialPath = process.argv[2];
    if (!serialPath) {
        console.error('Usage: npm run dev -- <serial-port>');
        process.exit(1);
    }
    const client = await createGatewayClient({
        port: serialPath,
        onMessage: (msg) => {
            console.log('[RX]', msg.raw);
        },
    });
    await client.getStatus();
    setInterval(() => {
        sendTestMessage(client);
    }, 2000);
    process.on('SIGINT', async () => {
        await client.close();
        process.exit(0);
    });
}
if (process.argv[1] && import.meta.url === new URL(`file://${process.argv[1]}`).href) {
    void runCli();
}
