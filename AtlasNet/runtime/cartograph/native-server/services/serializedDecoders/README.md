# Database Read-Only Decoder Package

This folder contains the hardcoded, read-only deserializers used by Cartograph's database viewer.

## Scope

- Decodes known Redis keys that store binary serialized values.
- Uses project serialization formats (`NetworkIdentity`, `UUID`, `IPAddress`, `AtlasEntity` snapshot).
- Produces text-only output for the existing database inspector.

## Removal

To remove this feature cleanly:

1. Delete this folder.
2. Remove the import/use sites in `native-server/services/databaseSnapshot.js`.
3. Remove the `decodeSerialized` query wiring in:
   - `native-server/native-server.js`
   - `src/app/api/databases/route.ts`
   - `src/app/database/page.tsx`

The rest of Cartograph DB viewing will continue working with standard payload rendering.
