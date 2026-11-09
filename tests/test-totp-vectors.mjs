import { createHmac } from 'node:crypto';
import assert from 'node:assert/strict';

function totp(secret, epoch, algorithm) {
  let counter = BigInt(Math.floor(epoch / 30));
  const message = Buffer.alloc(8);
  message.writeBigUInt64BE(counter);
  const digest = createHmac(algorithm, secret).update(message).digest();
  const offset = digest.at(-1) & 15;
  const binary = digest.readUInt32BE(offset) & 0x7fffffff;
  return String(binary % 100000000).padStart(8, '0');
}

function dynamicCode(secret, counter, algorithm = 'sha1') {
  const message = Buffer.alloc(8);
  message.writeBigUInt64BE(BigInt(counter));
  const digest = createHmac(algorithm, secret).update(message).digest();
  const offset = digest.at(-1) & 15;
  return digest.readUInt32BE(offset) & 0x7fffffff;
}

function steam(secret, epoch) {
  const alphabet = '23456789BCDFGHJKMNPQRTVWXY';
  let value = dynamicCode(secret, Math.floor(epoch / 30));
  let code = '';
  for (let i = 0; i < 5; i += 1) {
    code += alphabet[value % alphabet.length];
    value = Math.floor(value / alphabet.length);
  }
  return code;
}

assert.equal(totp('12345678901234567890', 59, 'sha1'), '94287082');
assert.equal(totp('12345678901234567890123456789012', 59, 'sha256'), '46119246');
assert.equal(totp('1234567890123456789012345678901234567890123456789012345678901234', 59, 'sha512'), '90693936');
assert.equal(String(dynamicCode('12345678901234567890', 0) % 1000000).padStart(6, '0'), '755224');
assert.equal(steam('12345678901234567890', 59), 'PV9M4');
console.log('RFC 4226/6238 and Steam vectors: OK');
