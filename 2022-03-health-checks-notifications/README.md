# What is this repository about?

status-page is the code for a custom worker that can be used to create a Custom status-page using Cloudflare workers. It acceptes webhook notifications from Cloudflare Alert Notification service and creates a status-page which stores data in durable objects.

Change wrangler.toml to have your account that has access to Durable Objects.

# Steps to deploy to your account.

Install wrangler https://developers.cloudflare.com/workers/cli-wrangler/install-update if you don't have it installed already. Update wrangler to something greater than 1.19.3 as durable objects support does not work with previous versions.

```
$ wrangler publish --new-class Status
```

This will ensure the new class which ties to the durable object gets saved.
