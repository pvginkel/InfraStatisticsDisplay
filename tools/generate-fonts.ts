import { execFileSync } from 'child_process';
import * as fs from 'fs';
import * as path from 'path';

const fonts: [{ name: string; size: number | [number]; fonts: [{ file: string; range: string }] }] = JSON.parse(
  fs.readFileSync('generate-fonts.json', 'utf-8')
);

for (const font of fonts) {
  let sizes = Array.isArray(font.size) ? font.size : [font.size];

  for (const size of sizes) {
    let name = font.name.replace(/\{size\}/g, `${size}`);

    console.log();
    console.log(`Generating ${name}`);
    console.log();

    let args = [
      ...['--no-compress', '--no-prefilter', '--bpp', '4', '--size', `${size}`, '--format', 'lvgl'],
      ...['-o', path.join('..', 'main', `${name}.c`), '--force-fast-kern-format', '--lv-include', 'lvgl.h'],
    ];

    for (const file of font.fonts) {
      args.push('--font', file.file, '--range', file.range);
    }

    const lvFontConv = path.join('node_modules', 'lv_font_conv', 'lv_font_conv.js');
    console.log(execFileSync(process.execPath, [lvFontConv, ...args], { encoding: 'utf-8' }));
  }
}
