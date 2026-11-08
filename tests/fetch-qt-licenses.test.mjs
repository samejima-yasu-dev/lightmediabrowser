import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fetchQtLicenses } from '../tools/fetch-qt-licenses.mjs';

test('fetches the Qt 6.8.3 LGPL and GPL license texts', async () => {
    const outputDirectory = await mkdtemp(path.join(os.tmpdir(), 'qt-licenses-'));
    const requests = [];
    try {
        await fetchQtLicenses(outputDirectory, async (url) => {
            requests.push(url);
            return new Response(`license text for ${path.basename(new URL(url).pathname)}`);
        });

        assert.deepEqual(requests, [
            'https://raw.githubusercontent.com/qt/qtbase/v6.8.3/LICENSES/LGPL-3.0-only.txt',
            'https://raw.githubusercontent.com/qt/qtbase/v6.8.3/LICENSES/GPL-3.0-only.txt',
        ]);
        assert.equal(
            await readFile(path.join(outputDirectory, 'LGPL-3.0-only.txt'), 'utf8'),
            'license text for LGPL-3.0-only.txt',
        );
        assert.equal(
            await readFile(path.join(outputDirectory, 'GPL-3.0-only.txt'), 'utf8'),
            'license text for GPL-3.0-only.txt',
        );
    } finally {
        await rm(outputDirectory, { recursive: true, force: true });
    }
});

test('fails when an upstream license request is unsuccessful', async () => {
    const outputDirectory = await mkdtemp(path.join(os.tmpdir(), 'qt-licenses-'));
    try {
        await assert.rejects(
            fetchQtLicenses(outputDirectory, async () => new Response('', { status: 404 })),
            /Failed to download Qt license.*404/,
        );
    } finally {
        await rm(outputDirectory, { recursive: true, force: true });
    }
});
