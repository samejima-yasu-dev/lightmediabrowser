import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';

const siteDirectory = path.resolve('site');
const siteUrl = 'https://samejima-yasu-dev.github.io/lightmediabrowser';

for (const page of [
    { file: 'index.html', language: 'en', path: '/' },
    { file: 'ja/index.html', language: 'ja', path: '/ja/' },
]) {
    test(`${page.language} landing page has crawlable metadata and project links`, async () => {
        const html = await readFile(path.join(siteDirectory, page.file), 'utf8');

        assert.match(html, new RegExp(`<html lang="${page.language}">`));
        assert.ok(html.includes(`<link rel="canonical" href="${siteUrl}${page.path}">`));
        assert.ok(html.includes('<meta name="description"'));
        assert.ok(html.includes('<meta name="robots" content="index, follow">'));
        assert.ok(html.includes('<meta property="og:title"'));
        assert.ok(html.includes('hreflang="en"'));
        assert.ok(html.includes('hreflang="ja"'));
        assert.ok(html.includes('https://github.com/samejima-yasu-dev/lightmediabrowser/releases/latest'));
        assert.ok(html.includes('https://github.com/samejima-yasu-dev/lightmediabrowser'));
    });
}

test('robots.txt allows crawling and points to a sitemap containing both pages', async () => {
    const robots = await readFile(path.join(siteDirectory, 'robots.txt'), 'utf8');
    const sitemap = await readFile(path.join(siteDirectory, 'sitemap.xml'), 'utf8');

    assert.ok(robots.includes('User-agent: *'));
    assert.ok(robots.includes(`Sitemap: ${siteUrl}/sitemap.xml`));
    assert.ok(sitemap.includes(`${siteUrl}/`));
    assert.ok(sitemap.includes(`${siteUrl}/ja/`));
});
