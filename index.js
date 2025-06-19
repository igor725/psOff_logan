import { readFileSync } from 'fs';
import bind from 'bindings';

const logBuffer = new readFileSync("./build/Release/20250126-173715.000.p7d", null).buffer;

const logan = bind('./Release/psOff_logan');

console.log(logan.meman(logBuffer));
