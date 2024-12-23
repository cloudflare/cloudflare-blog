import puppeteer from "@cloudflare/puppeteer";
import * as fastpng from "fast-png";

const CACHE_TTL_SECONDS = 60;

interface Env {
    BROWSER: Fetcher;
    KV: KVNamespace;
}

function convertToGrey(raw_rgba: Buffer,
                       width: number,
                       height: number
                      ): Buffer {
    const bw = Buffer.alloc(width * height);

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const off = (y * width + x) * 4;
            // Weights derived from the ITU-R BT.601 standard
            const luminance = Math.round(
                0.299 * raw_rgba[off + 0] +
                    0.587 * raw_rgba[off + 1] +
                    0.114 * raw_rgba[off + 2]);
            bw[y * width + x] = luminance;
        }
    }
    return bw;
}

function ditherTwoBits(px: Buffer,
                       width: number,
                       height: number
                      ): Buffer {
    px = new Float32Array(px);

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const old_pixel = px[y * width + x];
            const new_pixel = old_pixel > 128 ? 0xff : 0x00;

            const quant_error = (old_pixel - new_pixel) / 16.0;
            px[(y + 0) * width + (x + 0)] = new_pixel;
            px[(y + 0) * width + (x + 1)] += quant_error * 7.;
            px[(y + 1) * width + (x - 1)] += quant_error * 3.;
            px[(y + 1) * width + (x + 0)] += quant_error * 5.;
            px[(y + 1) * width + (x + 1)] += quant_error * 1.;
        }
    }

    return Buffer.from(Uint8ClampedArray.from(px));
}

function encodeBinary(raw: Buffer, width: number, height: number): Buffer {
    const out = Buffer.alloc(raw.length / 8);

    let s = 0;
    for (let x = width - 1; x >= 0; x--) {
        for (let y = 0; y < height; y++) {
            out[Math.floor(s / 8)] |= (raw[y * width + x] ? (1 << (7 - (s % 8))) : 0);
            s += 1;
        }
    }
    return out;
}

async function gen_md5(data: Buffer): Promise<string>   {
    const digest = await crypto.subtle.digest(
        {name: 'SHA-1',},
        data,
    );

    return  [...new Uint8Array(digest)]
        .map(b => b.toString(16).padStart(2, '0'))
        .join('')
}

export default {
    async fetch(request, env): Promise<Response> {
        const { searchParams } = new URL(request.url);

        // FIXME: Do you really want to allow rendering of arbitrary websites?
        let url = searchParams.get("url");
        if (!url) {
            url = "http://1.1.1.1/"
        }

        url = new URL(url).toString();

        let img: Buffer;
        img = await env.KV.get(url, { type: "arrayBuffer" });
        if (img === null) {
            let sessionId = await getRandomBrowserSession(env.BROWSER);
            let browser;
            if (sessionId) {
                try {
                    browser = await puppeteer.connect(env.BROWSER, sessionId);
                } catch (e) {
                    // another worker may have connected first
                    console.log(`Failed to connect to ${sessionId}. Error ${e}`);
                }
            }
            if (!browser) {
                // No open sessions, launch new session
                browser = await puppeteer.launch(env.BROWSER, { keep_alive: 600000 });
            }
            // get current session id
            sessionId = browser.sessionId();

            const page = await browser.newPage();
            await page.setViewport({
                width: 480,
                height: 800,
                deviceScaleFactor: 1,
            })
            await page.goto(url);
            img = (await page.screenshot()) as Buffer;
            if (CACHE_TTL_SECONDS) {
                await env.KV.put(url, img, {
                    expirationTtl: CACHE_TTL_SECONDS,
                });
            }
            await browser.disconnect();
        }

        const md5 = await gen_md5(img);
        const inm = request.headers.get("If-None-Match");
        if (inm == md5) {
            return new Response(null, {status:304});
        }

        const raw_rgba = fastpng.decode(img);
        let bw = convertToGrey(raw_rgba.data, 480, 800);
        bw = ditherTwoBits(bw, 480, 800);

        let data, ct;
        if (searchParams.get("binary")) {
            data = encodeBinary(bw, 480, 800);
            ct = "application/octet-stream";
        } else {
            data = fastpng.encode({ width: 480, height: 800, data: bw, channels: 1 });
            ct = "image/png";
        };

        return new Response(data, {headers: {
            "content-type": ct,
            "ETag": md5,
        }});
    },
} satisfies ExportedHandler<Env>;

// Pick random free session
async function getRandomBrowserSession(endpoint: puppeteer.BrowserWorker): Promise<string> {
    const sessions: puppeteer.ActiveSession[] =
        await puppeteer.sessions(endpoint);
    console.log(`Sessions: ${JSON.stringify(sessions)}`);
    const sessionsIds = sessions
        .filter((v) => {
            return !v.connectionId; // remove sessions with workers connected to them
        })
        .map((v) => {
            return v.sessionId;
        });
    if (sessionsIds.length === 0) {
        return;
    }

    const sessionId =
        sessionsIds[Math.floor(Math.random() * sessionsIds.length)];

    console.log(`Session: ${JSON.stringify(sessionId)}`);
    return sessionId!;
}
