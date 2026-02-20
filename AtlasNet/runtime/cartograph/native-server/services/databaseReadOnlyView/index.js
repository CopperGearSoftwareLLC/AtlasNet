const { decodeRedisDisplayValue } = require('./format');
const {
  decodeHardcodedHashEntry,
  decodeSetMemberForKey,
  hasHardcodedHashDecoder,
  shouldDecodeSetKey,
} = require('./hardcodedDecoders');
const { readHashPairsRaw, readSetMembersRaw } = require('./rawRedisReads');

function formatHashPayloadPairs(pairs) {
  return [...pairs]
    .sort(([a], [b]) => String(a).localeCompare(String(b)))
    .map(([k, v]) => `${k}\t${v}`)
    .join('\n');
}

async function readHardcodedHashPayload(client, key, decodeEnabled) {
  const rawPairs = await readHashPairsRaw(client, key);
  const pairs = rawPairs.map(([field, value]) =>
    decodeHardcodedHashEntry(key, field, value, decodeEnabled)
  );

  return {
    payload: formatHashPayloadPairs(pairs),
    entryCount: pairs.length,
  };
}

async function readHardcodedSetPayload(client, key, decodeEnabled) {
  const rawMembers = await readSetMembersRaw(client, key);
  const members = rawMembers.map((member) =>
    decodeSetMemberForKey(key, member, decodeEnabled)
  );
  members.sort((a, b) => String(a).localeCompare(String(b)));

  return {
    payload: members.join('\n'),
    entryCount: members.length,
  };
}

module.exports = {
  decodeRedisDisplayValue,
  formatHashPayloadPairs,
  hasHardcodedHashDecoder,
  readHardcodedHashPayload,
  readHardcodedSetPayload,
  shouldDecodeSetKey,
};
