import { MessageBatch, Queue } from "./types";

export interface Environment {
  readonly QUEUE: Queue<string>;
}

export default {
  async fetch(request: Request, env: Environment): Promise<Response> {
    const body = await request.text();
    await env.QUEUE.send(body);
    return new Response(body + " ");
  },

  async queue(batch: MessageBatch<string>, env: Environment) {
    const messages = batch.messages.map((msg) => msg.body);
    console.log("Received a batch of", messages.length, "messages:", messages);

    await slowRunningTask(batch);
  },
};

async function slowRunningTask(batch: MessageBatch<string>) {
  await new Promise((resolve) => setTimeout(resolve, 3000, []));

  if (Math.random() < 0.25) {
    throw "Oh no, the task failed!";
  }
}
