// Worker
export default {
  async fetch(request, env) {
    return await handleRequest(request, env);
  }
}

async function handleRequest(request, env) {
  let id = env.status.idFromName("A");
  let obj = env.status.get(id);
 
  return await obj.fetch(request);
}

// Durable Object
export class Status {
  constructor(state, env) {
    this.state = state;
  }

  async handleWebhook(request) {
    const json = await request.json();

    // Ignore webhook test notification upon creation
    if ((json.text || "").includes("Hello World!")) return;

    let healthCheckName = json.data?.name || "Unknown"
    let details = {
       status: json.data?.status || "Unknown",
       failureReason: json.data?.reason || "Unknown"
    }
    await this.state.storage.put(healthCheckName, details)
  }

  async statusHTML() {
    const statuses = await this.state.storage.list()
    let statHTML = ""
    for(let[hcName, details] of statuses) {
      const status = details.status || ""
      const failureReason = details.failureReason || ""
      let hc = `<p>HealthCheckName: ${hcName} </p>
                 <p>Status: ${status} </p>
                 <p>FailureReason: ${failureReason}</p>
                 <br/>`
      statHTML = statHTML + hc
    }
    return statHTML
  }

  async handleRoot() {
    // Default of healthy for before any notifications have been triggered
    const statuses = await this.statusHTML()

    return new Response(`
          <!DOCTYPE html>
          <head>
            <title>Status Page</title>
            <style>
              body {
                font-family: Courier New;
                padding-left: 10vw;
                padding-right: 10vw;
                padding-top: 5vh;
              }
            </style>
          </head>
          <body>
              <h1>Status of Production Servers</h1>
              <p>${statuses}</p>
          </body>
          `,
      {
        headers: {
          'Content-Type': "text/html"
        }
      })
  }

  // Handle HTTP requests from clients.
  async fetch(request) {
    const url = new URL(request.url)
    switch (url.pathname) {
      case "/webhook":
        await this.handleWebhook(request);
        return new Response()
      case "/":
        return await this.handleRoot();
      default:
        return new Response('Path not found', { status: 404 })
    }
  }
}

