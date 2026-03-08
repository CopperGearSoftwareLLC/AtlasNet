const { asBuffer } = require('./format');

function toBufferArray(raw) {
  if (!Array.isArray(raw)) {
    return [];
  }
  return raw.map((item) => asBuffer(item));
}

function toHashPairsFromFlatArray(raw) {
  const flat = toBufferArray(raw);
  const pairs = [];
  for (let i = 0; i + 1 < flat.length; i += 2) {
    pairs.push([flat[i], flat[i + 1]]);
  }
  return pairs;
}

function objectEntriesToBufferPairs(raw) {
  if (!raw || typeof raw !== 'object') {
    return [];
  }

  return Object.entries(raw).map(([field, value]) => [
    asBuffer(field),
    asBuffer(value),
  ]);
}

async function readHashPairsRaw(client, key) {
  if (typeof client.callBuffer === 'function') {
    const raw = await client.callBuffer('HGETALL', key);
    if (Array.isArray(raw)) {
      return toHashPairsFromFlatArray(raw);
    }
  }

  if (typeof client.hgetallBuffer === 'function') {
    const raw = await client.hgetallBuffer(key);
    if (Array.isArray(raw)) {
      return toHashPairsFromFlatArray(raw);
    }
    return objectEntriesToBufferPairs(raw);
  }

  const fieldList =
    typeof client.hkeysBuffer === 'function'
      ? await client.hkeysBuffer(key)
      : await client.hkeys(key);
  const fields = toBufferArray(fieldList);
  if (fields.length === 0) {
    return [];
  }

  let valuesRaw = [];
  if (typeof client.callBuffer === 'function') {
    valuesRaw = await client.callBuffer('HMGET', key, ...fields);
  } else if (typeof client.hmgetBuffer === 'function') {
    valuesRaw = await client.hmgetBuffer(key, ...fields);
  } else {
    valuesRaw = await client.hmget(key, ...fields.map((field) => field.toString('binary')));
  }

  const values = toBufferArray(valuesRaw);
  const pairs = [];
  for (let i = 0; i < fields.length; i += 1) {
    pairs.push([fields[i], values[i] ?? null]);
  }
  return pairs;
}

async function readSetMembersRaw(client, key) {
  if (typeof client.callBuffer === 'function') {
    const raw = await client.callBuffer('SMEMBERS', key);
    if (Array.isArray(raw)) {
      return toBufferArray(raw);
    }
  }

  if (typeof client.smembersBuffer === 'function') {
    return toBufferArray(await client.smembersBuffer(key));
  }

  return toBufferArray(await client.smembers(key));
}

module.exports = {
  readHashPairsRaw,
  readSetMembersRaw,
};
