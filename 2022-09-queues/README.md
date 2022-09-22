# Queues Blog Demo

This repo contains an early preview of Cloudflare Queues running in `wrangler dev` local mode.

In this example, the worker script accepts incoming HTTP requests via the `fetch()` handler, and places the request bodies onto a Queue.
The Queue batches messages (up to a configured time or size limit), and then invokes the script's `queue()` handler with the messages.
See `wrangler.toml` for the configuration.

In this example, the `queue()` handler simply logs the messages, and invokes a simulated `slowRunningTask()`.
The `slowRunningTask()` is hard-coded to fail 25% of the time, to illustrate the Queue's ability to redeliver messages upon failures.

## Getting Started

First install the dependencies via `npm`. This includes a custom version of a `wrangler` containing the Queues preview.

```bash
npm ci
```

## Running the local dev server

```bash
npx wrangler dev -l
```

## Sending requests

Send a single request to the server via curl:

```
curl localhost:8787 -d "hello"
```

Send requests with an incrementing counter once per second:
```
npm run simple_load
```