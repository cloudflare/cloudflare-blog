import { MessageBatch, Queue } from "./types";

export interface Env {
  // A queue to store incoming request bodies
  requestQueue: Queue<string>;
}

export default {
  async fetch(request: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
    await env.requestQueue.send(await request.text());
    return new Response("OK");
  },

  async queue(batch: MessageBatch<string>, env: Env, ctx: ExecutionContext) {
    console.log(`Receieved ${batch.messages.length} messages from ${batch.queue}:`);
    for (const msg of batch.messages) {
      console.log(`${msg.timestamp} ${msg.id} ${msg.body.length} bytes: ${msg.body}`);
    }

    await slowRunningTask();
  },
};

async function slowRunningTask() {
  console.log(`Performing a slow running task with the batch...`);

  await new Promise((resolve) => setTimeout(resolve, 3000, []));
  if (Math.random() < 0.25) {
    throw new Error("Oh no, the task failed!");
  }
}
