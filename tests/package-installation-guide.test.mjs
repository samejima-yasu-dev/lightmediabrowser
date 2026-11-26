import assert from 'node:assert/strict';
import { access, readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

test('Windows and macOS package scripts use the tracked installation guide', async () => {
    const windowsScript = await readFile(path.join(repositoryRoot, 'tools/package-windows.ps1'), 'utf8');
    const macosScript = await readFile(path.join(repositoryRoot, 'tools/package-macos.sh'), 'utf8');

    assert.match(windowsScript, /Join-Path \$repositoryRoot 'INSTALLATION\.md'/);
    assert.match(macosScript, /"\$repository_root\/INSTALLATION\.md"/);
    await access(path.join(repositoryRoot, 'INSTALLATION.md'));
});
