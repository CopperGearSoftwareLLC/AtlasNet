class ByteCursor {
  constructor(buffer) {
    this.buffer = buffer;
    this.offset = 0;
  }

  remaining() {
    return this.buffer.length - this.offset;
  }

  readU8() {
    this.#ensure(1);
    const out = this.buffer.readUInt8(this.offset);
    this.offset += 1;
    return out;
  }

  readU16() {
    this.#ensure(2);
    const out = this.buffer.readUInt16BE(this.offset);
    this.offset += 2;
    return out;
  }

  readU32() {
    this.#ensure(4);
    const out = this.buffer.readUInt32BE(this.offset);
    this.offset += 4;
    return out;
  }

  readU64() {
    this.#ensure(8);
    const out = this.buffer.readBigUInt64BE(this.offset);
    this.offset += 8;
    return out;
  }

  readF32() {
    this.#ensure(4);
    const out = this.buffer.readFloatBE(this.offset);
    this.offset += 4;
    return out;
  }

  readBytes(length) {
    this.#ensure(length);
    const out = this.buffer.subarray(this.offset, this.offset + length);
    this.offset += length;
    return out;
  }

  readVarU32() {
    let value = 0;
    let shift = 0;
    for (;;) {
      const byte = this.readU8();
      value |= (byte & 0x7f) << shift;
      if ((byte & 0x80) === 0) {
        return value >>> 0;
      }
      shift += 7;
      if (shift > 35) {
        throw new Error('varint overflow');
      }
    }
  }

  readBlob() {
    const len = this.readVarU32();
    return this.readBytes(len);
  }

  #ensure(size) {
    if (this.offset + size > this.buffer.length) {
      throw new Error('buffer underflow');
    }
  }
}

module.exports = {
  ByteCursor,
};
