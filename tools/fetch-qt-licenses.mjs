import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const licenseFiles = [
    'LGPL-3.0-only.txt',
    'GPL-3.0-only.txt',
];
const licenseBaseUrl = 'https://raw.githubusercontent.com/qt/qtbase/v6.8.3/LICENSES';

export async function fetchQtLicenses(outputDirectory, fetchImpl = fetch) {
    await mkdir(outputDirectory, { recursive: true });

    for (const licenseFile of licenseFiles) {
        const url = `${licenseBaseUrl}/${licenseFile}`;
        const response = await fetchImpl(url);
        if (!response.ok) {
            throw new Error(`Failed to download Qt license ${licenseFile}: HTTP ${response.status}`);
        }

        const content = await response.text();
        if (!content.trim()) {
            throw new Error(`Downloaded Qt license is empty: ${licenseFile}`);
        }
        await writeFile(path.join(outputDirectory, licenseFile), content);
        console.log(`Downloaded ${licenseFile}`);
    }
}

if (process.argv[1] && fileURLToPath(import.meta.url) === path.resolve(process.argv[1])) {
    const outputDirectory = process.argv[2];
    if (!outputDirectory) {
        throw new Error('Usage: node tools/fetch-qt-licenses.mjs <output-directory>');
    }
    await fetchQtLicenses(outputDirectory);
}
