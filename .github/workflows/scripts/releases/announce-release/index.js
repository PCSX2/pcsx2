import { MessageEmbed, WebhookClient } from "discord.js";
import { Octokit } from "@octokit/rest";
import { throttling } from "@octokit/plugin-throttling";
import { retry } from "@octokit/plugin-retry";

let owner = process.env.OWNER;
let repo = process.env.REPO;

Octokit.plugin(throttling);
Octokit.plugin(retry);
const octokit = new Octokit({
  auth: process.env.GITHUB_TOKEN,
  userAgent: `${owner}/${repo}`,
  log: {
    debug: () => { },
    info: () => { },
    warn: console.warn,
    error: console.error
  },
  throttle: {
    onRateLimit: (retryAfter, options) => {
      octokit.log.warn(
        `Request quota exhausted for request ${options.method} ${options.url}`
      );

      // Retry twice after hitting a rate limit error, then give up
      if (options.request.retryCount <= 2) {
        console.log(`Retrying after ${retryAfter} seconds!`);
        return true;
      }
    },
    onAbuseLimit: (retryAfter, options) => {
      // does not retry, only logs a warning
      octokit.log.warn(
        `Abuse detected for request ${options.method} ${options.url}`
      );
    },
  }
});

if (process.env.TAG_VAL === undefined || process.env.TAG_VAL === "") {
  console.log(`Not announcing - TAG_VAL not defined`);
  process.exit(1);
}

const { data: releaseInfo } = await octokit.rest.repos.getReleaseByTag({
  owner: owner,
  repo: repo,
  tag: process.env.TAG_VAL,
});

if (releaseInfo === undefined) {
  console.log(`Not announcing - could not locate release with tag ${process.env.TAG_VAL}`);
  process.exit(1);
}

if (!releaseInfo.prerelease) {
  console.log("Not announcing - release was not a pre-release (aka a Nightly)");
  process.exit(0);
}

// Publish Webhook
const embed = new MessageEmbed()
  .setColor('#FF8000')
  .setTitle('New PCSX2 Nightly Build Available!')
  .setDescription("To download the latest or previous builds, [visit the official downloads page](https://pcsx2.net/downloads/).")
  .addFields(
    { name: 'Version', value: releaseInfo.tag_name, inline: true },
    { name: 'Installation Steps', value: '[See Here](https://github.com/PCSX2/pcsx2/wiki/Nightly-Build-Usage-Guide)', inline: true },
    { name: 'Included Changes', value: releaseInfo.body, inline: false }
  );
console.log(embed);

// Get all webhooks, simple comma-sep string
const webhookUrls = process.env.DISCORD_BUILD_WEBHOOK.split(",");

for (const url of webhookUrls) {
  const webhookClient = new WebhookClient({ url: url });
  await webhookClient.send({
    embeds: [embed],
  });
}
